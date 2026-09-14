#include "AudioFixture.h"
#include "TestEnv.h"

#include "core/ConfigService.h"
#include "core/TrackMediaService.h"

#include <QFile>
#include <QSignalSpy>
#include <QtTest>

class TrackMediaTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void qualityLabelsPerFormat();
    void loadsSidecarLyrics();
    void findsFolderArt();
};

void TrackMediaTest::initTestCase() {
    if (!AudioFixture::ffmpegAvailable()) {
        QSKIP("ffmpeg not available");
    }
}

void TrackMediaTest::qualityLabelsPerFormat() {
    TestEnv env;
    const QString dir = env.libraryRoot();
    for (const auto &spec : AudioFixture::allFormatSpecs()) {
        const QString path = dir + QLatin1Char('/') + spec.fileName;
        QVERIFY2(AudioFixture::writeTaggedSample(path, spec.codecArgs), qPrintable(spec.fileName));
        const QString label = TrackMediaService::qualityLabelForPath(path);
        QVERIFY2(!label.isEmpty(), qPrintable(spec.fileName + QStringLiteral(": ") + label));
        if (spec.fileName.endsWith(QStringLiteral(".flac"))) {
            QVERIFY(label.contains(QStringLiteral("FLAC")));
        } else if (spec.fileName.endsWith(QStringLiteral(".opus"))) {
            QVERIFY(label.contains(QStringLiteral("Opus")));
        } else if (spec.fileName.endsWith(QStringLiteral(".ogg"))) {
            QVERIFY(label.contains(QStringLiteral("Ogg")));
        } else if (spec.fileName.endsWith(QStringLiteral(".mp3"))) {
            QVERIFY(label.contains(QStringLiteral("MP3")));
        }
    }
}

void TrackMediaTest::loadsSidecarLyrics() {
    TestEnv env;
    ConfigService config;
    config.load();
    config.setLyricsDir(env.lyricsDir());

    const QString path = env.libraryRoot() + QStringLiteral("/lyric.flac");
    QVERIFY(AudioFixture::writeTaggedSample(path, {QStringLiteral("-c:a"), QStringLiteral("flac")}));
    const QString lrc = env.libraryRoot() + QStringLiteral("/lyric.lrc");
    QFile file(lrc);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("[00:00.00]Hello\n[00:01.00]World\n");
    file.close();

    TrackMediaService media(&config);
    media.loadForTrack(path, QStringLiteral("Hyprplay Test"), QStringLiteral("Format Suite"));
    QTRY_VERIFY_WITH_TIMEOUT(!media.lyricLines().isEmpty(), 3000);
    QCOMPARE(media.lyricLines().first().toString(), QStringLiteral("Hello"));
    QCOMPARE(media.lyricIndexForPosition(1.05), 1);
    // Wait for async waveform worker so we don't destroy media mid-run.
    QTRY_VERIFY_WITH_TIMEOUT(!media.loading(), 60000);
}

void TrackMediaTest::findsFolderArt() {
    TestEnv env;
    ConfigService config;
    const QString albumDir =
        AudioFixture::createAlbumTree(env.libraryRoot(), QStringLiteral("Art"), QStringLiteral("Cov"));
    const QString path = albumDir + QStringLiteral("/song.flac");
    QVERIFY(AudioFixture::writeTaggedSample(path, {QStringLiteral("-c:a"), QStringLiteral("flac")}));
    // Minimal JPEG (1x1)
    QByteArray jpeg = QByteArray::fromHex(
        "ffd8ffe000104a46494600010100000100010000ffdb004300080606070605080707"
        "070909080a0c140d0c0b0b0c1912130f141d1a1f1e1d1a1c1c20242e2720222c231c"
        "1c2837292c30313434341f27393d38323c2e333432ffdb0043010909090c0b0c180d"
        "0d1832211c2132323232323232323232323232323232323232323232323232323232"
        "323232323232323232323232323232323232323232ffc00011080001000103011100"
        "021101031101ffc40014000100000000000000000000000000000000ffc400141001"
        "00000000000000000000000000000000ffda000c0301000210031000003f00bf80ffd9");
    QFile cover(albumDir + QStringLiteral("/cover.jpg"));
    QVERIFY(cover.open(QIODevice::WriteOnly));
    cover.write(jpeg);
    cover.close();

    TrackMediaService media(&config);
    const QString art = media.albumArtForTrack(path);
    QVERIFY(art.contains(QStringLiteral("cover.jpg")) || art.startsWith(QStringLiteral("file:")));
}

int runTrackMediaTests(int argc, char **argv) {
    TrackMediaTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_track_media.moc"
