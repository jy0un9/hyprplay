#include "PlaylistService.h"

#include "ConfigService.h"
#include "LibraryService.h"
#include "PlaylistM3u.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QVector>

namespace {

QVariantMap unresolvedEntry(const M3uEntry &parsed) {
    QVariantMap row;
    row.insert(QStringLiteral("path"), QString());
    row.insert(QStringLiteral("title"), parsed.display);
    row.insert(QStringLiteral("artist"), QString());
    row.insert(QStringLiteral("album"), QString());
    row.insert(QStringLiteral("trackNumber"), 0);
    row.insert(QStringLiteral("durationMs"),
              parsed.durationSecs >= 0 ? parsed.durationSecs * 1000 : 0);
    row.insert(QStringLiteral("resolved"), false);
    row.insert(QStringLiteral("sourcePath"), parsed.sourcePath);
    return row;
}

} // namespace

PlaylistService::PlaylistService(ConfigService *config, LibraryService *library, QObject *parent)
    : QObject(parent), m_config(config), m_library(library) {}

QString PlaylistService::playlistsDirectory() const {
    if (!m_config) {
        return {};
    }
    QString dir = m_config->expandPath(m_config->playlistsDir());
    QDir().mkpath(dir);
    return dir;
}

void PlaylistService::setStatus(const QString &status) {
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}

void PlaylistService::reload() {
    m_playlistNames.clear();
    m_trackCounts.clear();
    m_selectedTracks.clear();
    m_selectedTracksDirty = true;

    const QString dirPath = playlistsDirectory();
    if (dirPath.isEmpty()) {
        setStatus(QStringLiteral("Playlists directory not configured"));
        emit playlistsChanged();
        return;
    }

    QDir dir(dirPath);
    const QStringList files =
        dir.entryList({QStringLiteral("*.m3u8")}, QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QString &fileName : files) {
        const QString filePath = dir.absoluteFilePath(fileName);
        const QString name = QFileInfo(filePath).completeBaseName();
        if (name.isEmpty()) {
            continue;
        }
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        const int count = parseM3u8(QString::fromUtf8(file.readAll())).size();
        m_playlistNames << name;
        m_trackCounts.insert(name, count);
    }

    if (!m_selectedPlaylist.isEmpty() && !m_playlistNames.contains(m_selectedPlaylist)) {
        m_selectedPlaylist.clear();
        m_selectedTracks.clear();
        emit selectionChanged();
        emit playlistTracksChanged();
    }

    setStatus(m_playlistNames.isEmpty()
                  ? QStringLiteral("No playlists in %1").arg(dirPath)
                  : QStringLiteral("%1 playlist(s)").arg(m_playlistNames.size()));
    emit playlistsChanged();
}

PlaylistService::LoadedPlaylist PlaylistService::loadPlaylistFile(const QString &filePath) const {
    LoadedPlaylist playlist;
    playlist.filePath = filePath;
    playlist.name = QFileInfo(filePath).completeBaseName();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return playlist;
    }

    const QVector<M3uEntry> parsed = parseM3u8(QString::fromUtf8(file.readAll()));
    for (const M3uEntry &entry : parsed) {
        QVariantMap track =
            m_library->resolvePlaylistEntry(entry.sourcePath, entry.display);
        if (track.isEmpty()) {
            track = unresolvedEntry(entry);
        } else {
            track.insert(QStringLiteral("sourcePath"), entry.sourcePath);
            if (entry.durationSecs >= 0 && track.value(QStringLiteral("durationMs")).toInt() <= 0) {
                track.insert(QStringLiteral("durationMs"), entry.durationSecs * 1000);
            }
        }
        playlist.entries << track;
    }

    return playlist;
}

void PlaylistService::selectPlaylist(const QString &name) {
    if (m_selectedPlaylist == name) {
        return;
    }
    m_selectedPlaylist = name;
    m_selectedTracks.clear();
    m_selectedTracksDirty = true;
    emit selectionChanged();
    emit playlistTracksChanged();
}

QVariantList PlaylistService::tracksForSelectedPlaylist() const {
    if (!m_selectedTracksDirty) {
        return m_selectedTracks;
    }

    m_selectedTracks.clear();
    if (m_selectedPlaylist.isEmpty()) {
        m_selectedTracksDirty = false;
        return m_selectedTracks;
    }

    m_selectedTracks = playlistByName(m_selectedPlaylist).entries;
    m_selectedTracksDirty = false;
    return m_selectedTracks;
}

int PlaylistService::trackCountForPlaylist(const QString &name) const {
    return m_trackCounts.value(name, 0);
}

