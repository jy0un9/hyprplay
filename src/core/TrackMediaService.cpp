#include "TrackMediaService.h"

#include "ConfigService.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>
#include <QUrl>
#include <QtConcurrent>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <vector>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/flacproperties.h>
#include <taglib/opusfile.h>

namespace {

QString artCacheDir() {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/artwork");
    QDir().mkpath(dir);
    return dir;
}

QString waveformCacheDir() {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/waveforms");
    QDir().mkpath(dir);
    return dir;
}

QString cacheKeyForPath(const QString &path) {
    return QString::fromUtf8(
        QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex());
}

float percentileSorted(const std::vector<float> &sorted, float p) {
    if (sorted.empty()) {
        return 0.0f;
    }
    const float clamped = std::clamp(p, 0.0f, 1.0f);
    const size_t idx =
        static_cast<size_t>(std::round((sorted.size() - 1) * clamped));
    return sorted[std::min(idx, sorted.size() - 1)];
}

QVector<quint8> normalizePeaks(const QVector<float> &rms) {
    if (rms.isEmpty()) {
        return {};
    }

    std::vector<float> db;
    db.reserve(rms.size());
    for (float peak : rms) {
        db.push_back(20.0f * std::log10(std::max(peak, 1e-8f)));
    }

    std::vector<float> sorted = db;
    std::sort(sorted.begin(), sorted.end());
    const float lo = percentileSorted(sorted, 0.06f);
    const float mid = percentileSorted(sorted, 0.50f);
    const float hi = percentileSorted(sorted, 0.96f);

    QVector<quint8> out(rms.size());
    for (int i = 0; i < rms.size(); ++i) {
        const float value = db[static_cast<size_t>(i)];
        float t = 0.0f;
        if (value <= mid) {
            const float span = std::max(mid - lo, 1e-3f);
            t = 0.36f * std::clamp((value - lo) / span, 0.0f, 1.0f);
        } else {
            const float span = std::max(hi - mid, 1e-3f);
            t = 0.36f + 0.64f * std::clamp((value - mid) / span, 0.0f, 1.0f);
        }
        out[i] = static_cast<quint8>(std::clamp(std::round(t * 255.0f), 0.0f, 255.0f));
    }
    return out;
}

QByteArray pictureBytes(const TagLib::FLAC::Picture *picture) {
    if (!picture) {
        return {};
    }
    const TagLib::ByteVector data = picture->data();
    return QByteArray(data.data(), static_cast<int>(data.size()));
}

QString saveArtBytes(const QByteArray &bytes, const QString &key, const QString &mime) {
    if (bytes.isEmpty()) {
        return {};
    }
    QString ext = QStringLiteral(".jpg");
    if (mime.contains(QStringLiteral("png"), Qt::CaseInsensitive)) {
        ext = QStringLiteral(".png");
    } else if (mime.contains(QStringLiteral("webp"), Qt::CaseInsensitive)) {
        ext = QStringLiteral(".webp");
    }
    const QString outPath = artCacheDir() + QLatin1Char('/') + key + ext;
    if (!QFile::exists(outPath)) {
        QFile file(outPath);
        if (!file.open(QIODevice::WriteOnly)) {
            return {};
        }
        file.write(bytes);
    }
    return QUrl::fromLocalFile(outPath).toString();
}

QString extractFlacArt(const QString &path, const QString &key) {
    TagLib::FLAC::File file(QFile::encodeName(path).constData());
    if (!file.isValid()) {
        return {};
    }
    const auto pictures = file.pictureList();
    for (const auto *picture : pictures) {
        const QByteArray bytes = pictureBytes(picture);
        const QString mime = QString::fromStdString(picture->mimeType().to8Bit(true));
        const QString url = saveArtBytes(bytes, key, mime);
        if (!url.isEmpty()) {
            return url;
        }
    }
    return {};
}

QString extractOpusArt(const QString &path, const QString &key) {
    TagLib::Ogg::Opus::File file(QFile::encodeName(path).constData());
    if (!file.isValid() || !file.tag()) {
        return {};
    }
    const auto pictures = file.tag()->pictureList();
    for (const auto *picture : pictures) {
        const QByteArray bytes = pictureBytes(picture);
        const QString mime = QString::fromStdString(picture->mimeType().to8Bit(true));
        const QString url = saveArtBytes(bytes, key, mime);
        if (!url.isEmpty()) {
            return url;
        }
    }
    return {};
}

bool parseSecondsComponent(const QString &secPart, double *outSeconds) {
    if (!outSeconds || secPart.isEmpty()) {
        return false;
    }

    const int dot = secPart.indexOf(QLatin1Char('.'));
    const int colon = secPart.indexOf(QLatin1Char(':'));
    const int sep = dot >= 0 ? dot : colon;

    bool ok = false;
    double seconds = 0.0;
    if (sep >= 0) {
        seconds = secPart.left(sep).trimmed().toDouble(&ok);
        if (!ok) {
            return false;
        }
        const QString frac = secPart.mid(sep + 1).trimmed();
        if (frac.isEmpty()) {
            return false;
        }
        if (dot >= 0) {
            bool okFrac = false;
            const int centiseconds = frac.toInt(&okFrac);
            if (!okFrac) {
                return false;
            }
            if (frac.length() == 1) {
                seconds += centiseconds / 10.0;
            } else if (frac.length() == 2) {
                seconds += centiseconds / 100.0;
            } else {
                seconds += centiseconds / std::pow(10.0, frac.length());
            }
        } else {
            bool okFrac = false;
            const double fracVal = frac.toDouble(&okFrac);
            if (!okFrac) {
                return false;
            }
            const int digits = frac.length();
            seconds += fracVal / std::pow(10.0, digits);
        }
    } else {
        seconds = secPart.trimmed().toDouble(&ok);
        if (!ok) {
            return false;
        }
    }

    *outSeconds = seconds;
    return true;
}

bool parseLrcTimestamp(const QString &stamp, double *outSecs) {
    if (!outSecs || stamp.isEmpty()) {
        return false;
    }

    const QStringList parts = stamp.split(QLatin1Char(':'));
    double hours = 0.0;
    double minutes = 0.0;
    QString secPart;

    if (parts.size() == 3) {
        bool okH = false;
        bool okM = false;
        hours = parts.at(0).trimmed().toDouble(&okH);
        minutes = parts.at(1).trimmed().toDouble(&okM);
        if (!okH || !okM) {
            return false;
        }
        secPart = parts.at(2).trimmed();
    } else if (parts.size() == 2) {
        bool okM = false;
        minutes = parts.at(0).trimmed().toDouble(&okM);
        if (!okM) {
            return false;
        }
        secPart = parts.at(1).trimmed();
    } else {
        return false;
    }

    double seconds = 0.0;
    if (!parseSecondsComponent(secPart, &seconds)) {
        return false;
    }

    *outSecs = hours * 3600.0 + minutes * 60.0 + seconds;
    return true;
}

bool parseLrcOffsetMs(const QString &line, double *outOffsetSecs) {
    if (!line.startsWith(QLatin1Char('[')) || !line.endsWith(QLatin1Char(']'))) {
        return false;
    }
    const QString inner = line.mid(1, line.size() - 2).trimmed();
    const int colon = inner.indexOf(QLatin1Char(':'));
    if (colon <= 0) {
        return false;
    }
    if (inner.left(colon).compare(QStringLiteral("offset"), Qt::CaseInsensitive) != 0) {
        return false;
    }
    bool ok = false;
    const double ms = inner.mid(colon + 1).trimmed().toDouble(&ok);
    if (!ok || !outOffsetSecs) {
        return false;
    }
    *outOffsetSecs = ms / 1000.0;
    return true;
}

QString stripBom(QString text) {
    if (text.startsWith(QChar(0xFEFF))) {
        text.remove(0, 1);
    }
    return text;
}

QString stripEnhancedLrcTags(const QString &text) {
    QString out;
    out.reserve(text.size());
    for (int i = 0; i < text.size(); ++i) {
        if (text.at(i) != QLatin1Char('<')) {
            out.append(text.at(i));
            continue;
        }
        const int close = text.indexOf(QLatin1Char('>'), i + 1);
        if (close <= i) {
            out.append(text.at(i));
            continue;
        }
        const QString inner = text.mid(i + 1, close - i - 1);
        double ts = 0.0;
        if (parseLrcTimestamp(inner, &ts)) {
            i = close;
            continue;
        }
        out.append(text.at(i));
    }
    return out.trimmed();
}

} // namespace

