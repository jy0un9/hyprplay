#include "ImportService.h"

#include "AudioFormats.h"
#include "ConfigService.h"
#include "LibraryService.h"
#include "TagService.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutexLocker>
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

bool isImportableAudio(const QString &path) {
    return isSupportedAudioFile(path);
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

QString destTrackPath(const QString &musicDir, const QString &artist, const QString &album,
                      const QString &sourcePath) {
    const QFileInfo info(sourcePath);
    return QDir(musicDir).absoluteFilePath(artist + QLatin1Char('/') + album + QLatin1Char('/')
                                           + info.fileName());
}

bool copyAudioFile(const QString &from, const QString &to, QString *errorOut) {
    const auto setError = [errorOut](const QString &msg) {
        if (errorOut) {
            *errorOut = msg;
        }
    };
    QDir().mkpath(QFileInfo(to).absolutePath());
    if (QFile::exists(to)) {
        return true;
    }
    if (QFile::copy(from, to)) {
        return true;
    }
    setError(QStringLiteral("Failed to copy track into the library."));
    return false;
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

QVariantList discoverAlbums(const QString &inboxRoot, const QString &musicDir) {
    QHash<QString, QStringList> byDir;
    QDirIterator it(inboxRoot, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (!isImportableAudio(path)) {
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
        QStringList tracks = it.value();
        std::sort(tracks.begin(), tracks.end());

        const QString destDir =
            musicDir.isEmpty()
                ? QString()
                : QDir(musicDir).absoluteFilePath(artist + QLatin1Char('/') + album);

        QVariantMap row;
        row.insert(QStringLiteral("sourceDir"), it.key());
        row.insert(QStringLiteral("artist"), artist);
        row.insert(QStringLiteral("album"), album);
        row.insert(QStringLiteral("trackCount"), tracks.size());
        row.insert(QStringLiteral("tracks"), tracks);
        // Keep legacy key for any callers still reading "flacs".
        row.insert(QStringLiteral("flacs"), tracks);
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
                             QObject *parent)
    : QObject(parent),
      m_config(config),
      m_library(library),
      m_tags(tags) {}

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
                  ? QStringLiteral("No audio albums found in inbox")
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
    QMutexLocker lock(&m_decisionMutex);
    if (m_awaitingDecision) {
        m_decision = Decision::Abort;
        m_awaitingDecision = false;
        m_decisionCond.wakeAll();
    }
    QMetaObject::invokeMethod(
        this,
        [this]() {
            emit decisionChanged();
        },
        Qt::QueuedConnection);
}

void ImportService::resolveImportDecision(const QString &action) {
    Decision d = Decision::Abort;
    if (action == QLatin1String("retry")) {
        d = Decision::Retry;
    } else if (action == QLatin1String("skipTrack")) {
        d = Decision::SkipTrack;
    } else if (action == QLatin1String("skipAlbum")) {
        d = Decision::SkipAlbum;
    } else if (action == QLatin1String("abort")) {
        d = Decision::Abort;
    } else {
        return;
    }

    QMutexLocker lock(&m_decisionMutex);
    if (!m_awaitingDecision) {
        return;
    }
    m_decision = d;
    m_awaitingDecision = false;
    m_decisionCond.wakeAll();
    lock.unlock();
    emit decisionChanged();
}

void ImportService::clearDecisionState() {
    QMutexLocker lock(&m_decisionMutex);
    m_awaitingDecision = false;
    m_decision = Decision::None;
    m_decisionTitle.clear();
    m_decisionMessage.clear();
    m_decisionDetail.clear();
}

ImportService::Decision ImportService::waitForDecision(const QString &title, const QString &message,
                                                       const QString &detail) {
    {
        QMutexLocker lock(&m_decisionMutex);
        m_decision = Decision::None;
        m_awaitingDecision = true;
        m_decisionTitle = title;
        m_decisionMessage = message;
        m_decisionDetail = detail;
    }

    QMetaObject::invokeMethod(
        this,
        [this, title, message, detail]() {
            emit decisionChanged();
            emit decisionRequired(title, message, detail);
        },
        Qt::QueuedConnection);

    QMutexLocker lock(&m_decisionMutex);
    while (m_decision == Decision::None && !m_cancelRequested) {
        m_decisionCond.wait(&m_decisionMutex);
    }

    Decision result = m_cancelRequested ? Decision::Abort : m_decision;
    m_awaitingDecision = false;
    m_decision = Decision::None;
    lock.unlock();

    QMetaObject::invokeMethod(
        this,
        [this]() {
            emit decisionChanged();
        },
        Qt::QueuedConnection);

    return result;
}

void ImportService::startImport() {
    if (m_importing || m_albums.isEmpty() || !m_config || !m_library) {
        return;
    }

    const QString musicDir = musicLibraryRoot();
    if (musicDir.isEmpty()) {
        setStatus(QStringLiteral("Library path not configured"));
        return;
    }

    QVariantList selectedAlbums;
    int totalTracks = 0;
    for (const QVariant &item : m_albums) {
        const QVariantMap album = item.toMap();
        if (!album.value(QStringLiteral("selected"), true).toBool()) {
            continue;
        }
        selectedAlbums << album;
        const QStringList tracks = album.value(QStringLiteral("tracks")).toStringList();
        totalTracks += tracks.isEmpty() ? album.value(QStringLiteral("flacs")).toStringList().size()
                                        : tracks.size();
    }
    if (selectedAlbums.isEmpty()) {
        setStatus(QStringLiteral("No albums selected"));
        return;
    }

    m_cancelRequested = false;
    clearDecisionState();
    setImporting(true);
    setTrackProgress(0, totalTracks);
    setProgress(0);
    setStatus(QStringLiteral("Importing %1 album(s), %2 track(s)…")
                  .arg(selectedAlbums.size())
                  .arg(totalTracks));

    ImportService *self = this;

    (void)QtConcurrent::run([self, selectedAlbums, musicDir, totalTracks]() {
        int tracksDone = 0;
        bool success = true;
        bool abortAll = false;

        for (const QVariant &item : selectedAlbums) {
            if (abortAll || self->m_cancelRequested) {
                success = false;
                break;
            }

            const QVariantMap album = item.toMap();
            const QString sourceDir = album.value(QStringLiteral("sourceDir")).toString();
            const QString artist = album.value(QStringLiteral("artist")).toString();
            const QString albumName = album.value(QStringLiteral("album")).toString();
            QStringList tracks = album.value(QStringLiteral("tracks")).toStringList();
            if (tracks.isEmpty()) {
                tracks = album.value(QStringLiteral("flacs")).toStringList();
            }
            const QString albumLabel = artist + QStringLiteral(" — ") + albumName;

            QString destDir = album.value(QStringLiteral("destDir")).toString();
            if (destDir.isEmpty()) {
                destDir = QDir(musicDir).absoluteFilePath(artist + QLatin1Char('/') + albumName);
            }
            QDir().mkpath(destDir);

            bool skipAlbum = false;
            bool albumHadSuccessTrack = false;

            for (int ti = 0; ti < tracks.size(); ++ti) {
                if (abortAll || self->m_cancelRequested) {
                    success = false;
                    abortAll = true;
                    break;
                }

                const QString sourcePath = tracks.at(ti);
                const QString trackName = QFileInfo(sourcePath).fileName();
                const QString status = QStringLiteral("%1 · %2/%3 · %4")
                                          .arg(albumLabel)
                                          .arg(ti + 1)
                                          .arg(tracks.size())
                                          .arg(trackName);
                const int percent =
                    totalTracks > 0 ? qBound(0, (tracksDone * 100) / totalTracks, 99) : 0;
                QMetaObject::invokeMethod(
                    self,
                    [self, percent, status, tracksDone, totalTracks]() {
                        self->setProgress(percent);
                        self->setStatus(status);
                        self->setTrackProgress(tracksDone, totalTracks);
                    },
                    Qt::QueuedConnection);

                const QString destPath = destTrackPath(musicDir, artist, albumName, sourcePath);
                bool trackOk = QFile::exists(destPath);

                while (!trackOk) {
                    QString error;
                    if (copyAudioFile(sourcePath, destPath, &error)) {
                        trackOk = true;
                        break;
                    }

                    success = false;
                    const Decision decision = self->waitForDecision(
                        QStringLiteral("Import copy failed"),
                        error.isEmpty() ? QStringLiteral("Could not copy this track.") : error,
                        sourcePath);

                    if (decision == Decision::Retry) {
                        continue;
                    }
                    if (decision == Decision::SkipTrack) {
                        break;
                    }
                    if (decision == Decision::SkipAlbum) {
                        skipAlbum = true;
                        break;
                    }
                    abortAll = true;
                    break;
                }

                if (abortAll || skipAlbum) {
                    break;
                }

                if (trackOk && QFile::exists(destPath)) {
                    albumHadSuccessTrack = true;
                    if (self->m_tags) {
                        QVariantMap fields;
                        fields.insert(QStringLiteral("artist"), artist);
                        fields.insert(QStringLiteral("album"), albumName);
                        fields.insert(QStringLiteral("albumArtist"), artist);
                        self->m_tags->writeTagsToFile(destPath, fields);
                    }
                }

                ++tracksDone;
                const int donePercent =
                    totalTracks > 0 ? qBound(0, (tracksDone * 100) / totalTracks, 100) : 100;
                QMetaObject::invokeMethod(
                    self,
                    [self, donePercent, tracksDone, totalTracks]() {
                        self->setProgress(donePercent);
                        self->setTrackProgress(tracksDone, totalTracks);
                    },
                    Qt::QueuedConnection);
            }

            if (abortAll) {
                break;
            }
            if (skipAlbum) {
                continue;
            }

            if (albumHadSuccessTrack) {
                if (const QString cover = findCover(sourceDir); !cover.isEmpty()) {
                    const QString ext = QFileInfo(cover).suffix().toLower();
                    const QString destCover = destDir + QStringLiteral("/cover.") + ext;
                    if (!QFile::exists(destCover)) {
                        QFile::copy(cover, destCover);
                    }
                }
                copyLyrics(sourceDir, destDir, self->m_config);
            }
        }

        if (self->m_cancelRequested) {
            success = false;
        }

        QMetaObject::invokeMethod(
            self,
            [self, success, musicDir, tracksDone, totalTracks]() {
                self->clearDecisionState();
                self->setImporting(false);
                self->setTrackProgress(tracksDone, totalTracks);
                self->setProgress(100);
                self->setStatus(success ? QStringLiteral("Import complete")
                                        : (self->m_cancelRequested
                                               ? QStringLiteral("Import cancelled")
                                               : QStringLiteral("Import finished with issues")));
                emit self->decisionChanged();
                if (self->m_library) {
                    self->m_library->rescan({musicDir});
                }
                self->scanInbox();
                emit self->importFinished(success);
            },
            Qt::QueuedConnection);
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

void ImportService::setTrackProgress(int done, int total) {
    done = qMax(0, done);
    total = qMax(0, total);
    if (m_tracksDone == done && m_tracksTotal == total) {
        return;
    }
    m_tracksDone = done;
    m_tracksTotal = total;
    emit progressChanged();
}
