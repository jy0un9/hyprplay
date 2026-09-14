#include "AudioFixture.h"
#include "TestEnv.h"

#include "core/ConfigService.h"
#include "core/PlaybackService.h"

#include <QFileInfo>
#include <QSignalSpy>
#include <QtTest>

class PlaybackTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void playPauseSeekAndQueue();
    void previousRestartsWhenPastThreshold();
    void shuffleAndRepeatCycle();
};

void PlaybackTest::initTestCase() {
    if (!AudioFixture::ffmpegAvailable()) {
        QSKIP("ffmpeg not available");
    }
    qputenv("HYPRPLAY_TEST_AO", QByteArrayLiteral("null"));
}

void PlaybackTest::playPauseSeekAndQueue() {
    TestEnv env;
    const QString dir = env.libraryRoot();
    QStringList paths;
    for (int i = 0; i < 3; ++i) {
        const QString path = dir + QStringLiteral("/t%1.flac").arg(i);
        QVERIFY(AudioFixture::writeTaggedSample(
            path, {QStringLiteral("-c:a"), QStringLiteral("flac")},
            {{QStringLiteral("title"), QStringLiteral("T%1").arg(i)}}));
        paths << path;
    }

    PlaybackService playback;
    QVariantList queue;
    for (const QString &path : paths) {
        queue << QVariantMap{{QStringLiteral("path"), path},
                            {QStringLiteral("title"), QFileInfo(path).completeBaseName()}};
    }
    playback.playTracks(queue, 0);
    QTRY_VERIFY_WITH_TIMEOUT(playback.playing() || playback.duration() > 0.0, 8000);
    QVERIFY(playback.currentPath().endsWith(QStringLiteral("t0.flac")));

    playback.pause();
    QTRY_VERIFY(!playback.playing());
    playback.play();
    QTRY_VERIFY(playback.playing());

    playback.seek(0.4);
    QTRY_VERIFY(playback.position() >= 0.2);

    playback.next();
    QTRY_VERIFY(playback.currentPath().endsWith(QStringLiteral("t1.flac")));
    playback.stop();
}

void PlaybackTest::previousRestartsWhenPastThreshold() {
    TestEnv env;
    const QString path = env.libraryRoot() + QStringLiteral("/long.flac");
    QVERIFY(AudioFixture::runFfmpeg(
        {QStringLiteral("-y"), QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"),
         QStringLiteral("sine=frequency=440:duration=6"), QStringLiteral("-c:a"),
         QStringLiteral("flac"), path}));

    PlaybackService playback;
    playback.playPath(path, QStringLiteral("long"));
    QTRY_VERIFY_WITH_TIMEOUT(playback.duration() > 4.0, 8000);
    playback.seek(4.0);
    QTRY_VERIFY(playback.position() > 3.0);
    playback.previous();
    QTRY_VERIFY(playback.position() < 1.0);
}

void PlaybackTest::shuffleAndRepeatCycle() {
    PlaybackService playback;
    QCOMPARE(playback.repeatMode(), static_cast<int>(RepeatMode::Off));
    playback.setRepeatMode(static_cast<int>(RepeatMode::Track));
    QCOMPARE(playback.repeatMode(), static_cast<int>(RepeatMode::Track));
    playback.setRepeatMode(static_cast<int>(RepeatMode::Queue));
    QCOMPARE(playback.repeatMode(), static_cast<int>(RepeatMode::Queue));
    playback.setRepeatMode(static_cast<int>(RepeatMode::Off));
    QCOMPARE(playback.repeatMode(), static_cast<int>(RepeatMode::Off));

    playback.setShuffle(true);
    QVERIFY(playback.shuffle());
    playback.setShuffle(false);
    QVERIFY(!playback.shuffle());
}

int runPlaybackTests(int argc, char **argv) {
    PlaybackTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_playback.moc"