QString formatSampleRateKhz(int sampleRate) {
    if (sampleRate <= 0) {
        return {};
    }
    if (sampleRate % 1000 == 0) {
        return QString::number(sampleRate / 1000);
    }
    return QString::number(sampleRate / 1000.0, 'f', 1);
}

QString TrackMediaService::qualityLabelForPath(const QString &path) {
    const QString lower = path.toLower();
    if (lower.endsWith(QStringLiteral(".flac"))) {
        TagLib::FLAC::File file(QFile::encodeName(path).constData());
        if (file.isValid() && file.audioProperties()) {
            const auto *flacProps =
                dynamic_cast<const TagLib::FLAC::Properties *>(file.audioProperties());
            const int bits = flacProps ? flacProps->bitsPerSample() : 0;
            const QString rate =
                formatSampleRateKhz(static_cast<int>(file.audioProperties()->sampleRate()));
            if (bits > 0 && !rate.isEmpty()) {
                return QStringLiteral("FLAC %1-bit / %2 kHz").arg(bits).arg(rate);
            }
            if (!rate.isEmpty()) {
                return QStringLiteral("FLAC / %1 kHz").arg(rate);
            }
        }
        return QStringLiteral("FLAC");
    }

    if (lower.endsWith(QStringLiteral(".opus"))) {
        TagLib::Ogg::Opus::File file(QFile::encodeName(path).constData());
        if (file.isValid() && file.audioProperties()) {
            const TagLib::AudioProperties *props = file.audioProperties();
            const int bitrate = props->bitrate();
            const QString rate = formatSampleRateKhz(static_cast<int>(props->sampleRate()));
            if (bitrate > 0 && !rate.isEmpty()) {
                return QStringLiteral("Opus %1 kbps / %2 kHz").arg(bitrate).arg(rate);
            }
            if (!rate.isEmpty()) {
                return QStringLiteral("Opus / %1 kHz").arg(rate);
            }
        }
        return QStringLiteral("Opus");
    }

    const QString ext = QFileInfo(path).suffix();
    return ext.isEmpty() ? QString() : ext.toUpper();
}

