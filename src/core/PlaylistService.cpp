#include "PlaylistService.h"

#include "ConfigService.h"
#include "LibraryService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace {

struct ParsedEntry {
    QString sourcePath;
    QString display;
    int durationSecs = -1;
};

QString unescapeExtInfDisplay(const QString &display) {
    return display.trimmed();
}

QString fallbackDisplayForPath(const QString &path) {
    return QFileInfo(path).completeBaseName();
}

ParsedEntry parseExtInfLine(const QString &raw) {
    ParsedEntry entry;
    const int comma = raw.indexOf(QLatin1Char(','));
    const QString durationPart = comma >= 0 ? raw.left(comma).trimmed() : raw.trimmed();
    entry.display =
        comma >= 0 ? unescapeExtInfDisplay(raw.mid(comma + 1)) : QString();
    bool ok = false;
    const int duration = durationPart.toInt(&ok);
    if (ok && duration >= 0) {
        entry.durationSecs = duration;
    }
    return entry;
}

QVector<ParsedEntry> parseM3u8(const QString &content) {
    QVector<ParsedEntry> entries;
    ParsedEntry pending;
    bool hasPending = false;

    const QStringList lines = content.split(QLatin1Char('\n'));
    for (QString rawLine : lines) {
        rawLine = rawLine.trimmed();
        if (rawLine.startsWith(QChar(0xFEFF))) {
            rawLine = rawLine.mid(1);
        }
        if (rawLine.isEmpty()) {
            continue;
        }

        if (rawLine.startsWith(QStringLiteral("#EXTINF:"), Qt::CaseInsensitive)) {
            pending = parseExtInfLine(rawLine.mid(8));
            hasPending = true;
            continue;
        }

        if (rawLine.startsWith(QLatin1Char('#'))) {
            continue;
        }

        ParsedEntry entry;
        entry.sourcePath = rawLine;
        if (hasPending) {
            entry.display = pending.display;
            entry.durationSecs = pending.durationSecs;
            hasPending = false;
        }
        if (entry.display.isEmpty()) {
            entry.display = fallbackDisplayForPath(entry.sourcePath);
        }
        entries.append(entry);
    }

    return entries;
}

QVariantMap unresolvedEntry(const ParsedEntry &parsed) {
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

    const QVector<ParsedEntry> parsed = parseM3u8(QString::fromUtf8(file.readAll()));
    for (const ParsedEntry &entry : parsed) {
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

    QTextStream out(&file);
    out << "#EXTM3U\n";
    for (const QVariant &item : playlist.entries) {
        const QVariantMap track = item.toMap();
        const QString display =
            track.value(QStringLiteral("artist")).toString().isEmpty()
                ? track.value(QStringLiteral("title")).toString()
                : track.value(QStringLiteral("artist")).toString() + QStringLiteral(" - ")
                      + track.value(QStringLiteral("title")).toString();
        const int durationSecs = qMax(0, track.value(QStringLiteral("durationMs")).toInt() / 1000);
        out << "#EXTINF:" << durationSecs << ',' << display << "\n";

        QString pathLine = track.value(QStringLiteral("sourcePath")).toString();
        if (pathLine.isEmpty()) {
            pathLine =
                relativeTrackPath(track.value(QStringLiteral("path")).toString(), roots);
        }
        pathLine.replace(QLatin1Char('\\'), QLatin1Char('/'));
        out << pathLine << "\n";
    }

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

bool PlaylistService::addTrackToPlaylist(const QString &playlistName, const QVariantMap &track) {
    if (playlistName.isEmpty() || track.value(QStringLiteral("path")).toString().isEmpty()) {
        return false;
    }

    LoadedPlaylist playlist = playlistByName(playlistName);
    if (playlist.filePath.isEmpty() || !QFile::exists(playlist.filePath)) {
        return false;
    }

    QStringList roots;
    if (m_config) {
        for (const QString &path : m_config->libraryPaths()) {
            roots << m_config->expandPath(path);
        }
    }

    QVariantMap entry = track;
    entry.insert(QStringLiteral("resolved"), true);
    entry.insert(QStringLiteral("sourcePath"),
                 relativeTrackPath(track.value(QStringLiteral("path")).toString(), roots));
    playlist.entries << entry;

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
    setStatus(QStringLiteral("Added track to \"%1\"").arg(playlistName));
    return true;
}
