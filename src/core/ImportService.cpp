#include "ImportService.h"

#include "BeetsService.h"
#include "ConfigService.h"
#include "LibraryService.h"
#include "TagService.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QProcess>
#include <QRegularExpression>
#include <QtConcurrent>
#include <algorithm>

namespace {

const QStringList kContainerFolders = {
    QStringLiteral("albums"), QStringLiteral("singles"), QStringLiteral("eps"),
    QStringLiteral("compilations"), QStringLiteral("ep"),  QStringLiteral("lp"),
};

const QStringList kCoverNames = {
    QStringLiteral("cover.jpg"),  QStringLiteral("cover.jpeg"),
    QStringLiteral("cover.png"),  QStringLiteral("folder.jpg"),
    QStringLiteral("folder.jpeg"), QStringLiteral("folder.png"),
    QStringLiteral("front.jpg"),  QStringLiteral("front.jpeg"),
    QStringLiteral("front.png"),
};

bool isFlac(const QString &path) {
    return path.toLower().endsWith(QStringLiteral(".flac"));
}

bool isContainerFolder(const QDir &dir) {
    return kContainerFolders.contains(dir.dirName().toLower());
}

QString folderName(const QDir &dir) {
    return dir.dirName().trimmed();
}

QString collapseSpaces(QString text) {
    text = text.simplified();
    return text.trimmed();
}

QString stripTrailingQualityTags(QString album) {
    static const QRegularExpression re(
        QStringLiteral("(\\s*[\\[\\(][^\\]\\)]*(?:FLAC|24B|44\\.1|48k|96k|192k)[^\\]\\)]*[\\]\\)]+$)"),
        QRegularExpression::CaseInsensitiveOption);
    while (true) {
        const QRegularExpressionMatch match = re.match(album);
        if (!match.hasMatch()) {
            break;
        }
        album = album.left(match.capturedStart()).trimmed();
    }
    return album;
}

QString stripArtistPrefix(QString album, const QString &artist) {
    const QString prefix = artist.trimmed() + QStringLiteral(" - ");
    if (album.startsWith(prefix, Qt::CaseInsensitive)) {
        return album.mid(prefix.length()).trimmed();
    }
    return album;
}

QString cleanAlbumTitle(const QString &raw, const QString &artist) {
    QString album = stripTrailingQualityTags(raw.trimmed());
    album = stripArtistPrefix(album, artist);
    album = stripTrailingQualityTags(album);
    return collapseSpaces(album);
}

bool folderArtistAlbum(const QString &albumDirPath, QString *artist, QString *album) {
    QDir albumDir(albumDirPath);
    const QString rawAlbum = folderName(albumDir);
    if (rawAlbum.isEmpty()) {
        return false;
    }

    QDir artistDir = albumDir;
    if (!artistDir.cdUp()) {
        return false;
    }
    if (isContainerFolder(artistDir)) {
        if (!artistDir.cdUp()) {
            return false;
        }
    }

    const QString artistName = folderName(artistDir);
    const QString albumName = cleanAlbumTitle(rawAlbum, artistName);
    if (artistName.isEmpty() || albumName.isEmpty()) {
        return false;
    }

    if (artist) {
        *artist = artistName;
    }
    if (album) {
        *album = albumName;
    }
    return true;
}

QString destOpusPath(const QString &musicDir, const QString &artist, const QString &album,
                     const QString &flacPath) {
    const QString stem = QFileInfo(flacPath).completeBaseName();
    return QDir(musicDir).absoluteFilePath(artist + QLatin1Char('/') + album + QLatin1Char('/')
                                           + stem + QStringLiteral(".opus"));
}

QString findCover(const QString &sourceDir) {
    for (const QString &name : kCoverNames) {
        const QString path = sourceDir + QLatin1Char('/') + name;
        if (QFile::exists(path)) {
            return path;
        }
    }
    return {};
}

void copyLyrics(const QString &sourceDir, const QString &destDir, ConfigService *config) {
    if (!config) {
        return;
    }
    const QString lyricsRoot = config->lyricsDir();
    if (lyricsRoot.isEmpty()) {
        return;
    }

    QDir().mkpath(destDir);
    const QStringList entries = QDir(sourceDir).entryList(QDir::Files);
    for (const QString &entry : entries) {
        const QString lower = entry.toLower();
        if (!lower.endsWith(QStringLiteral(".lrc")) && !lower.endsWith(QStringLiteral(".txt"))) {
            continue;
        }
        const QString src = sourceDir + QLatin1Char('/') + entry;
        QFile::copy(src, destDir + QLatin1Char('/') + entry);
        QDir(lyricsRoot).mkpath(QStringLiteral("."));
        QFile::copy(src, lyricsRoot + QLatin1Char('/') + entry);
    }
}

bool commandExists(const QString &program) {
    QProcess process;
    process.start(QStringLiteral("sh"),
                  {QStringLiteral("-lc"),
                   QStringLiteral("command -v %1").arg(program)});
    return process.waitForFinished(3000) && process.exitCode() == 0;
}

bool convertFlacToOpus(const QString &flacPath, const QString &opusPath) {
    QDir().mkpath(QFileInfo(opusPath).absolutePath());

    if (commandExists(QStringLiteral("opusenc"))) {
        QProcess process;
        process.start(QStringLiteral("opusenc"),
                      {QStringLiteral("--vbr"), QStringLiteral("--comp 10"), flacPath, opusPath});
        if (process.waitForFinished(600000)
            && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
            return true;
        }
    }

    if (!commandExists(QStringLiteral("ffmpeg"))) {
        return false;
    }

    QProcess ffmpeg;
    ffmpeg.start(QStringLiteral("ffmpeg"),
                 {QStringLiteral("-y"), QStringLiteral("-i"), flacPath,
                  QStringLiteral("-c:a"), QStringLiteral("libopus"), QStringLiteral("-b:a"),
                  QStringLiteral("256k"), opusPath});
    return ffmpeg.waitForFinished(600000) && ffmpeg.exitStatus() == QProcess::NormalExit
           && ffmpeg.exitCode() == 0 && QFile::exists(opusPath);
}

QVariantList discoverAlbums(const QString &inboxRoot, const QString &musicDir) {
    QHash<QString, QStringList> byDir;
    QDirIterator it(inboxRoot, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (!isFlac(path)) {
            continue;
        }
        const QString parent = QFileInfo(path).absolutePath();
        byDir[parent].append(path);
    }

    QVariantList albums;
    for (auto it = byDir.constBegin(); it != byDir.constEnd(); ++it) {
        QString artist;
        QString album;
        if (!folderArtistAlbum(it.key(), &artist, &album)) {
            continue;
        }
        QStringList flacs = it.value();
        std::sort(flacs.begin(), flacs.end());

        const QString destDir =
            musicDir.isEmpty()
                ? QString()
                : QDir(musicDir).absoluteFilePath(artist + QLatin1Char('/') + album);

        QVariantMap row;
        row.insert(QStringLiteral("sourceDir"), it.key());
        row.insert(QStringLiteral("artist"), artist);
        row.insert(QStringLiteral("album"), album);
        row.insert(QStringLiteral("trackCount"), flacs.size());
        row.insert(QStringLiteral("flacs"), flacs);
        row.insert(QStringLiteral("destDir"), destDir);
        row.insert(QStringLiteral("selected"), true);
        albums << row;
    }

    std::sort(albums.begin(), albums.end(), [](const QVariant &left, const QVariant &right) {
        const QVariantMap l = left.toMap();
        const QVariantMap r = right.toMap();
        return l.value(QStringLiteral("artist")).toString().toLower()
                   < r.value(QStringLiteral("artist")).toString().toLower()
               || (l.value(QStringLiteral("artist")).toString().compare(
                       r.value(QStringLiteral("artist")).toString(), Qt::CaseInsensitive)
                       == 0
                   && l.value(QStringLiteral("album")).toString().toLower()
                          < r.value(QStringLiteral("album")).toString().toLower());
    });
    return albums;
}

} // namespace

