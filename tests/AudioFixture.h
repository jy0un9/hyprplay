#pragma once

#include <QDir>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace AudioFixture {

inline bool ffmpegAvailable() {
    return QProcess::execute(QStringLiteral("ffmpeg"), {QStringLiteral("-version")}) == 0;
}

inline bool runFfmpeg(const QStringList &args) {
    QProcess proc;
    proc.start(QStringLiteral("ffmpeg"), args);
    if (!proc.waitForStarted(5000)) {
        return false;
    }
    if (!proc.waitForFinished(60000)) {
        proc.kill();
        return false;
    }
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

struct SampleSpec {
    QString fileName;
    QStringList codecArgs;
};

inline QList<SampleSpec> allFormatSpecs() {
    return {
        {QStringLiteral("track.flac"), {QStringLiteral("-c:a"), QStringLiteral("flac")}},
        {QStringLiteral("track.opus"),
         {QStringLiteral("-c:a"), QStringLiteral("libopus"), QStringLiteral("-b:a"),
          QStringLiteral("96k")}},
        {QStringLiteral("track.ogg"),
         {QStringLiteral("-c:a"), QStringLiteral("libvorbis"), QStringLiteral("-q:a"),
          QStringLiteral("4")}},
        {QStringLiteral("track.mp3"),
         {QStringLiteral("-c:a"), QStringLiteral("libmp3lame"), QStringLiteral("-b:a"),
          QStringLiteral("192k")}},
        {QStringLiteral("track.m4a"),
         {QStringLiteral("-c:a"), QStringLiteral("aac"), QStringLiteral("-b:a"),
          QStringLiteral("160k")}},
        {QStringLiteral("track.aac"),
         {QStringLiteral("-c:a"), QStringLiteral("aac"), QStringLiteral("-b:a"),
          QStringLiteral("128k"), QStringLiteral("-f"), QStringLiteral("adts")}},
    };
}

inline bool writeTaggedSample(const QString &path, const QStringList &codecArgs,
                              const QVariantMap &tags = {}) {
    const QString title = tags.value(QStringLiteral("title"), QStringLiteral("Format Probe")).toString();
    const QString artist =
        tags.value(QStringLiteral("artist"), QStringLiteral("Hyprplay Test")).toString();
    const QString album =
        tags.value(QStringLiteral("album"), QStringLiteral("Format Suite")).toString();
    const int track = tags.value(QStringLiteral("trackNumber"), 1).toInt();
    const int year = tags.value(QStringLiteral("year"), 2026).toInt();

    QStringList args = {QStringLiteral("-y"),
                        QStringLiteral("-f"),
                        QStringLiteral("lavfi"),
                        QStringLiteral("-i"),
                        QStringLiteral("sine=frequency=440:duration=1.5"),
                        QStringLiteral("-metadata"),
                        QStringLiteral("title=") + title,
                        QStringLiteral("-metadata"),
                        QStringLiteral("artist=") + artist,
                        QStringLiteral("-metadata"),
                        QStringLiteral("album=") + album,
                        QStringLiteral("-metadata"),
                        QStringLiteral("track=") + QString::number(track),
                        QStringLiteral("-metadata"),
                        QStringLiteral("date=") + QString::number(year)};
    args << codecArgs << path;
    return runFfmpeg(args);
}

inline QString createAlbumTree(const QString &libraryRoot, const QString &artist,
                               const QString &album) {
    const QString dir = libraryRoot + QLatin1Char('/') + artist + QLatin1Char('/') + album;
    QDir().mkpath(dir);
    return dir;
}

} // namespace AudioFixture
