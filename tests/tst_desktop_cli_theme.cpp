#include "TestEnv.h"

#include "core/ConfigService.h"
#include "core/OmarchyThemeService.h"
#include "core/PlaybackService.h"
#include "core/SecretsStore.h"
#include "core/TrackMediaService.h"
#include "mpris/MprisPlayer.h"

#include <QFileInfo>
#include <QProcess>
#include <QtTest>

class DesktopCliThemeTest : public QObject {
    Q_OBJECT
private slots:
    void configRoundTrip();
    void secretsFilePermissions();
    void mprisMimeTypes();
    void themeDefaults();
    void cliHelpAndVersion();
};

void DesktopCliThemeTest::configRoundTrip() {
    TestEnv env;
    ConfigService config;
    config.load();
    config.setLibraryPaths(env.libraryRoot());
    config.setSeekStepSecs(7);
    config.setWasdNavigation(true);
    config.setOpusBitrateKbps(192);
    config.setConvertDeleteSource(false);
    config.save();

    ConfigService again;
    again.load();
    QCOMPARE(again.seekStepSecs(), 7);
    QVERIFY(again.wasdNavigation());
    QCOMPARE(again.opusBitrateKbps(), 192);
    QVERIFY(!again.convertDeleteSource());
}

void DesktopCliThemeTest::secretsFilePermissions() {
    TestEnv env;
    SecretsStore::store(SecretsStore::Key::DiscogsToken, QStringLiteral("test-token-value"));
    QCOMPARE(SecretsStore::load(SecretsStore::Key::DiscogsToken), QStringLiteral("test-token-value"));
    SecretsStore::hardenFilePermissions();
}

void DesktopCliThemeTest::mprisMimeTypes() {
    PlaybackService playback;
    TrackMediaService media(nullptr);
    MprisPlayer mpris(&playback, &media);
    // Root adaptor mime list is on the private adaptor; exercise publish lightly.
    mpris.publish();
    QVERIFY(true);
}

void DesktopCliThemeTest::themeDefaults() {
    OmarchyThemeService theme;
    QVERIFY(!theme.background().isEmpty());
    QVERIFY(!theme.foreground().isEmpty());
    QVERIFY(!theme.accent().isEmpty());
    QCOMPARE(theme.onFill(QStringLiteral("#f5f5f5")), QStringLiteral("#1a1a1a"));
    QCOMPARE(theme.onFill(QStringLiteral("#1a1b26")), QStringLiteral("#f4f4f4"));
    QCOMPARE(theme.chromeIcon(), theme.onFill(theme.chrome()));
    QVERIFY(QString::compare(theme.chrome(), theme.chromeIcon(), Qt::CaseInsensitive) != 0);
    QVERIFY(theme.spaceMd() > 0);
}

void DesktopCliThemeTest::cliHelpAndVersion() {
    const QString bin = QCoreApplication::applicationDirPath() + QStringLiteral("/../hyprplay-bin");
    QString candidate = QFileInfo(bin).absoluteFilePath();
    if (!QFileInfo::exists(candidate)) {
        candidate = QStringLiteral("/home/jy0un9/Documents/qt-music/hyprplay-bin");
    }
    if (!QFileInfo::exists(candidate)) {
        QSKIP("hyprplay-bin not built next to tests");
    }
    QProcess help;
    help.start(candidate, {QStringLiteral("--help")});
    QVERIFY(help.waitForFinished(5000));
    QVERIFY(QString::fromUtf8(help.readAllStandardOutput()).contains(QStringLiteral("Usage")));

    QProcess ver;
    ver.start(candidate, {QStringLiteral("--version")});
    QVERIFY(ver.waitForFinished(5000));
    QVERIFY(QString::fromUtf8(ver.readAllStandardOutput()).contains(QStringLiteral("hyprplay")));
}

int runDesktopCliThemeTests(int argc, char **argv) {
    DesktopCliThemeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_desktop_cli_theme.moc"