ImportService::ImportService(ConfigService *config, LibraryService *library, TagService *tags,
                             BeetsService *beets, QObject *parent)
    : QObject(parent),
      m_config(config),
      m_library(library),
      m_tags(tags),
      m_beets(beets) {}

QString ImportService::musicLibraryRoot() const {
    if (!m_config || m_config->libraryPaths().isEmpty()) {
        return {};
    }
    return m_config->expandPath(m_config->libraryPaths().constFirst());
}

QString ImportService::destinationRoot() const {
    return musicLibraryRoot();
}

int ImportService::selectedCount() const {
    int count = 0;
    for (const QVariant &item : m_albums) {
        if (item.toMap().value(QStringLiteral("selected"), true).toBool()) {
            ++count;
        }
    }
    return count;
}

void ImportService::scanInbox() {
    if (!m_config) {
        setStatus(QStringLiteral("Config unavailable"));
        return;
    }

    const QString inbox = m_config->importInboxPath();
    if (inbox.isEmpty() || !QDir(inbox).exists()) {
        m_albums.clear();
        emit albumsChanged();
        setStatus(QStringLiteral("Import inbox not configured or missing"));
        return;
    }

    m_albums = discoverAlbums(inbox, musicLibraryRoot());
    emit albumsChanged();
    setStatus(m_albums.isEmpty()
                  ? QStringLiteral("No FLAC albums found in inbox")
                  : QStringLiteral("Found %1 album(s) — review destinations, then import")
                        .arg(m_albums.size()));
}

void ImportService::setAlbumSelected(int index, bool selected) {
    if (index < 0 || index >= m_albums.size()) {
        return;
    }
    QVariantMap row = m_albums.at(index).toMap();
    if (row.value(QStringLiteral("selected"), true).toBool() == selected) {
        return;
    }
    row.insert(QStringLiteral("selected"), selected);
    m_albums[index] = row;
    emit albumsChanged();
}

