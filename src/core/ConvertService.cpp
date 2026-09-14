#include "ConvertService.h"

#include "ConfigService.h"
#include "LibraryService.h"
#include "OpusConvert.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMetaObject>
#include <QMutexLocker>
#include <QtConcurrent>
#include <algorithm>

namespace {

bool isFlacPath(const QString &path) {
    return path.toLower().endsWith(QStringLiteral(".flac"));
}

QVariantList discoverFlacAlbums(LibraryService *library) {
    QVariantList albums;
    if (!library) {
        return albums;
    }

    QHash<QString, QVariantMap> byKey;
    const QVariantList tracks = library->allTracks();
    for (const QVariant &item : tracks) {
        const QVariantMap track = item.toMap();
        const QString path = track.value(QStringLiteral("path")).toString();
        if (!isFlacPath(path) || !QFile::exists(path)) {
            continue;
        }
        const QString artist = track.value(QStringLiteral("artist")).toString().trimmed();
        const QString album = track.value(QStringLiteral("album")).toString().trimmed();
        if (artist.isEmpty() || album.isEmpty()) {
            continue;
        }
        const QString key = artist.toLower() + QLatin1Char('\n') + album.toLower();
        QVariantMap &row = byKey[key];
        if (!row.contains(QStringLiteral("artist"))) {
            row.insert(QStringLiteral("artist"), artist);
            row.insert(QStringLiteral("album"), album);
            row.insert(QStringLiteral("tracks"), QStringList());
            row.insert(QStringLiteral("selected"), true);
        }
        QStringList list = row.value(QStringLiteral("tracks")).toStringList();
        list.append(path);
        row.insert(QStringLiteral("tracks"), list);
    }

    for (auto it = byKey.begin(); it != byKey.end(); ++it) {
        QStringList list = it.value().value(QStringLiteral("tracks")).toStringList();
        std::sort(list.begin(), list.end());
        it.value().insert(QStringLiteral("tracks"), list);
        it.value().insert(QStringLiteral("trackCount"), list.size());
        const QString destDir = QFileInfo(list.constFirst()).absolutePath();
        it.value().insert(QStringLiteral("destDir"), destDir);
        albums << it.value();
    }

    std::sort(albums.begin(), albums.end(), [](const QVariant &left, const QVariant &right) {
        const QVariantMap l = left.toMap();
        const QVariantMap r = right.toMap();
        const QString la = l.value(QStringLiteral("artist")).toString().toLower();
        const QString ra = r.value(QStringLiteral("artist")).toString().toLower();
        if (la != ra) {
            return la < ra;
        }
        return l.value(QStringLiteral("album")).toString().toLower()
               < r.value(QStringLiteral("album")).toString().toLower();
    });
    return albums;
}

} // namespace

ConvertService::ConvertService(ConfigService *config, LibraryService *library, QObject *parent)
    : QObject(parent), m_config(config), m_library(library) {}

int ConvertService::selectedCount() const {
    int count = 0;
    for (const QVariant &item : m_albums) {
        if (item.toMap().value(QStringLiteral("selected"), true).toBool()) {
            ++count;
        }
    }
    return count;
}

void ConvertService::scanLibrary() {
    m_albums = discoverFlacAlbums(m_library);
    emit albumsChanged();
    if (m_albums.isEmpty()) {
        setStatus(QStringLiteral("No FLAC albums found in the library"));
    } else {
        setStatus(QStringLiteral("Found %1 FLAC album(s)").arg(m_albums.size()));
    }
}

