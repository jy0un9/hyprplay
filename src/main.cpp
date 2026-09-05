#include "core/AppController.h"
#include "core/OmarchyThemeService.h"
#include "core/ThemeIconProvider.h"
#include "mpris/MprisPlayer.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>

#include <clocale>
#include <cstdio>
#include <unistd.h>

namespace {

QString runtimeDir() {
    const QByteArray env = qgetenv("XDG_RUNTIME_DIR");
    if (!env.isEmpty()) {
        return QString::fromLocal8Bit(env);
    }
    return QStringLiteral("/run/user/%1").arg(getuid());
}

bool waylandSocketExists(const QString &runtime, const QString &display) {
    if (display.isEmpty()) {
        return false;
    }
    return QFile::exists(runtime + QLatin1Char('/') + display);
}

QString discoverWaylandDisplay(const QString &runtime) {
    QDir dir(runtime);
    if (!dir.exists()) {
        return {};
    }

    const QStringList sockets =
        dir.entryList({QStringLiteral("wayland-*")}, QDir::Files | QDir::System);
    for (const QString &name : sockets) {
        if (waylandSocketExists(runtime, name)) {
            return name;
        }
    }
    return {};
}

bool prepareDisplayPlatform() {
    if (!qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        return true;
    }

    QString runtime = runtimeDir();
    if (qEnvironmentVariableIsEmpty("XDG_RUNTIME_DIR") && QDir(runtime).exists()) {
        qputenv("XDG_RUNTIME_DIR", runtime.toLocal8Bit());
    }

    QString display = QString::fromLocal8Bit(qgetenv("WAYLAND_DISPLAY"));
    if (!waylandSocketExists(runtime, display)) {
        display = discoverWaylandDisplay(runtime);
        if (!display.isEmpty()) {
            qputenv("WAYLAND_DISPLAY", display.toLocal8Bit());
        }
    }

    if (waylandSocketExists(runtime, display)) {
        qputenv("QT_QPA_PLATFORM", "wayland");
        return true;
    }

    if (!qEnvironmentVariableIsEmpty("DISPLAY")) {
        return true;
    }

    std::fprintf(stderr,
                 "qt-music: no graphical session found.\n"
                 "  Run from a Hyprland/Omarchy terminal, or set WAYLAND_DISPLAY.\n"
                 "  Wayland socket dir checked: %s\n",
                 qPrintable(runtime));
    return false;
}

} // namespace

int main(int argc, char *argv[]) {
    setlocale(LC_NUMERIC, "C");

    if (!prepareDisplayPlatform()) {
        return 1;
    }

    QGuiApplication app(argc, argv);
    setlocale(LC_NUMERIC, "C");

    QGuiApplication::setApplicationName(QStringLiteral("qt-music"));
    QGuiApplication::setOrganizationName(QStringLiteral("qt-music"));
    QGuiApplication::setDesktopFileName(QStringLiteral("qt-music"));
    app.setWindowIcon(QIcon(QStringLiteral(":/qt-music.svg")));

    QQuickStyle::setStyle(QStringLiteral("Material"));
    setupSystemIconTheme();

    OmarchyThemeService theme;
    AppController controller;
    controller.initialize();

    MprisPlayer mpris(controller.playback(), controller.nowPlaying());
    mpris.publish();

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("themeicon"), new ThemeIconProvider());
    engine.addImportPath(QStringLiteral("qrc:/"));
    engine.rootContext()->setContextProperty(QStringLiteral("Theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("App"), &controller);
    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    QObject *window = engine.rootObjects().first();
    if (auto *quickWindow = qobject_cast<QQuickWindow *>(window)) {
        QSettings windowSettings;
        const int x = windowSettings.value(QStringLiteral("window/x"), -1).toInt();
        const int y = windowSettings.value(QStringLiteral("window/y"), -1).toInt();
        const int width = windowSettings.value(QStringLiteral("window/width"), 0).toInt();
        const int height = windowSettings.value(QStringLiteral("window/height"), 0).toInt();
        if (width >= 1024 && height >= 640) {
            quickWindow->setWidth(width);
            quickWindow->setHeight(height);
            if (x >= 0 && y >= 0) {
                quickWindow->setX(x);
                quickWindow->setY(y);
            }
        }
        QObject::connect(&app, &QGuiApplication::aboutToQuit, quickWindow, [quickWindow]() {
            QSettings settings;
            settings.setValue(QStringLiteral("window/x"), quickWindow->x());
            settings.setValue(QStringLiteral("window/y"), quickWindow->y());
            settings.setValue(QStringLiteral("window/width"), quickWindow->width());
            settings.setValue(QStringLiteral("window/height"), quickWindow->height());
        });
    }

    QObject::connect(&app, &QGuiApplication::aboutToQuit, &controller, &AppController::saveOnExit);

    return app.exec();
}
