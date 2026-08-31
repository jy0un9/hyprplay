#include "core/AppController.h"
#include "core/OmarchyThemeService.h"
#include "core/ThemeIconProvider.h"
#include "mpris/MprisPlayer.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

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

    QQuickStyle::setStyle(QStringLiteral("Material"));
    setupSystemIconTheme();

    OmarchyThemeService theme;
    AppController controller;
    controller.initialize();

    MprisPlayer mpris(controller.playback());
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

    QObject::connect(&app, &QGuiApplication::aboutToQuit, &controller, &AppController::saveOnExit);

    return app.exec();
}