TrackMediaService::TrackMediaService(ConfigService *config, QObject *parent)
    : QObject(parent), m_config(config) {}

void TrackMediaService::clearMedia() {
    m_albumArtUrl.clear();
    m_lyricsText.clear();
    m_lyricLines.clear();
    m_lyricTimes.clear();
    m_timedLines.clear();
    m_currentLyricIndex = -1;
    m_lastLyricSyncPos = -1.0;
    m_waveformPeaks.clear();
    m_waveformReady = false;
    m_qualityLabel.clear();
}

void TrackMediaService::loadForTrack(const QString &path, const QString &artist,
                                     const QString &album) {
    const QString absPath = QFileInfo(path).absoluteFilePath();
    if (absPath.isEmpty()) {
        clearMedia();
        emit mediaChanged();
        return;
    }

    if (absPath == m_currentPath && (m_waveformReady || m_loading)) {
        return;
    }

    m_currentPath = absPath;
    const quint64 generation = ++m_loadGeneration;
    m_lastLyricSyncPos = -1.0;

    m_loading = true;
    emit loadingChanged();

    clearMedia();
    m_qualityLabel = qualityLabelForPath(absPath);
    emit mediaChanged();
    emit currentLyricChanged();

    loadAlbumArt(absPath, album);
    loadLyrics(absPath, artist, album);
    emit mediaChanged();
    emit currentLyricChanged();

    (void)QtConcurrent::run([this, absPath, generation]() {
        const QVector<quint8> peaks = generatePeaksWithFfmpeg(absPath);
        QMetaObject::invokeMethod(this, [this, peaks, generation]() {
            if (generation != m_loadGeneration) {
                return;
            }
            m_waveformPeaks.clear();
            m_waveformPeaks.reserve(peaks.size());
            for (quint8 peak : peaks) {
                m_waveformPeaks << static_cast<double>(peak) / 255.0;
            }
            m_waveformReady = !peaks.isEmpty();
            m_loading = false;
            emit loadingChanged();
            emit mediaChanged();
        }, Qt::QueuedConnection);
    });
}

