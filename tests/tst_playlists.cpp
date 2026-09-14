#include "AudioFixture.h"
#include "TestEnv.h"

#include "core/ConfigService.h"
#include "core/LibraryService.h"
#include "core/PlaylistService.h"

#include <QSignalSpy>
#include <QtTest>
#include <QUuid>

class PlaylistServiceTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void createAddReorderDedupAndRename();
};

void PlaylistServiceTest::initTestCase() {
    if (!AudioFixture::ffmpegAvailable()) {
        QSKIP("ffmpeg not available");
    }
}

void PlaylistServiceTest::createAddReorderDedupAndRename() {
    TestEnv env;
    const QString root = env.libraryRoot();
    const QString albumDir =
        AudioFixture::createAlbumTree(root, QStringLiteral("PL"), QStringLiteral("A"));
    QStringList paths;
    for (int i = 0; i < 3; ++i) {
        const QString path = albumDir + QStringLiteral("/t%1.flac").arg(i + 1);
        QVERIFY(AudioFixture::writeTaggedSample(
            path, {QStringLiteral("-c:a"), QStringLiteral("flac")},
            {{QStringLiteral("title"), QStringLiteral("T%1").arg(i + 1)},
             {QStringLiteral("artist"), QStringLiteral("PL")},
             {QStringLiteral("album"), QStringLiteral("A")},
             {QStringLiteral("trackNumber"), i + 1}}));
        paths << path;
    }

    ConfigService config;
    config.load();
    // Force playlists under our sandbox via library path defaulting.
    config.setLibraryPaths(root);
    config.save();
    config.load();

    LibraryService library;
    QSignalSpy scanDone(&library, &LibraryService::scanFinished);
    library.ensureLibrary({root});
    QVERIFY(scanDone.wait(60000));

    PlaylistService playlists(&config, &library);
    const QString plName =
        QStringLiteral("Favorites_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    QVERIFY2(playlists.createPlaylist(plName), qPrintable(playlists.status()));
    QVERIFY(playlists.playlistNames().contains(plName));

    QVariantList tracks;
    for (const QString &path : paths) {
        tracks << library.trackByPath(path);
    }
    QCOMPARE(playlists.addTracksToPlaylist(plName, tracks), 3);
    QCOMPARE(playlists.lastDuplicateSkipCount(), 0);
    QCOMPARE(playlists.addTracksToPlaylist(plName, tracks.mid(0, 1)), 0);
    QVERIFY(playlists.lastDuplicateSkipCount() >= 1);

    playlists.selectPlaylist(plName);
    QCOMPARE(playlists.tracksForSelectedPlaylist().size(), 3);
    QVERIFY(playlists.moveTrackInPlaylist(plName, 0, 2));
    QVERIFY(playlists.removeTrackFromPlaylist(plName, 0));
    QCOMPARE(playlists.tracksForSelectedPlaylist().size(), 2);

    const QString renamed = plName + QStringLiteral("_renamed");
    QVERIFY(playlists.renamePlaylist(plName, renamed));
    QVERIFY(playlists.playlistNames().contains(renamed));
    QVERIFY(playlists.deletePlaylist(renamed));
    QVERIFY(!playlists.playlistNames().contains(renamed));
}

int runPlaylistServiceTests(int argc, char **argv) {
    PlaylistServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_playlists.moc"