void ImportService::setAllAlbumsSelected(bool selected) {
    bool changed = false;
    for (int i = 0; i < m_albums.size(); ++i) {
        QVariantMap row = m_albums.at(i).toMap();
        if (row.value(QStringLiteral("selected"), true).toBool() == selected) {
            continue;
        }
        row.insert(QStringLiteral("selected"), selected);
        m_albums[i] = row;
        changed = true;
    }
    if (changed) {
        emit albumsChanged();
    }
}

void ImportService::cancelImport() {
    m_cancelRequested = true;
}

void ImportService::startImport(bool runBeets) {
    if (m_importing || m_albums.isEmpty() || !m_config || !m_library) {
        return;
    }

    const QString musicDir = musicLibraryRoot();
    if (musicDir.isEmpty()) {
        setStatus(QStringLiteral("Library path not configured"));
        return;
    }

    QVariantList selectedAlbums;
    for (const QVariant &item : m_albums) {
        const QVariantMap album = item.toMap();
        if (!album.value(QStringLiteral("selected"), true).toBool()) {
            continue;
        }
        selectedAlbums << album;
    }
    if (selectedAlbums.isEmpty()) {
        setStatus(QStringLiteral("No albums selected"));
        return;
    }

    m_cancelRequested = false;
    setImporting(true);
    setProgress(0);
    setStatus(QStringLiteral("Importing %1 album(s)…").arg(selectedAlbums.size()));

    ImportService *self = this;

    (void)QtConcurrent::run([self, selectedAlbums, musicDir, runBeets]() {
        int done = 0;
        const int total = selectedAlbums.size();
        bool success = true;

        for (const QVariant &item : selectedAlbums) {
            if (self->m_cancelRequested) {
                success = false;
                break;
            }

            const QVariantMap album = item.toMap();
            const QString sourceDir = album.value(QStringLiteral("sourceDir")).toString();
            const QString artist = album.value(QStringLiteral("artist")).toString();
            const QString albumName = album.value(QStringLiteral("album")).toString();
            const QStringList flacs = album.value(QStringLiteral("flacs")).toStringList();

            QString destDir = album.value(QStringLiteral("destDir")).toString();
            if (destDir.isEmpty()) {
                destDir = QDir(musicDir).absoluteFilePath(artist + QLatin1Char('/') + albumName);
            }
            QDir().mkpath(destDir);

            for (const QString &flac : flacs) {
                const QString opusPath = destOpusPath(musicDir, artist, albumName, flac);
                if (!QFile::exists(opusPath)) {
                    if (!convertFlacToOpus(flac, opusPath)) {
                        success = false;
                        continue;
                    }
                }

                if (self->m_tags) {
                    QVariantMap fields;
                    fields.insert(QStringLiteral("artist"), artist);
                    fields.insert(QStringLiteral("album"), albumName);
                    fields.insert(QStringLiteral("albumArtist"), artist);
                    self->m_tags->writeTagsToFile(opusPath, fields);
                }
            }

            if (const QString cover = findCover(sourceDir); !cover.isEmpty()) {
                const QString ext = QFileInfo(cover).suffix().toLower();
                const QString destCover = destDir + QStringLiteral("/cover.") + ext;
                if (!QFile::exists(destCover)) {
                    QFile::copy(cover, destCover);
                }
            }
            copyLyrics(sourceDir, destDir, self->m_config);

            if (runBeets && self->m_beets && self->m_beets->available()) {
                self->m_beets->importAlbum(destDir, true);
            }

            ++done;
            const int percent = total > 0 ? (done * 100) / total : 100;
            const QString status = QStringLiteral("Imported %1 — %2 (%3/%4)")
                                       .arg(artist, albumName)
                                       .arg(done)
                                       .arg(total);
            QMetaObject::invokeMethod(self, [self, percent, status]() {
                self->setProgress(percent);
                self->setStatus(status);
            }, Qt::QueuedConnection);
        }

        QMetaObject::invokeMethod(self, [self, success, musicDir]() {
            self->setImporting(false);
            self->setProgress(100);
            self->setStatus(success ? QStringLiteral("Import complete")
                                    : QStringLiteral("Import finished with errors"));
            if (self->m_library) {
                self->m_library->rescan({musicDir});
            }
            self->scanInbox();
            emit self->importFinished(success);
        }, Qt::QueuedConnection);
    });
}

void ImportService::setImporting(bool importing) {
    if (m_importing == importing) {
        return;
    }
    m_importing = importing;
    emit importingChanged();
}

void ImportService::setStatus(const QString &status) {
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}

void ImportService::setProgress(int progress) {
    progress = qBound(0, progress, 100);
    if (m_progress == progress) {
        return;
    }
    m_progress = progress;
    emit progressChanged();
}
