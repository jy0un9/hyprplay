#include <functional>

#include "core/AudioFormats.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/flacfile.h>
#include <taglib/opusfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/mpegfile.h>
#include <taglib/mp4file.h>

class AudioFormatsTest : public QObject {
    Q_OBJECT

private slots:
    void extensions_recognizeSupportedFormats();
    void ffmpegSamples_taglibCanOpenAndTag();
};

void AudioFormatsTest::extensions_recognizeSupportedFormats() {
    QVERIFY(isSupportedAudioFile(QStringLiteral("/a/b/track.flac")));
    QVERIFY(isSupportedAudioFile(QStringLiteral("/a/b/track.opus")));
    QVERIFY(isSupportedAudioFile(QStringLiteral("/a/b/track.ogg")));
    QVERIFY(isSupportedAudioFile(QStringLiteral("/a/b/track.oga")));
    QVERIFY(isSupportedAudioFile(QStringLiteral("/a/b/track.mp3")));
    QVERIFY(isSupportedAudioFile(QStringLiteral("/a/b/track.m4a")));
    QVERIFY(isSupportedAudioFile(QStringLiteral("/a/b/track.aac")));
    QVERIFY(isSupportedAudioFile(QStringLiteral("/a/b/TRACK.FLAC")));
    QVERIFY(!isSupportedAudioFile(QStringLiteral("/a/b/track.wav")));
    QVERIFY(!isSupportedAudioFile(QStringLiteral("/a/b/track.txt")));
}

static bool runFfmpeg(const QStringList &args) {
    QProcess proc;
    proc.start(QStringLiteral("ffmpeg"), args);
    if (!proc.waitForStarted(5000)) {
        return false;
    }
    if (!proc.waitForFinished(30000)) {
        proc.kill();
        return false;
    }
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

static bool writeTaggedSample(const QString &path, const QStringList &codecArgs) {
    QStringList args = {QStringLiteral("-y"),
                        QStringLiteral("-f"),
                        QStringLiteral("lavfi"),
                        QStringLiteral("-i"),
                        QStringLiteral("sine=frequency=440:duration=1"),
                        QStringLiteral("-metadata"),
                        QStringLiteral("title=Format Probe"),
                        QStringLiteral("-metadata"),
                        QStringLiteral("artist=Hyprplay Test"),
                        QStringLiteral("-metadata"),
                        QStringLiteral("album=Format Suite")};
    args << codecArgs << path;
    return runFfmpeg(args);
}

void AudioFormatsTest::ffmpegSamples_taglibCanOpenAndTag() {
    if (QProcess::execute(QStringLiteral("ffmpeg"),
                          {QStringLiteral("-version")}) != 0) {
        QSKIP("ffmpeg not available");
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    struct Case {
        QString name;
        QStringList codecArgs;
        std::function<bool(const QString &)> openOk;
    };

    const QList<Case> cases = {
        {QStringLiteral("sample.flac"),
         {QStringLiteral("-c:a"), QStringLiteral("flac")},
         [](const QString &p) {
             TagLib::FLAC::File f(QFile::encodeName(p).constData());
             return f.isValid() && f.tag() && !f.tag()->title().isEmpty();
         }},
        {QStringLiteral("sample.opus"),
         {QStringLiteral("-c:a"), QStringLiteral("libopus"), QStringLiteral("-b:a"),
          QStringLiteral("96k")},
         [](const QString &p) {
             TagLib::Ogg::Opus::File f(QFile::encodeName(p).constData());
             return f.isValid() && f.tag() && !f.tag()->title().isEmpty();
         }},
        {QStringLiteral("sample.ogg"),
         {QStringLiteral("-c:a"), QStringLiteral("libvorbis"), QStringLiteral("-q:a"),
          QStringLiteral("4")},
         [](const QString &p) {
             TagLib::Ogg::Vorbis::File f(QFile::encodeName(p).constData());
             return f.isValid() && f.tag() && !f.tag()->title().isEmpty();
         }},
        {QStringLiteral("sample.mp3"),
         {QStringLiteral("-c:a"), QStringLiteral("libmp3lame"), QStringLiteral("-b:a"),
          QStringLiteral("192k")},
         [](const QString &p) {
             TagLib::MPEG::File f(QFile::encodeName(p).constData());
             return f.isValid() && f.tag() && !f.tag()->title().isEmpty();
         }},
        {QStringLiteral("sample.m4a"),
         {QStringLiteral("-c:a"), QStringLiteral("aac"), QStringLiteral("-b:a"),
          QStringLiteral("160k")},
         [](const QString &p) {
             TagLib::MP4::File f(QFile::encodeName(p).constData());
             return f.isValid() && f.tag() && !f.tag()->title().isEmpty();
         }},
        {QStringLiteral("sample.aac"),
         {QStringLiteral("-c:a"), QStringLiteral("aac"), QStringLiteral("-b:a"),
          QStringLiteral("128k"), QStringLiteral("-f"), QStringLiteral("adts")},
         [](const QString &p) {
             // ADTS AAC often has no container tags; FileRef should still open audio.
             TagLib::FileRef ref(QFile::encodeName(p).constData());
             return !ref.isNull() && ref.audioProperties();
         }},
    };

    for (const Case &c : cases) {
        const QString path = dir.filePath(c.name);
        QVERIFY2(isSupportedAudioFile(path), qPrintable(c.name));
        QVERIFY2(writeTaggedSample(path, c.codecArgs),
                 qPrintable(QStringLiteral("ffmpeg failed for %1").arg(c.name)));
        QVERIFY2(QFile::exists(path), qPrintable(c.name));
        QVERIFY2(c.openOk(path), qPrintable(QStringLiteral("TagLib open failed for %1").arg(c.name)));
    }
}

int runAudioFormatsTests(int argc, char **argv) {
    AudioFormatsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_audio_formats.moc"