void ConvertService::setAlbumSelected(int index, bool selected) {
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

void ConvertService::setAllAlbumsSelected(bool selected) {
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

void ConvertService::cancelConvert() {
    m_cancelRequested = true;
    QMutexLocker locker(&m_decisionMutex);
    if (m_awaitingDecision) {
        m_decision = Decision::Abort;
        m_decisionCond.wakeAll();
    }
}

void ConvertService::resolveConvertDecision(const QString &action) {
    QMutexLocker locker(&m_decisionMutex);
    if (!m_awaitingDecision) {
        return;
    }
    if (action == QLatin1String("retry")) {
        m_decision = Decision::Retry;
    } else if (action == QLatin1String("skipTrack")) {
        m_decision = Decision::SkipTrack;
    } else if (action == QLatin1String("skipAlbum")) {
        m_decision = Decision::SkipAlbum;
    } else {
        m_decision = Decision::Abort;
    }
    m_awaitingDecision = false;
    m_decisionCond.wakeAll();
}

void ConvertService::clearDecisionState() {
    QMutexLocker locker(&m_decisionMutex);
    m_decision = Decision::None;
    m_awaitingDecision = false;
    m_decisionTitle.clear();
    m_decisionMessage.clear();
    m_decisionDetail.clear();
}

ConvertService::Decision ConvertService::waitForDecision(const QString &title,
                                                         const QString &message,
                                                         const QString &detail) {
    {
        QMutexLocker locker(&m_decisionMutex);
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

    QMutexLocker locker(&m_decisionMutex);
    while (m_decision == Decision::None && !m_cancelRequested) {
        m_decisionCond.wait(&m_decisionMutex);
    }
    const Decision result = m_cancelRequested ? Decision::Abort : m_decision;
    m_awaitingDecision = false;
    m_decision = Decision::None;
    QMetaObject::invokeMethod(this, [this]() { emit decisionChanged(); }, Qt::QueuedConnection);
    return result;
}

void ConvertService::setConverting(bool converting) {
    if (m_converting == converting) {
        return;
    }
    m_converting = converting;
    emit convertingChanged();
}

void ConvertService::setStatus(const QString &status) {
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}

void ConvertService::setProgress(int progress) {
    progress = qBound(0, progress, 100);
    if (m_progress == progress) {
        return;
    }
    m_progress = progress;
    emit progressChanged();
}

void ConvertService::setTrackProgress(int done, int total) {
    if (m_tracksDone == done && m_tracksTotal == total) {
        return;
    }
    m_tracksDone = done;
    m_tracksTotal = total;
    emit progressChanged();
}

void ConvertService::startConvert() {
    if (m_converting || !m_config || !m_library) {
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
        totalTracks += album.value(QStringLiteral("tracks")).toStringList().size();
    }
    if (selectedAlbums.isEmpty()) {
        setStatus(QStringLiteral("No albums selected"));
        return;
    }

    const int bitrateKbps = m_config->opusBitrateKbps();
    const bool deleteSource = m_config->convertDeleteSource();

    m_cancelRequested = false;
    clearDecisionState();
    setConverting(true);
    setTrackProgress(0, totalTracks);
    setProgress(0);
    setStatus(QStringLiteral("Converting %1 album(s), %2 track(s)…")
                  .arg(selectedAlbums.size())
                  .arg(totalTracks));

    ConvertService *self = this;
    (void)QtConcurrent::run([self, selectedAlbums, bitrateKbps, deleteSource, totalTracks]() {
        int tracksDone = 0;
        bool success = true;
        bool abortAll = false;
        QStringList libraryRoots;

        for (const QVariant &item : selectedAlbums) {
            if (abortAll || self->m_cancelRequested) {
                success = false;
                break;
            }

            const QVariantMap album = item.toMap();
            const QString artist = album.value(QStringLiteral("artist")).toString();
            const QString albumName = album.value(QStringLiteral("album")).toString();
            const QStringList tracks = album.value(QStringLiteral("tracks")).toStringList();
            const QString albumLabel = artist + QStringLiteral(" — ") + albumName;
            bool skipAlbum = false;

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

                const QFileInfo info(sourcePath);
                const QString opusPath =
                    info.absolutePath() + QLatin1Char('/') + info.completeBaseName()
                    + QStringLiteral(".opus");

                bool trackOk = QFile::exists(opusPath) && !QFile::exists(sourcePath);
                if (!trackOk && QFile::exists(opusPath) && QFile::exists(sourcePath)) {
                    // Already have Opus beside FLAC — treat as done for convert step.
                    trackOk = true;
                    if (deleteSource) {
                        QFile::remove(sourcePath);
                    }
                }

                while (!trackOk) {
                    QString error;
                    if (convertFlacToOpus(sourcePath, opusPath, bitrateKbps, &error)) {
                        if (deleteSource && !QFile::remove(sourcePath)) {
                            success = false;
                            const Decision decision = self->waitForDecision(
                                QStringLiteral("Could not remove FLAC"),
                                QStringLiteral(
                                    "Opus was written, but the source FLAC could not be deleted."),
                                sourcePath);
                            if (decision == Decision::Retry) {
                                continue;
                            }
                            if (decision == Decision::SkipTrack) {
                                trackOk = true;
                                break;
                            }
                            if (decision == Decision::SkipAlbum) {
                                skipAlbum = true;
                                break;
                            }
                            abortAll = true;
                            break;
                        }
                        trackOk = true;
                        break;
                    }

                    success = false;
                    const Decision decision = self->waitForDecision(
                        QStringLiteral("Conversion failed"),
                        error.isEmpty() ? QStringLiteral("Could not convert this FLAC to Opus.")
                                        : error,
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
        }

        if (self->m_cancelRequested) {
            success = false;
        }

        if (self->m_config) {
            libraryRoots = self->m_config->libraryPaths();
        }

        QMetaObject::invokeMethod(
            self,
            [self, success, libraryRoots, tracksDone, totalTracks]() {
                self->clearDecisionState();
                self->setConverting(false);
                self->setTrackProgress(tracksDone, totalTracks);
                self->setProgress(100);
                self->setStatus(success ? QStringLiteral("Convert complete")
                                        : (self->m_cancelRequested
                                               ? QStringLiteral("Convert cancelled")
                                               : QStringLiteral("Convert finished with issues")));
                emit self->decisionChanged();
                emit self->convertFinished(success);
                if (self->m_library && !libraryRoots.isEmpty()) {
                    QObject::connect(
                        self->m_library, &LibraryService::scanFinished, self,
                        [self](bool) { self->scanLibrary(); },
                        static_cast<Qt::ConnectionType>(Qt::QueuedConnection
                                                        | Qt::SingleShotConnection));
                    self->m_library->rescan(libraryRoots);
                } else {
                    self->scanLibrary();
                }
            },
            Qt::QueuedConnection);
    });
}