void TrackMediaService::loadAlbumArt(const QString &path, const QString &album) {
    const QString key = cacheKeyForPath(path);
    for (const QString &ext : {QStringLiteral(".jpg"), QStringLiteral(".png"),
                               QStringLiteral(".webp")}) {
        const QString cached = artCacheDir() + QLatin1Char('/') + key + ext;
        if (QFile::exists(cached)) {
            m_albumArtUrl = QUrl::fromLocalFile(cached).toString();
            return;
        }
    }

    const QString lower = path.toLower();
    if (lower.endsWith(QStringLiteral(".flac"))) {
        m_albumArtUrl = extractFlacArt(path, key);
    } else if (lower.endsWith(QStringLiteral(".opus"))) {
        m_albumArtUrl = extractOpusArt(path, key);
    }

    if (m_albumArtUrl.isEmpty()) {
        const QString folderArt = findFolderArt(path);
        if (!folderArt.isEmpty()) {
            m_albumArtUrl = QUrl::fromLocalFile(folderArt).toString();
        }
    }

    Q_UNUSED(album);
}

QString TrackMediaService::albumArtForTrack(const QString &path) {
    if (path.isEmpty()) {
        return {};
    }

    const QString resolvedPath = path.startsWith(QStringLiteral("file:"))
                                     ? QUrl(path).toLocalFile()
                                     : path;
    const QString absolutePath = QFileInfo(resolvedPath).absoluteFilePath();
    if (!QFileInfo::exists(absolutePath)) {
        return {};
    }
    const QString key = cacheKeyForPath(absolutePath);
    for (const QString &ext : {QStringLiteral(".jpg"), QStringLiteral(".png"),
                               QStringLiteral(".webp")}) {
        const QString cached = artCacheDir() + QLatin1Char('/') + key + ext;
        if (QFile::exists(cached)) {
            return QUrl::fromLocalFile(cached).toString();
        }
    }

    QString artUrl;
    const QString lower = absolutePath.toLower();
    if (lower.endsWith(QStringLiteral(".flac"))) {
        artUrl = extractFlacArt(absolutePath, key);
    } else if (lower.endsWith(QStringLiteral(".opus"))) {
        artUrl = extractOpusArt(absolutePath, key);
    }
    if (!artUrl.isEmpty()) {
        return artUrl;
    }

    const QString folderArt = findFolderArt(absolutePath);
    return folderArt.isEmpty() ? QString() : QUrl::fromLocalFile(folderArt).toString();
}

