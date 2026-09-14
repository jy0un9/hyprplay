#include "OpusConvert.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace {

bool commandExists(const QString &program) {
    return !QStandardPaths::findExecutable(program).isEmpty();
}

bool renameOrCopy(const QString &from, const QString &to) {
    QFile::remove(to);
    if (QFile::rename(from, to)) {
        return true;
    }
    if (QFile::copy(from, to)) {
        QFile::remove(from);
        return QFile::exists(to);
    }
    return false;
}

} // namespace

bool convertFlacToOpus(const QString &flacPath, const QString &opusPath, int bitrateKbps,
                       QString *errorOut) {
    const auto setError = [errorOut](const QString &msg) {
        if (errorOut) {
            *errorOut = msg;
        }
    };

    QDir().mkpath(QFileInfo(opusPath).absolutePath());
    const QString tempPath = QFileInfo(opusPath).absolutePath() + QLatin1Char('/')
                             + QFileInfo(opusPath).completeBaseName()
                             + QStringLiteral(".partial.opus");
    QFile::remove(tempPath);

    const bool haveOpusenc = commandExists(QStringLiteral("opusenc"));
    const bool haveFfmpeg = commandExists(QStringLiteral("ffmpeg"));
    if (!haveOpusenc && !haveFfmpeg) {
        setError(QStringLiteral(
            "Neither opusenc nor ffmpeg was found. Install opus-tools or ffmpeg, then Retry."));
        return false;
    }

    const int bitrate = qBound(48, bitrateKbps, 512);
    const QString bitrateArg = QString::number(bitrate);

    if (haveOpusenc) {
        QProcess process;
        process.start(QStringLiteral("opusenc"),
                      {QStringLiteral("--vbr"), QStringLiteral("--comp"), QStringLiteral("10"),
                       QStringLiteral("--bitrate"), bitrateArg, flacPath, tempPath});
        if (process.waitForFinished(600000) && process.exitStatus() == QProcess::NormalExit
            && process.exitCode() == 0 && QFile::exists(tempPath)) {
            if (renameOrCopy(tempPath, opusPath)) {
                return true;
            }
            QFile::remove(tempPath);
            setError(QStringLiteral("Encoded OK but failed to move Opus into the library."));
            return false;
        }
        QFile::remove(tempPath);
        if (!haveFfmpeg) {
            const QString err = QString::fromUtf8(process.readAllStandardError()).trimmed();
            setError(err.isEmpty() ? QStringLiteral("opusenc failed for this track.")
                                   : QStringLiteral("opusenc: ") + err);
            return false;
        }
    }

    QProcess ffmpeg;
    ffmpeg.start(QStringLiteral("ffmpeg"),
                 {QStringLiteral("-nostdin"), QStringLiteral("-y"), QStringLiteral("-i"), flacPath,
                  QStringLiteral("-c:a"), QStringLiteral("libopus"), QStringLiteral("-b:a"),
                  bitrateArg + QStringLiteral("k"), QStringLiteral("-map_metadata"),
                  QStringLiteral("0"), tempPath});
    if (ffmpeg.waitForFinished(600000) && ffmpeg.exitStatus() == QProcess::NormalExit
        && ffmpeg.exitCode() == 0 && QFile::exists(tempPath)) {
        if (renameOrCopy(tempPath, opusPath)) {
            return true;
        }
        QFile::remove(tempPath);
        setError(QStringLiteral("Encoded OK but failed to move Opus into the library."));
        return false;
    }

    QFile::remove(tempPath);
    const QString err = QString::fromUtf8(ffmpeg.readAllStandardError()).trimmed();
    setError(err.isEmpty() ? QStringLiteral("ffmpeg failed for this track.")
                           : QStringLiteral("ffmpeg: ") + err.right(400));
    return false;
}
