#include "core/AppController.h"
#include "core/OmarchyThemeService.h"
#include "core/PlaybackService.h"
#include "core/ThemeIconProvider.h"
#include "mpris/MprisPlayer.h"

#include <QDir>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QUrl>
#include <QVersionNumber>

#include <clocale>
#include <cstdio>
#include <unistd.h>

namespace {

constexpr char kAppVersion[] = "0.1.1";
constexpr char kMprisService[] = "org.mpris.MediaPlayer2.hyprplay";
constexpr char kMprisPath[] = "/org/mpris/MediaPlayer2";

QStringList collectPlayPaths(int argc, char **argv) {
    QStringList paths;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg.startsWith(QLatin1Char('-'))) {
            continue;
        }
        QString path;
        if (arg.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)) {
            path = QUrl(arg).toLocalFile();
        } else {
            path = QFileInfo(arg).absoluteFilePath();
        }
        const QFileInfo info(path);
        if (info.exists() && info.isFile()) {
            paths << info.absoluteFilePath();
        } else {
            std::fprintf(stderr, "hyprplay: skipping missing file: %s\n", qPrintable(arg));
        }
    }
    return paths;
}

bool handleCliArgs(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--help") || arg == QStringLiteral("-h")) {
            std::fprintf(stdout,
                         "hyprplay %s — local music player\n"
                         "\n"
                         "Usage: hyprplay [options] [file…]\n"
                         "  -h, --help     show this help and exit\n"
                         "  -V, --version  show version and exit\n"
                         "\n"
                         "Formats: FLAC, Opus, Ogg Vorbis, MP3, M4A, AAC.\n"
                         "Audio files open in the running instance when one exists.\n",
                         kAppVersion);
            return true;
        }
        if (arg == QStringLiteral("--version") || arg == QStringLiteral("-V")) {
            std::fprintf(stdout, "hyprplay %s\n", kAppVersion);
            return true;
        }
    }
    return false;
}

bool handoffToExistingInstance(const QStringList &playPaths) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return false;
    }
    QDBusConnectionInterface *iface = bus.interface();
    if (!iface || !iface->isServiceRegistered(QString::fromLatin1(kMprisService))) {
        return false;
    }

    for (const QString &path : playPaths) {
        QDBusMessage openUri = QDBusMessage::createMethodCall(
            QString::fromLatin1(kMprisService), QString::fromLatin1(kMprisPath),
            QStringLiteral("org.mpris.MediaPlayer2"), QStringLiteral("OpenUri"));
        openUri << QUrl::fromLocalFile(path).toString();
        bus.call(openUri, QDBus::Block, 2000);
    }

    QDBusMessage raise = QDBusMessage::createMethodCall(QString::fromLatin1(kMprisService),
                                                        QString::fromLatin1(kMprisPath),
                                                        QStringLiteral("org.mpris.MediaPlayer2"),
                                                        QStringLiteral("Raise"));
    bus.call(raise, QDBus::Block, 2000);
    return true;
}

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
                 "hyprplay: no graphical session found.\n"
                 "  Run from a Hyprland/Omarchy terminal, or set WAYLAND_DISPLAY.\n"
                 "  Wayland socket dir checked: %s\n",
                 qPrintable(runtime));
    return false;
}

} // namespace

int main(int argc, char *argv[]) {
    setlocale(LC_NUMERIC, "C");

    if (handleCliArgs(argc, argv)) {
        return 0;
    }
    const QStringList playPaths = collectPlayPaths(argc, argv);

    if (!prepareDisplayPlatform()) {
        return 1;
    }

    QGuiApplication app(argc, argv);
    setlocale(LC_NUMERIC, "C");

    QGuiApplication::setApplicationName(QStringLiteral("hyprplay"));
    QGuiApplication::setOrganizationName(QString());
    QGuiApplication::setOrganizationDomain(QStringLiteral("jy0un9.org"));
    QGuiApplication::setDesktopFileName(QStringLiteral("hyprplay"));
    QGuiApplication::setApplicationVersion(QString::fromLatin1(kAppVersion));
    app.setWindowIcon(QIcon(QStringLiteral(":/hyprplay.svg")));

    if (handoffToExistingInstance(playPaths)) {
        std::fprintf(stderr, "hyprplay: already running, handed off to existing window\n");
        return 0;
    }

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
        QObject::connect(&mpris, &MprisPlayer::raiseRequested, quickWindow, [quickWindow]() {
            quickWindow->show();
            quickWindow->raise();
            quickWindow->requestActivate();
        });
    }

    if (!playPaths.isEmpty() && controller.playback()) {
        const QString path = playPaths.first();
        controller.playback()->playPath(path, QFileInfo(path).completeBaseName());
    }

    QObject::connect(&app, &QGuiApplication::aboutToQuit, &controller, &AppController::saveOnExit);

    return app.exec();
}
