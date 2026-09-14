#include "AudioFixture.h"
#include "TestEnv.h"

#include "core/ConfigService.h"
#include "core/DiscogsService.h"
#include "core/LibraryService.h"
#include "core/LyricsService.h"
#include "core/MetadataSearchService.h"
#include "core/PlaybackService.h"
#include "core/TagService.h"
#include "core/TrackMediaService.h"

#include <QSignalSpy>
#include <QtTest>

// End-to-end matrix: for every supported format, build a real file, scan it,
// round-trip tags, load quality/art path, play via mpv (ao=null), then optionally
// hit MusicBrainz / Discogs / lyrics providers when HYPRPLAY_LIVE_NETWORK=1.
class FormatMatrixTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void allFormats_libraryTagsPlaybackMedia();
    void live_metadataAndLyricsProviders();
};

void FormatMatrixTest::initTestCase() {
    if (!AudioFixture::ffmpegAvailable()) {
        QSKIP("ffmpeg not available");
    }
    qputenv("HYPRPLAY_TEST_AO", QByteArrayLiteral("null"));
}

void FormatMatrixTest::allFormats_libraryTagsPlaybackMedia() {
    TestEnv env;
    const QString root = env.libraryRoot();
    const QString albumDir = AudioFixture::createAlbumTree(
        root, QStringLiteral("Matrix Artist"), QStringLiteral("Matrix Album"));

    QStringList created;
    int index = 1;
    for (const auto &spec : AudioFixture::allFormatSpecs()) {
        const QString path = albumDir + QLatin1Char('/') + spec.fileName;
        QVERIFY2(AudioFixture::writeTaggedSample(
                     path, spec.codecArgs,
                     {{QStringLiteral("title"), QStringLiteral("Track %1").arg(index)},
                      {QStringLiteral("artist"), QStringLiteral("Matrix Artist")},
                      {QStringLiteral("album"), QStringLiteral("Matrix Album")},
                      {QStringLiteral("trackNumber"), index}}),
                 qPrintable(QStringLiteral("create ") + spec.fileName));
        created << path;
        ++index;
    }

    ConfigService config;
    config.load();
    config.setLibraryPaths(root);
    config.setLyricsDir(env.lyricsDir());
    config.save();

    LibraryService library;
    QSignalSpy scanDone(&library, &LibraryService::scanFinished);
    library.ensureLibrary({root});
    QVERIFY(scanDone.wait(60000));
    QCOMPARE(library.trackCount(), created.size());
    QVERIFY(library.artists().contains(QStringLiteral("Matrix Artist")));

    TagService tags(&library);
    TrackMediaService media(&config);
    PlaybackService playback;

    for (const QString &path : created) {
        const QFileInfo info(path);
        const QString suffix = info.suffix().toLower();

        // Tag edit round-trip (skip fragile raw ADTS for asserted equality).
        const QString newTitle = QStringLiteral("Edited ") + info.completeBaseName();
        const bool wrote = tags.writeTagsToFile(
            path, {{QStringLiteral("title"), newTitle},
                   {QStringLiteral("artist"), QStringLiteral("Matrix Artist")},
                   {QStringLiteral("album"), QStringLiteral("Matrix Album")},
                   {QStringLiteral("genre"), QStringLiteral("Matrix")}});
        if (suffix != QLatin1String("aac")) {
            QVERIFY2(wrote, qPrintable(path));
            QCOMPARE(tags.loadTags(path).value(QStringLiteral("title")).toString(), newTitle);
            QVERIFY(library.reingestFile(path));
            QCOMPARE(library.trackByPath(path).value(QStringLiteral("title")).toString(), newTitle);
        }

        const QString quality = TrackMediaService::qualityLabelForPath(path);
        QVERIFY2(!quality.isEmpty(), qPrintable(path + QStringLiteral(" quality=") + quality));

        // Playback smoke (null AO).
        playback.playPath(path, newTitle, QStringLiteral("Matrix Artist"),
                          QStringLiteral("Matrix Album"));
        QTRY_VERIFY_WITH_TIMEOUT(playback.currentPath() == path, 8000);
        QTRY_VERIFY_WITH_TIMEOUT(playback.duration() > 0.5 || playback.playing(), 8000);
        playback.stop();
    }
}

void FormatMatrixTest::live_metadataAndLyricsProviders() {
    if (!qEnvironmentVariableIsSet("HYPRPLAY_LIVE_NETWORK")) {
        QSKIP("Set HYPRPLAY_LIVE_NETWORK=1 (and DISCOGS_TOKEN for Discogs) for live provider matrix");
    }

    TestEnv env;
    ConfigService config;
    config.load();
    config.setLyricsNeteaseEnabled(true);
    config.setLyricsPlainEnabled(true);
    config.setLyricsDir(env.lyricsDir());

    const QString path = env.libraryRoot() + QStringLiteral("/live-queen.flac");
    QVERIFY(AudioFixture::writeTaggedSample(
        path, {QStringLiteral("-c:a"), QStringLiteral("flac")},
        {{QStringLiteral("title"), QStringLiteral("Bohemian Rhapsody")},
         {QStringLiteral("artist"), QStringLiteral("Queen")},
         {QStringLiteral("album"), QStringLiteral("A Night at the Opera")}}));

    // MusicBrainz via MetadataSearchService
    DiscogsService discogs(&config);
    const QByteArray discogsToken = qgetenv("DISCOGS_TOKEN");
    if (!discogsToken.isEmpty()) {
        discogs.setToken(QString::fromUtf8(discogsToken));
    }
    MetadataSearchService meta(&config, &discogs);
    meta.setCurrentFields({{QStringLiteral("artist"), QStringLiteral("Queen")},
                           {QStringLiteral("album"), QStringLiteral("A Night at the Opera")},
                           {QStringLiteral("title"), QStringLiteral("Bohemian Rhapsody")}});
    QSignalSpy metaDone(&meta, &MetadataSearchService::searchFinished);
    meta.searchRelease(QStringLiteral("Queen"), QStringLiteral("A Night at the Opera"));
    QVERIFY(metaDone.wait(90000));
    QVERIFY2(meta.candidates().size() >= 1, "MusicBrainz/Discogs merge should return candidates");

    if (!discogsToken.isEmpty()) {
        QSignalSpy discogsDone(&discogs, &DiscogsService::releasesSearchFinished);
        discogs.searchReleases(QStringLiteral("Queen"), QStringLiteral("A Night at the Opera"));
        QVERIFY(discogsDone.wait(90000));
        QVERIFY(!discogsDone.takeFirst().at(0).toList().isEmpty());
    }

    // Lyrics: LRCLIB first; NetEase/OV as fallbacks in chain.
    LyricsService lyrics(&config);
    QSignalSpy lyricsDone(&lyrics, &LyricsService::trackFetched);
    lyrics.retryForTrack({{QStringLiteral("path"), path},
                          {QStringLiteral("title"), QStringLiteral("Bohemian Rhapsody")},
                          {QStringLiteral("artist"), QStringLiteral("Queen")},
                          {QStringLiteral("album"), QStringLiteral("A Night at the Opera")},
                          {QStringLiteral("durationMs"), 354000}});
    QVERIFY(lyricsDone.wait(120000));
    QVERIFY2(lyricsDone.takeFirst().at(1).toBool(), "Lyrics chain should succeed for Queen track");
    QVERIFY(LyricsService::sidecarExists(path));
}

int runFormatMatrixTests(int argc, char **argv) {
    FormatMatrixTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_format_matrix.moc"