QString TrackMediaService::findFolderArt(const QString &trackPath) {
    const QDir dir = QFileInfo(trackPath).dir();
    for (const QString &name :
         {QStringLiteral("cover.jpg"), QStringLiteral("cover.png"), QStringLiteral("folder.jpg"),
          QStringLiteral("Folder.jpg"), QStringLiteral("front.jpg")}) {
        const QString candidate = dir.filePath(name);
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

void TrackMediaService::loadLyrics(const QString &path, const QString &artist,
                                   const QString &album) {
    const QString lyricsFile = findLyricsFile(path, artist, album);
    if (lyricsFile.isEmpty()) {
        return;
    }

    m_timedLines = parseLyricsFile(lyricsFile);
    m_lyricLines.clear();
    m_lyricTimes.clear();
    QStringList plainLines;
    QVector<TimedLine> timedOnly;
    timedOnly.reserve(m_timedLines.size());
    for (const TimedLine &line : m_timedLines) {
        if (line.time < 0.0) {
            continue;
        }
        timedOnly.append(line);
    }
    for (int i = 1; i < timedOnly.size(); ++i) {
        if (timedOnly[i].time <= timedOnly[i - 1].time) {
            timedOnly[i].time = timedOnly[i - 1].time + 0.001;
        }
    }
    if (timedOnly.isEmpty()) {
        // Plain (untimed) lyrics, e.g. a .txt fallback: show every line
        // statically. Timings stay empty so there is no karaoke highlight.
        m_lyricLines.clear();
        m_lyricTimes.clear();
        QStringList staticLines;
        for (const TimedLine &line : m_timedLines) {
            if (line.text.isEmpty()) {
                continue;
            }
            m_lyricLines.append(line.text);
            staticLines << line.text;
        }
        m_timedLines.clear();
        m_lyricsText = staticLines.join(QLatin1Char('\n'));
        m_currentLyricIndex = -1;
        m_lastLyricSyncPos = -1.0;
        return;
    }
    for (const TimedLine &line : timedOnly) {
        m_lyricLines.append(line.text);
        m_lyricTimes.append(line.time);
        plainLines << line.text;
    }
    m_timedLines = timedOnly;
    m_lyricsText = plainLines.join(QLatin1Char('\n'));
    m_currentLyricIndex = -1;
    m_lastLyricSyncPos = -1.0;
}

QString TrackMediaService::findLyricsFile(const QString &path, const QString &artist,
                                           const QString &album) const {
    const QFileInfo track(path);
    const QString base = track.absolutePath() + QLatin1Char('/') + track.completeBaseName();
    QStringList exts{QStringLiteral(".lrc")};
    // Plain (.txt) sidecars are only considered when plain-text lyrics are
    // enabled; synced-only users never see unsynced lyrics.
    if (!m_config || m_config->lyricsPlainEnabled()) {
        exts << QStringLiteral(".txt");
    }
    for (const QString &ext : exts) {
        const QString sidecar = base + ext;
        if (QFile::exists(sidecar)) {
            return sidecar;
        }
    }

    if (!m_config) {
        return {};
    }

    // Config may store lyrics_dir — read via expandPath if we add getter later.
    const QString configPath = m_config->configPath();
    Q_UNUSED(configPath);

    QString lyricsRoot = m_config->lyricsDir();
    if (!lyricsRoot.isEmpty()) {
        lyricsRoot = m_config->expandPath(lyricsRoot);
    }

    QStringList roots;
    if (!lyricsRoot.isEmpty()) {
        roots << lyricsRoot;
    }
    roots << QDir(QFileInfo(path).absolutePath()).absolutePath();

    for (const QString &root : roots) {
        if (!QDir(root).exists()) {
            continue;
        }
        for (const QString &ext : exts) {
            const QString flat = root + QLatin1Char('/') + track.completeBaseName() + ext;
            if (QFile::exists(flat)) {
                return flat;
            }
        }
        if (!artist.isEmpty() && !album.isEmpty()) {
            for (const QString &ext : exts) {
                const QString nested =
                    root + QLatin1Char('/') + artist + QLatin1Char('/') + album + QLatin1Char('/')
                    + track.completeBaseName() + ext;
                if (QFile::exists(nested)) {
                    return nested;
                }
            }
        }
    }
    return {};
}

QVector<TrackMediaService::TimedLine> TrackMediaService::parseLyricsFile(const QString &filePath) {
    QVector<TimedLine> lines;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return lines;
    }

    const bool isLrc = filePath.endsWith(QStringLiteral(".lrc"), Qt::CaseInsensitive);
    double offsetSecs = 0.0;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd()) {
        QString raw = stripBom(in.readLine().trimmed());
        if (raw.isEmpty()) {
            continue;
        }

        if (isLrc) {
            if (parseLrcOffsetMs(raw, &offsetSecs)) {
                continue;
            }

            QString remaining = raw;
            QVector<double> timestamps;
            while (remaining.startsWith(QLatin1Char('['))) {
                const int close = remaining.indexOf(QLatin1Char(']'));
                if (close <= 1) {
                    break;
                }
                const QString stamp = remaining.mid(1, close - 1);
                double ts = 0.0;
                if (parseLrcTimestamp(stamp, &ts)) {
                    timestamps.append(ts + offsetSecs);
                }
                remaining = remaining.mid(close + 1);
            }

            const QString text = stripEnhancedLrcTags(remaining.trimmed());
            if (!timestamps.isEmpty()) {
                for (double ts : timestamps) {
                    if (text.isEmpty()) {
                        continue;
                    }
                    TimedLine line;
                    line.time = qMax(0.0, ts);
                    line.text = text;
                    lines.append(line);
                }
                continue;
            }

            if (raw.startsWith(QLatin1Char('['))) {
                continue;
            }
        }

        TimedLine plain;
        plain.time = -1.0;
        plain.text = raw;
        if (!plain.text.isEmpty()) {
            lines.append(plain);
        }
    }

    if (isLrc) {
        std::stable_sort(lines.begin(), lines.end(),
                         [](const TimedLine &a, const TimedLine &b) { return a.time < b.time; });
    }
    return lines;
}

void TrackMediaService::updateLyricPosition(double positionSecs) {
    const int target = computeLyricIndex(positionSecs);

    if (target == m_currentLyricIndex) {
        return;
    }
    m_currentLyricIndex = target;
    emit currentLyricChanged();
}

int TrackMediaService::lyricIndexForPosition(double positionSecs) const {
    return computeLyricIndex(positionSecs);
}