QString PlaylistService::sanitizePlaylistName(const QString &name) {
    QString sanitized = name.trimmed();
    for (QChar &ch : sanitized) {
        if (ch == QLatin1Char('/') || ch == QLatin1Char('\\') || ch == QLatin1Char(':')
            || ch == QLatin1Char('*') || ch == QLatin1Char('?') || ch == QLatin1Char('"')
            || ch == QLatin1Char('<') || ch == QLatin1Char('>') || ch == QLatin1Char('|')) {
            ch = QLatin1Char('_');
        }
    }
    return sanitized.trimmed();
}

QString PlaylistService::relativeTrackPath(const QString &absolutePath,
                                           const QStringList &roots) {
    const QString abs = QFileInfo(absolutePath).absoluteFilePath();
    for (const QString &root : roots) {
        const QString normalizedRoot = QDir(root).absolutePath();
        if (abs.startsWith(normalizedRoot + QLatin1Char('/'))) {
            return abs.mid(normalizedRoot.length() + 1);
        }
    }
    return abs;
}

bool PlaylistService::savePlaylist(const LoadedPlaylist &playlist) const {
    QFile file(playlist.filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QStringList roots;
    if (m_config) {
        for (const QString &path : m_config->libraryPaths()) {
            roots << m_config->expandPath(path);
        }
    }

    QVector<M3uEntry> entries;
    for (const QVariant &item : playlist.entries) {
        const QVariantMap track = item.toMap();
        M3uEntry entry;
        entry.display =
            track.value(QStringLiteral("artist")).toString().isEmpty()
                ? track.value(QStringLiteral("title")).toString()
                : track.value(QStringLiteral("artist")).toString() + QStringLiteral(" - ")
                      + track.value(QStringLiteral("title")).toString();
        entry.durationSecs = qMax(0, track.value(QStringLiteral("durationMs")).toInt() / 1000);
        entry.sourcePath = track.value(QStringLiteral("sourcePath")).toString();
        if (entry.sourcePath.isEmpty()) {
            entry.sourcePath =
                relativeTrackPath(track.value(QStringLiteral("path")).toString(), roots);
        }
        entries.append(entry);
    }

    file.write(serializeM3u8(entries).toUtf8());
    return true;
}

PlaylistService::LoadedPlaylist PlaylistService::playlistByName(const QString &name) const {
    const QString dirPath = playlistsDirectory();
    const QString filePath =
        QDir(dirPath).absoluteFilePath(name + QStringLiteral(".m3u8"));
    return loadPlaylistFile(filePath);
}

bool PlaylistService::createPlaylist(const QString &name) {
    const QString sanitized = sanitizePlaylistName(name);
    if (sanitized.isEmpty()) {
        setStatus(QStringLiteral("Playlist name cannot be empty"));
        return false;
    }

    const QString filePath =
        QDir(playlistsDirectory()).absoluteFilePath(sanitized + QStringLiteral(".m3u8"));
    if (QFile::exists(filePath)) {
        setStatus(QStringLiteral("Playlist already exists"));
        return false;
    }

    LoadedPlaylist playlist;
    playlist.name = sanitized;
    playlist.filePath = filePath;
    if (!savePlaylist(playlist)) {
        setStatus(QStringLiteral("Failed to create playlist"));
        return false;
    }

    reload();
    selectPlaylist(sanitized);
    setStatus(QStringLiteral("Created playlist \"%1\"").arg(sanitized));
    return true;
}

bool PlaylistService::deletePlaylist(const QString &name) {
    if (name.isEmpty()) {
        return false;
    }

    const QString filePath =
        QDir(playlistsDirectory()).absoluteFilePath(name + QStringLiteral(".m3u8"));
    const QString metaPath =
        QDir(playlistsDirectory()).absoluteFilePath(name + QStringLiteral(".meta.toml"));

    if (!QFile::remove(filePath)) {
        setStatus(QStringLiteral("Failed to delete playlist"));
        return false;
    }
    QFile::remove(metaPath);

    if (m_selectedPlaylist == name) {
        m_selectedPlaylist.clear();
        m_selectedTracks.clear();
        m_selectedTracksDirty = true;
        emit selectionChanged();
        emit playlistTracksChanged();
    }

    reload();
    setStatus(QStringLiteral("Deleted playlist \"%1\"").arg(name));
    return true;
}

bool PlaylistService::renamePlaylist(const QString &oldName, const QString &newName) {
    const QString sanitized = sanitizePlaylistName(newName);
    if (oldName.isEmpty() || sanitized.isEmpty()) {
        setStatus(QStringLiteral("Playlist name cannot be empty"));
        return false;
    }
    if (oldName == sanitized) {
        return true;
    }

    const QDir dir(playlistsDirectory());
    const QString oldPath = dir.absoluteFilePath(oldName + QStringLiteral(".m3u8"));
    const QString newPath = dir.absoluteFilePath(sanitized + QStringLiteral(".m3u8"));
    if (QFile::exists(newPath)) {
        setStatus(QStringLiteral("Playlist already exists"));
        return false;
    }
    if (!QFile::rename(oldPath, newPath)) {
        setStatus(QStringLiteral("Failed to rename playlist"));
        return false;
    }

    if (m_selectedPlaylist == oldName) {
        m_selectedPlaylist = sanitized;
        emit selectionChanged();
    }
    reload();
    setStatus(QStringLiteral("Renamed playlist to \"%1\"").arg(sanitized));
    return true;
}

bool PlaylistService::removeTrackFromPlaylist(const QString &playlistName, int index) {
    if (playlistName.isEmpty() || index < 0) {
        return false;
    }

    LoadedPlaylist playlist = playlistByName(playlistName);
    if (index >= playlist.entries.size()) {
        return false;
    }
    playlist.entries.removeAt(index);
    if (!savePlaylist(playlist)) {
        setStatus(QStringLiteral("Failed to update playlist"));
        return false;
    }

    m_trackCounts.insert(playlistName, playlist.entries.size());
    if (m_selectedPlaylist == playlistName) {
        m_selectedTracksDirty = true;
        emit playlistTracksChanged();
    }
    emit playlistsChanged();
    setStatus(QStringLiteral("Removed track from \"%1\"").arg(playlistName));
    return true;
}

bool PlaylistService::moveTrackInPlaylist(const QString &playlistName, int fromIndex, int toIndex) {
    if (playlistName.isEmpty()) {
        return false;
    }

    LoadedPlaylist playlist = playlistByName(playlistName);
    if (playlist.filePath.isEmpty() || !QFile::exists(playlist.filePath)) {
        return false;
    }
    if (fromIndex < 0 || fromIndex >= playlist.entries.size()) {
        return false;
    }
    const int boundedTo = qBound(0, toIndex, playlist.entries.size() - 1);
    if (fromIndex == boundedTo) {
        return true;
    }

    const QVariant item = playlist.entries.takeAt(fromIndex);
    playlist.entries.insert(boundedTo, item);

    if (!savePlaylist(playlist)) {
        setStatus(QStringLiteral("Failed to reorder playlist"));
        return false;
    }

    if (m_selectedPlaylist == playlistName) {
        m_selectedTracksDirty = true;
        emit playlistTracksChanged();
    }
    setStatus(QStringLiteral("Reordered \"%1\"").arg(playlistName));
    return true;
}

bool PlaylistService::addTrackToPlaylist(const QString &playlistName, const QVariantMap &track) {
    return addTracksToPlaylist(playlistName, QVariantList{track}) > 0;
}

int PlaylistService::addTracksToPlaylist(const QString &playlistName, const QVariantList &tracks) {
    if (playlistName.isEmpty() || tracks.isEmpty()) {
        return 0;
    }

    LoadedPlaylist playlist = playlistByName(playlistName);
    if (playlist.filePath.isEmpty() || !QFile::exists(playlist.filePath)) {
        return 0;
    }

    QStringList roots;
    if (m_config) {
        for (const QString &path : m_config->libraryPaths()) {
            roots << m_config->expandPath(path);
        }
    }

    int added = 0;
    for (const QVariant &item : tracks) {
        const QVariantMap track = item.toMap();
        const QString path = track.value(QStringLiteral("path")).toString();
        if (path.isEmpty()) {
            continue;
        }
        QVariantMap entry = track;
        entry.insert(QStringLiteral("resolved"), true);
        entry.insert(QStringLiteral("sourcePath"), relativeTrackPath(path, roots));
        playlist.entries << entry;
        ++added;
    }

    if (added == 0) {
        return 0;
    }

    if (!savePlaylist(playlist)) {
        setStatus(QStringLiteral("Failed to update playlist"));
        return 0;
    }

    m_trackCounts.insert(playlistName, playlist.entries.size());
    if (m_selectedPlaylist == playlistName) {
        m_selectedTracksDirty = true;
        emit playlistTracksChanged();
    }
    emit playlistsChanged();
    setStatus(added == 1 ? QStringLiteral("Added track to \"%1\"").arg(playlistName)
                         : QStringLiteral("Added %1 tracks to \"%2\"").arg(added).arg(playlistName));
    return added;
}