int TrackMediaService::computeLyricIndex(double positionSecs) const {
    if (m_timedLines.isEmpty()) {
        return -1;
    }

    if (m_config) {
        positionSecs -= m_config->lyricsOffsetMs() / 1000.0;
    }

    if (positionSecs + 0.001 < m_timedLines.first().time) {
        return -1;
    }

    int low = 0;
    int high = m_timedLines.size() - 1;
    int index = -1;
    while (low <= high) {
        const int mid = low + (high - low) / 2;
        if (m_timedLines.at(mid).time <= positionSecs + 0.001) {
            index = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    return index;
}

QString TrackMediaService::peakCachePath(const QString &trackPath) {
    const QFileInfo info(trackPath);
    const QString key =
        cacheKeyForPath(trackPath + QString::number(info.lastModified().toSecsSinceEpoch())
                        + QString::number(info.size()));
    return waveformCacheDir() + QLatin1Char('/') + key + QStringLiteral(".peaks");
}

QVector<quint8> TrackMediaService::loadPeakCache(const QString &cacheFile) {
    QFile file(cacheFile);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray data = file.readAll();
    if (data.size() < 8) {
        return {};
    }
    if (!data.startsWith("QWMK2")) {
        return {};
    }
    const int count = qFromBigEndian<qint32>(data.constData() + 5);
    if (count <= 0 || 8 + count > data.size()) {
        return {};
    }
    QVector<quint8> peaks(count);
    for (int i = 0; i < count; ++i) {
        peaks[i] = static_cast<quint8>(data.at(8 + i));
    }
    return peaks;
}

bool TrackMediaService::savePeakCache(const QString &cacheFile, const QVector<quint8> &peaks) {
    QFile file(cacheFile);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    QByteArray header("QWMK2", 5);
    char countBytes[4];
    qToBigEndian(static_cast<qint32>(peaks.size()), countBytes);
    header.append(countBytes, 4);
    file.write(header);
    for (quint8 peak : peaks) {
        file.write(reinterpret_cast<const char *>(&peak), 1);
    }
    return true;
}

QVector<quint8> TrackMediaService::generatePeaksWithFfmpeg(const QString &path) {
    const QString cacheFile = peakCachePath(path);
    const QVector<quint8> cached = loadPeakCache(cacheFile);
    if (!cached.isEmpty()) {
        return cached;
    }

    QProcess ffmpeg;
    ffmpeg.start(QStringLiteral("ffmpeg"),
                 {QStringLiteral("-v"), QStringLiteral("quiet"), QStringLiteral("-i"), path,
                  QStringLiteral("-vn"), QStringLiteral("-ac"), QStringLiteral("1"),
                  QStringLiteral("-ar"), QStringLiteral("8000"), QStringLiteral("-f"),
                  QStringLiteral("s16le"), QStringLiteral("-")});
    if (!ffmpeg.waitForStarted(5000)) {
        return {};
    }
    if (!ffmpeg.waitForFinished(120000)) {
        ffmpeg.kill();
        return {};
    }
    if (ffmpeg.exitStatus() != QProcess::NormalExit || ffmpeg.exitCode() != 0) {
        return {};
    }

    const QByteArray pcm = ffmpeg.readAllStandardOutput();
    if (pcm.size() < 4) {
        return {};
    }

    constexpr int binCount = 1024;
    QVector<float> rmsWindows(binCount, 0.0f);
    const int sampleCount = pcm.size() / 2;
    const int samplesPerBin = qMax(1, sampleCount / binCount);

    for (int bin = 0; bin < binCount; ++bin) {
        double sumSq = 0.0;
        int count = 0;
        float peak = 0.0f;
        const int start = bin * samplesPerBin;
        const int end = qMin(sampleCount, start + samplesPerBin);
        for (int i = start; i < end; ++i) {
            const qint16 sample =
                qFromLittleEndian<qint16>(pcm.constData() + i * 2);
            const float norm = static_cast<float>(sample) / 32768.0f;
            sumSq += norm * norm;
            peak = std::max(peak, std::abs(norm));
            ++count;
        }
        if (count > 0) {
            const float rms = static_cast<float>(std::sqrt(sumSq / count));
            rmsWindows[bin] = std::max(rms, peak * 0.35f);
        }
    }

    const QVector<quint8> peaks = normalizePeaks(rmsWindows);

    savePeakCache(cacheFile, peaks);
    return peaks;
}
