#include "AppController.h"

#include "FuzzyMatch.h"

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QFile>
#include <QFileInfo>
#include <QCursor>
#include <QGuiApplication>
#include <QSet>
#include <QTimer>

#include <algorithm>

AppController::AppController(QObject *parent) : QObject(parent) {
    m_config = new ConfigService(this);
    m_library = new LibraryService(this);
    m_playback = new PlaybackService(this);
    m_nowPlaying = new TrackMediaService(m_config, this);
    m_selectedArtistMedia = new ArtistMediaService(m_config, this);
    m_playlists = new PlaylistService(m_config, m_library, this);
    m_tags = new TagService(m_library, this);
    m_discogs = new DiscogsService(m_config, this);
    m_importInbox = new ImportService(m_config, m_library, m_tags, this);
    m_convert = new ConvertService(m_config, m_library, this);
    m_lyrics = new LyricsService(m_config, this);
    m_metadataSearch = new MetadataSearchService(m_config, m_discogs, this);
    m_enrichment = new LibraryEnrichmentService(m_config, m_library, m_discogs, m_metadataSearch,
                                               m_tags, m_lyrics, this, this);
    m_artists = new ArtistModel(this);
    m_tracks = new TrackListModel(this);
    m_playlistItems = new PlaylistListModel(this);
    m_playlistTracks = new TrackListModel(this);

    connect(m_library, &LibraryService::scanFinished, this, &AppController::refreshArtists);
    connect(m_library, &LibraryService::scanStatusChanged, this, &AppController::scanStatusChanged);
    connect(m_playback, &PlaybackService::volumeChanged, this, [this]() {
        m_config->setVolume(m_playback->volume());
    });
    connect(m_library, &LibraryService::scanningChanged, this, [this]() {
        if (m_library->scanning()) {
            m_scanToastArmed = true;
        }
    });
    connect(m_library, &LibraryService::scanFinished, this, [this](bool success) {
        if (!m_scanToastArmed) {
            return;
        }
        m_scanToastArmed = false;
        notify(success ? QStringLiteral("Library scan complete")
                       : QStringLiteral("Library scan failed"),
               success ? QStringLiteral("success") : QStringLiteral("error"));
    });
    connect(m_playback, &PlaybackService::trackChanged, this, &AppController::onTrackChanged);
    connect(m_playback, &PlaybackService::positionChanged, m_nowPlaying,
            [this]() {
                m_nowPlaying->updateLyricPosition(m_playback->position());
            });
    connect(m_playlists, &PlaylistService::playlistsChanged, this, &AppController::refreshPlaylistItems);
    connect(m_playlists, &PlaylistService::playlistTracksChanged, this,
            &AppController::refreshPlaylistTracks);
    connect(m_library, &LibraryService::scanFinished, this, [this](bool success) {
        Q_UNUSED(success);
        reloadPlaylistsIfReady();
    });
    connect(m_discogs, &DiscogsService::artistFetched, this, &AppController::onDiscogsArtistFetched);
    connect(m_discogs, &DiscogsService::releaseFetched, this,
            &AppController::onDiscogsReleaseFetched);
    connect(m_discogs, &DiscogsService::releasesSearchFinished, this,
            [this](const QVariantList &results) {
                if (m_enrichment && m_enrichment->active()
                    && m_enrichment->phase() == QStringLiteral("album")) {
                    m_enrichment->handleAlbumSearchResults(results);
                    return;
                }
                if (m_bulkDiscogsAlbumsActive) {
                    handleBulkDiscogsSearch(results);
                    return;
                }
                if (!m_albumDiscogsOpen) {
                    return;
                }
                const int localTracks = selectedAlbumTrackCount();
                const QVariantList ranked = rankDiscogsAlbumCandidates(results, localTracks);
                m_albumDiscogsCandidates = ranked;
                m_albumDiscogsSelectedIndex = ranked.isEmpty() ? -1 : 0;
                emit albumDiscogsChanged();
            });
    connect(m_discogs, &DiscogsService::statusChanged, this, [this]() {
        const QString status = m_discogs->status();
        if (status.isEmpty() || m_discogs->busy()) {
            return;
        }
        const QString lowered = status.toLower();
        const bool isError = lowered.contains(QStringLiteral("fail"))
                             || lowered.contains(QStringLiteral("error"))
                             || lowered.contains(QStringLiteral("invalid"))
                             || lowered.contains(QStringLiteral("unauthorized"))
                             || lowered.contains(QStringLiteral("not found"));
        if (isError) {
            notify(QStringLiteral("Discogs: ") + status, QStringLiteral("error"));
        }
    });
    connect(m_lyrics, &LyricsService::busyChanged, this, [this]() {
        if (m_lyrics->busy()) {
            return;
        }
        const QString status = m_lyrics->status();
        if (status.isEmpty()) {
            return;
        }
        const QString lowered = status.toLower();
        const bool isError =
            lowered.contains(QStringLiteral("fail")) || lowered.contains(QStringLiteral("error"));
        notify(status, isError ? QStringLiteral("error") : QStringLiteral("success"));
    });
    connect(m_importInbox, &ImportService::importFinished, this, [this](bool success) {
        notify(success ? QStringLiteral("Import complete") : QStringLiteral("Import failed"),
               success ? QStringLiteral("success") : QStringLiteral("error"));
    });
    connect(m_convert, &ConvertService::convertFinished, this, [this](bool success) {
        notify(success ? QStringLiteral("Convert complete") : QStringLiteral("Convert failed"),
               success ? QStringLiteral("success") : QStringLiteral("error"));
    });
    connect(m_tags, &TagService::tagsSaved, this, [this]() {
        refreshArtists();
        refreshTracks();
    });
    connect(m_lyrics, &LyricsService::trackFetched, this,
            [this](const QString &path, bool success) {
                if (!success || path != m_playback->currentPath()) {
                    return;
                }
                // Fresh .lrc landed for the playing track: reload lyrics.
                m_nowPlaying->loadForTrack(path, m_playback->artist(), m_playback->album());
            });
    connect(m_config, &ConfigService::configChanged, this, &AppController::applyUiFont);
}

void AppController::initialize() {
    m_config->load();
    m_playback->setDacPassthrough(m_config->dacPassthrough());
    // Apply the stored value before mpv starts so its initialization signal
    // cannot replace the user's saved volume with PlaybackService's default.
    m_playback->setVolume(m_config->volume());
    // Stored before mpv starts so initMpv() applies it at creation; empty = system default.
    m_playback->setAudioDevice(m_config->audioDevice());
    if (!m_playback->ensureMpv()) {
        qWarning("Audio engine failed to start — playback will not work until mpv initializes");
    }
    m_library->setScanOnLaunch(m_config->scanOnLaunch());
    m_library->setWatchEnabled(m_config->libraryWatchEnabled());
    m_library->ensureLibrary(m_config->libraryPaths());
    applyUiFont();
    refreshArtists();
    reloadPlaylistsIfReady();
}

void AppController::saveOnExit() {
    m_config->setVolume(m_playback->volume());
    m_config->saveOnExit();
}

void AppController::reloadPlaylistsIfReady() {
    m_playlists->reload();
    refreshPlaylistItems();

    if (m_library->scanning()) {
        m_pendingPlaylistReload = true;
        m_playlists->setStatus(QStringLiteral("Waiting for library scan…"));
        if (m_mainView == QStringLiteral("playlists")) {
            m_playlistTracks->setTracks({});
        }
        return;
    }

    m_pendingPlaylistReload = false;
    if (m_mainView == QStringLiteral("playlists")) {
        refreshPlaylistTracks();
    }
}

void AppController::rescanLibrary() {
    m_library->rescan(m_config->libraryPaths());
}

void AppController::showLibrary() {
    if (m_mainView == QStringLiteral("library")) {
        return;
    }
    m_mainView = QStringLiteral("library");
    emit mainViewChanged();
}

void AppController::showPlaylists() {
    if (m_mainView != QStringLiteral("playlists")) {
        m_mainView = QStringLiteral("playlists");
        emit mainViewChanged();
    }
    if (m_playlistFocusColumn != QStringLiteral("playlists")
        && m_playlistFocusColumn != QStringLiteral("tracks")) {
        m_playlistFocusColumn = QStringLiteral("playlists");
        emit playlistFocusChanged();
    }
    reloadPlaylistsIfReady();
}

void AppController::showSettings() {
    if (m_mainView != QStringLiteral("settings")) {
        m_mainView = QStringLiteral("settings");
        emit mainViewChanged();
    }
}

void AppController::setLibraryFocusColumn(const QString &column) {
    const QString normalized = column.trimmed().isEmpty() ? QStringLiteral("artists")
                                                          : column.trimmed();
    if (m_libraryFocusColumn == normalized) {
        return;
    }
    m_libraryFocusColumn = normalized;
    emit libraryFocusChanged();
}

void AppController::libraryMoveUp() {
    if (m_mainView != QStringLiteral("library")) {
        showLibrary();
    }
    if (m_libraryFocusColumn == QStringLiteral("tracks")) {
        const int count = m_tracks->rowCount();
        if (count == 0) {
            return;
        }
        const int current = m_selectedTrackIndex >= 0 ? m_selectedTrackIndex : 0;
        const int next = (current - 1 + count) % count;
        clearMultiTrackSelectionInternal(false);
        m_multiSelectedTracks = {next};
        m_multiSelectAnchor = next;
        setSelectedTrackIndex(next);
        emit multiSelectionChanged();
        return;
    }
    if (m_libraryFocusColumn == QStringLiteral("albums")) {
        const int count = m_albums.size();
        if (count == 0) {
            return;
        }
        int index = m_albums.indexOf(m_selectedAlbum);
        index = index < 0 ? 0 : (index - 1 + count) % count;
        selectAlbum(m_albums.at(index));
        return;
    }
    const int count = m_artists->rowCount();
    if (count == 0) {
        return;
    }
    int index = 0;
    if (!m_selectedArtist.isEmpty()) {
        for (int i = 0; i < count; ++i) {
            if (m_artists->artistAt(i) == m_selectedArtist) {
                index = i;
                break;
            }
        }
        index = (index - 1 + count) % count;
    }
    selectArtist(m_artists->artistAt(index));
}

void AppController::libraryMoveDown() {
    if (m_mainView != QStringLiteral("library")) {
        showLibrary();
    }
    if (m_libraryFocusColumn == QStringLiteral("tracks")) {
        const int count = m_tracks->rowCount();
        if (count == 0) {
            return;
        }
        const int current = m_selectedTrackIndex >= 0 ? m_selectedTrackIndex : -1;
        const int next = (current + 1) % count;
        clearMultiTrackSelectionInternal(false);
        m_multiSelectedTracks = {next};
        m_multiSelectAnchor = next;
        setSelectedTrackIndex(next);
        emit multiSelectionChanged();
        return;
    }
    if (m_libraryFocusColumn == QStringLiteral("albums")) {
        const int count = m_albums.size();
        if (count == 0) {
            return;
        }
        int index = m_albums.indexOf(m_selectedAlbum);
        index = index < 0 ? 0 : (index + 1) % count;
        selectAlbum(m_albums.at(index));
        return;
    }
    const int count = m_artists->rowCount();
    if (count == 0) {
        return;
    }
    int index = 0;
    if (!m_selectedArtist.isEmpty()) {
        for (int i = 0; i < count; ++i) {
            if (m_artists->artistAt(i) == m_selectedArtist) {
                index = i;
                break;
            }
        }
        index = (index + 1) % count;
    }
    selectArtist(m_artists->artistAt(index));
}

void AppController::libraryMoveLeft() {
    if (m_mainView != QStringLiteral("library")) {
        showLibrary();
        return;
    }
    if (m_libraryFocusColumn == QStringLiteral("tracks")) {
        if (!m_albums.isEmpty()) {
            m_libraryFocusColumn = QStringLiteral("albums");
            emit libraryFocusChanged();
        } else {
            m_libraryFocusColumn = QStringLiteral("artists");
            emit libraryFocusChanged();
        }
        return;
    }
    if (m_libraryFocusColumn == QStringLiteral("albums")) {
        m_libraryFocusColumn = QStringLiteral("artists");
        emit libraryFocusChanged();
        return;
    }
    if (!m_selectedAlbum.isEmpty()) {
        selectAlbumDrillOut();
    } else if (!m_selectedArtist.isEmpty()) {
        clearLibrarySelection();
    }
}

void AppController::libraryMoveRight() {
    if (m_mainView != QStringLiteral("library")) {
        showLibrary();
        return;
    }
    if (m_libraryFocusColumn == QStringLiteral("tracks")) {
        if (m_selectedTrackIndex >= 0 && m_selectedTrackIndex < m_tracks->rowCount()) {
            const QVariantMap track = m_tracks->trackAt(m_selectedTrackIndex);
            const QString path =
                QFileInfo(track.value(QStringLiteral("path")).toString()).absoluteFilePath();
            const bool isPlayingTrack =
                !path.isEmpty() && path == m_playback->currentPath()
                && (m_playback->playing() || m_playback->paused());
            if (isPlayingTrack) {
                m_playback->seekRelative(10.0);
            } else {
                playTrackIndex(m_selectedTrackIndex);
            }
        }
        return;
    }
    if (m_libraryFocusColumn == QStringLiteral("artists")) {
        if (!m_albums.isEmpty()) {
            m_libraryFocusColumn = QStringLiteral("albums");
            emit libraryFocusChanged();
            if (m_selectedAlbum.isEmpty()) {
                selectAlbum(m_albums.first());
            }
        } else if (m_tracks->rowCount() > 0) {
            m_libraryFocusColumn = QStringLiteral("tracks");
            emit libraryFocusChanged();
            if (m_selectedTrackIndex < 0) {
                setSelectedTrackIndex(0);
            }
        }
        return;
    }
    if (m_libraryFocusColumn == QStringLiteral("albums") && m_tracks->rowCount() > 0) {
        m_libraryFocusColumn = QStringLiteral("tracks");
        emit libraryFocusChanged();
        if (m_selectedTrackIndex < 0) {
            setSelectedTrackIndex(0);
        }
    }
}

void AppController::setPlaylistFocusColumn(const QString &column) {
    const QString normalized = column.trimmed() == QLatin1String("tracks")
                                   ? QStringLiteral("tracks")
                                   : QStringLiteral("playlists");
    if (m_playlistFocusColumn == normalized) {
        return;
    }
    m_playlistFocusColumn = normalized;
    emit playlistFocusChanged();
}

void AppController::setSelectedPlaylistTrackIndex(int index) {
    const int bounded =
        m_playlistTracks->rowCount() > 0 ? qBound(0, index, m_playlistTracks->rowCount() - 1) : -1;
    if (m_selectedPlaylistTrackIndex == bounded) {
        return;
    }
    m_selectedPlaylistTrackIndex = bounded;
    emit playlistFocusChanged();
}

void AppController::playlistMoveUp() {
    if (m_mainView != QStringLiteral("playlists")) {
        showPlaylists();
    }
    if (m_playlistFocusColumn == QStringLiteral("tracks")) {
        const int count = m_playlistTracks->rowCount();
        if (count == 0) {
            return;
        }
        const int current = m_selectedPlaylistTrackIndex >= 0 ? m_selectedPlaylistTrackIndex : 0;
        setSelectedPlaylistTrackIndex((current - 1 + count) % count);
        return;
    }
    const QStringList names = m_playlists->playlistNames();
    if (names.isEmpty()) {
        return;
    }
    int index = names.indexOf(m_playlists->selectedPlaylist());
    index = index < 0 ? 0 : (index - 1 + names.size()) % names.size();
    selectPlaylist(names.at(index));
}

void AppController::playlistMoveDown() {
    if (m_mainView != QStringLiteral("playlists")) {
        showPlaylists();
    }
    if (m_playlistFocusColumn == QStringLiteral("tracks")) {
        const int count = m_playlistTracks->rowCount();
        if (count == 0) {
            return;
        }
        const int current = m_selectedPlaylistTrackIndex >= 0 ? m_selectedPlaylistTrackIndex : -1;
        setSelectedPlaylistTrackIndex((current + 1) % count);
        return;
    }
    const QStringList names = m_playlists->playlistNames();
    if (names.isEmpty()) {
        return;
    }
    int index = names.indexOf(m_playlists->selectedPlaylist());
    index = index < 0 ? 0 : (index + 1) % names.size();
    selectPlaylist(names.at(index));
}

void AppController::playlistMoveLeft() {
    if (m_mainView != QStringLiteral("playlists")) {
        showPlaylists();
        return;
    }
    if (m_playlistFocusColumn == QStringLiteral("tracks")) {
        m_playlistFocusColumn = QStringLiteral("playlists");
        emit playlistFocusChanged();
    }
}

void AppController::playlistMoveRight() {
    if (m_mainView != QStringLiteral("playlists")) {
        showPlaylists();
        return;
    }
    if (m_playlistFocusColumn == QStringLiteral("tracks")) {
        if (m_selectedPlaylistTrackIndex >= 0
            && m_selectedPlaylistTrackIndex < m_playlistTracks->rowCount()) {
            const QVariantMap track = m_playlistTracks->trackAt(m_selectedPlaylistTrackIndex);
            const QString path =
                QFileInfo(track.value(QStringLiteral("path")).toString()).absoluteFilePath();
            const bool isPlayingTrack =
                !path.isEmpty() && path == m_playback->currentPath()
                && (m_playback->playing() || m_playback->paused());
            if (isPlayingTrack) {
                m_playback->seekRelative(10.0);
            } else {
                playPlaylistTrackIndex(m_selectedPlaylistTrackIndex);
            }
        }
        return;
    }
    if (m_playlists->selectedPlaylist().isEmpty()) {
        const QStringList names = m_playlists->playlistNames();
        if (!names.isEmpty()) {
            selectPlaylist(names.first());
        }
    }
    if (m_playlistTracks->rowCount() > 0) {
        m_playlistFocusColumn = QStringLiteral("tracks");
        emit playlistFocusChanged();
        if (m_selectedPlaylistTrackIndex < 0) {
            setSelectedPlaylistTrackIndex(0);
        }
    }
}

void AppController::clearLibrarySelection() {
    m_selectedArtist.clear();
    m_selectedAlbum.clear();
    m_selectedAlbumArtUrl.clear();
    m_selectedAlbumInfo.clear();
    m_albums.clear();
    m_selectedTrackIndex = -1;
    clearMultiTrackSelectionInternal(true);
    m_tracks->setTracks({});
    m_selectedArtistMedia->clearMedia();
    emit albumsChanged();
    emit selectionChanged();
}

void AppController::selectAlbumDrillOut() {
    m_selectedAlbum.clear();
    m_selectedAlbumArtUrl.clear();
    m_selectedAlbumInfo.clear();
    m_nowPlayingFocused = false;
    m_selectedTrackIndex = -1;
    clearMultiTrackSelectionInternal(true);
    m_tracks->setTracks({});
    emit selectionChanged();
    refreshTracks();
}

void AppController::refreshArtists() {
    QStringList artists = m_library->artists();
    if (m_librarySearchOpen && !m_librarySearchQuery.trimmed().isEmpty()
        && m_librarySearchScope == QStringLiteral("artists")) {
        artists = filterFuzzy(artists, m_librarySearchQuery);
    }
    m_artists->setArtists(artists);
    if (!m_selectedArtist.isEmpty() && m_artists->rowCount() > 0) {
        refreshAlbums();
    }
}

void AppController::refreshPlaylistItems() {
    m_playlistItems->setPlaylists(m_playlists->playlistNames(), m_playlists->trackCounts());
}

void AppController::refreshPlaylistTracks() {
    m_playlistTracks->setTracks(m_playlists->tracksForSelectedPlaylist());
    if (m_selectedPlaylistTrackIndex >= m_playlistTracks->rowCount()) {
        m_selectedPlaylistTrackIndex =
            m_playlistTracks->rowCount() > 0 ? 0 : -1;
        emit playlistFocusChanged();
    }
}

void AppController::selectArtist(const QString &artist) {
    if (m_selectedArtist == artist) {
        if (m_nowPlayingFocused) {
            m_nowPlayingFocused = false;
            emit selectionChanged();
        }
        return;
    }
    m_selectedArtist = artist;
    m_selectedAlbum.clear();
    m_selectedAlbumArtUrl.clear();
    m_nowPlayingFocused = false;
    m_selectedTrackIndex = -1;
    clearMultiTrackSelectionInternal(true);
    if (m_libraryFocusColumn != QStringLiteral("artists")) {
        m_libraryFocusColumn = QStringLiteral("artists");
        emit libraryFocusChanged();
    }
    emit selectionChanged();
    refreshAlbums();
    m_tracks->setTracks({});

    const QString samplePath = m_library->sampleTrackPathForArtist(artist);
    m_selectedArtistMedia->loadForArtist(artist, samplePath);
}

void AppController::selectAlbum(const QString &album) {
    if (m_selectedAlbum == album) {
        if (m_nowPlayingFocused) {
            m_nowPlayingFocused = false;
            emit selectionChanged();
        }
        return;
    }
    m_selectedAlbum = album;
    m_nowPlayingFocused = false;
    m_selectedTrackIndex = -1;
    clearMultiTrackSelectionInternal(true);
    if (m_libraryFocusColumn != QStringLiteral("albums")) {
        m_libraryFocusColumn = QStringLiteral("albums");
        emit libraryFocusChanged();
    }
    const QVariantList albumTracks = m_library->tracksForAlbum(m_selectedArtist, album);
    m_selectedAlbumArtUrl.clear();
    m_selectedAlbumInfo.clear();
    if (!albumTracks.isEmpty()) {
        const QString trackPath =
            albumTracks.first().toMap().value(QStringLiteral("path")).toString();
        m_selectedAlbumArtUrl = m_nowPlaying->albumArtForTrack(trackPath);
        QFile infoFile(QFileInfo(trackPath).absolutePath() + QStringLiteral("/album-info.txt"));
        if (infoFile.open(QIODevice::ReadOnly | QIODevice::Text))
            m_selectedAlbumInfo = QString::fromUtf8(infoFile.readAll()).trimmed();
    }
    emit selectionChanged();
    refreshTracks();
}

void AppController::selectPlaylist(const QString &name) {
    m_playlists->selectPlaylist(name);
    clearLibrarySelection();
    m_selectedPlaylistTrackIndex = -1;
    m_playlistFocusColumn = QStringLiteral("playlists");
    emit playlistFocusChanged();
    refreshPlaylistTracks();
}

void AppController::refreshAlbums() {
    QStringList albums = m_library->albumsForArtist(m_selectedArtist);
    if (m_librarySearchOpen && !m_librarySearchQuery.trimmed().isEmpty()
        && m_librarySearchScope == QStringLiteral("albums")) {
        albums = filterFuzzy(albums, m_librarySearchQuery);
    }
    m_albums = albums;
    emit albumsChanged();
}

void AppController::refreshTracks() {
    QVariantList tracks;
    if (m_librarySearchOpen && !m_librarySearchQuery.trimmed().isEmpty()
        && m_librarySearchScope == QStringLiteral("tracks")) {
        if (!m_selectedArtist.isEmpty() && !m_selectedAlbum.isEmpty()) {
            tracks = m_library->tracksForAlbum(m_selectedArtist, m_selectedAlbum);
        } else if (!m_selectedArtist.isEmpty()) {
            tracks = m_library->tracksForArtist(m_selectedArtist);
        } else {
            tracks = m_library->allTracks();
        }
        tracks = filterTracksFuzzy(tracks, m_librarySearchQuery);
    } else if (!m_selectedArtist.isEmpty() && !m_selectedAlbum.isEmpty()) {
        tracks = m_library->tracksForAlbum(m_selectedArtist, m_selectedAlbum);
    }
    m_tracks->setTracks(tracks);
    if (m_selectedTrackIndex >= m_tracks->rowCount()) {
        m_selectedTrackIndex = m_tracks->rowCount() > 0 ? 0 : -1;
        emit selectionChanged();
    }
}

void AppController::setSelectedTrackIndex(int index) {
    const int bounded = m_tracks->rowCount() > 0 ? qBound(0, index, m_tracks->rowCount() - 1) : -1;
    if (m_selectedTrackIndex == bounded) {
        return;
    }
    m_selectedTrackIndex = bounded;
    emit selectionChanged();
}

void AppController::clearMultiTrackSelectionInternal(bool emitSignal) {
    if (m_multiSelectedTracks.isEmpty() && m_multiSelectAnchor < 0) {
        return;
    }
    m_multiSelectedTracks.clear();
    m_multiSelectAnchor = -1;
    if (emitSignal) {
        emit multiSelectionChanged();
    }
}

void AppController::clearMultiTrackSelection() {
    clearMultiTrackSelectionInternal(true);
}

QVariantList AppController::multiSelectedTrackIndices() const {
    QVariantList out;
    out.reserve(m_multiSelectedTracks.size());
    for (int index : m_multiSelectedTracks) {
        out << index;
    }
    return out;
}

bool AppController::isTrackMultiSelected(int index) const {
    return m_multiSelectedTracks.contains(index);
}

void AppController::handleTrackClick(int index, int modifiers) {
    if (index < 0 || index >= m_tracks->rowCount()) {
        return;
    }
    setLibraryFocusColumn(QStringLiteral("tracks"));

    const bool ctrl = (modifiers & Qt::ControlModifier) != 0;
    const bool shift = (modifiers & Qt::ShiftModifier) != 0;

    if (shift && m_multiSelectAnchor >= 0) {
        const int from = qMin(m_multiSelectAnchor, index);
        const int to = qMax(m_multiSelectAnchor, index);
        m_multiSelectedTracks.clear();
        for (int i = from; i <= to; ++i) {
            m_multiSelectedTracks << i;
        }
        m_selectedTrackIndex = index;
        emit selectionChanged();
        emit multiSelectionChanged();
        return;
    }

    if (ctrl) {
        if (m_multiSelectedTracks.contains(index)) {
            m_multiSelectedTracks.removeAll(index);
        } else {
            m_multiSelectedTracks << index;
            std::sort(m_multiSelectedTracks.begin(), m_multiSelectedTracks.end());
        }
        m_multiSelectAnchor = index;
        m_selectedTrackIndex = index;
        emit selectionChanged();
        emit multiSelectionChanged();
        return;
    }

    clearMultiTrackSelectionInternal(false);
    m_multiSelectedTracks = {index};
    m_multiSelectAnchor = index;
    emit multiSelectionChanged();
    playTrackIndex(index);
}

void AppController::prepareTrackContextMenu(int index) {
    if (index < 0 || index >= m_tracks->rowCount()) {
        return;
    }
    setLibraryFocusColumn(QStringLiteral("tracks"));
    m_selectedTrackIndex = index;
    if (!m_multiSelectedTracks.contains(index)) {
        m_multiSelectedTracks = {index};
        m_multiSelectAnchor = index;
        emit multiSelectionChanged();
    }
    emit selectionChanged();
}

void AppController::selectAllVisibleTracks() {
    if (m_mainView != QStringLiteral("library") || m_tracks->rowCount() <= 0) {
        return;
    }
    setLibraryFocusColumn(QStringLiteral("tracks"));
    m_multiSelectedTracks.clear();
    for (int i = 0; i < m_tracks->rowCount(); ++i) {
        m_multiSelectedTracks << i;
    }
    m_multiSelectAnchor = 0;
    if (m_selectedTrackIndex < 0) {
        m_selectedTrackIndex = 0;
    }
    emit selectionChanged();
    emit multiSelectionChanged();
}

void AppController::notify(const QString &text, const QString &kind) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    m_noticeText = trimmed;
    m_noticeKind = kind.trimmed().isEmpty() ? QStringLiteral("info") : kind.trimmed();
    ++m_noticeSerial;
    emit noticeChanged();
}

void AppController::playTrackIndex(int index) {
    const QVariantMap track = m_tracks->trackAt(index);
    if (track.isEmpty()) {
        return;
    }
    m_nowPlayingFocused = true;
    m_selectedTrackIndex = index;
    emit selectionChanged();

    QVariantList queue;
    for (int i = 0; i < m_tracks->rowCount(); ++i) {
        queue << m_tracks->trackAt(i);
    }
    m_playback->playTracks(queue, index);
}

void AppController::playPlaylistTrackIndex(int index) {
    if (index < 0 || index >= m_playlistTracks->rowCount()) {
        return;
    }

    QVariantList queue;
    int startIndex = 0;
    for (int i = 0; i < m_playlistTracks->rowCount(); ++i) {
        const QVariantMap track = m_playlistTracks->trackAt(i);
        if (!track.value(QStringLiteral("resolved"), true).toBool()
            || track.value(QStringLiteral("path")).toString().isEmpty()) {
            continue;
        }
        if (i == index) {
            startIndex = queue.size();
        }
        queue << track;
    }

    if (queue.isEmpty()) {
        return;
    }
    m_nowPlayingFocused = true;
    emit selectionChanged();
    m_playback->playTracks(queue, startIndex);
}

void AppController::playPlaylist() {
    if (m_playlistTracks->rowCount() == 0) {
        refreshPlaylistTracks();
    }
    for (int i = 0; i < m_playlistTracks->rowCount(); ++i) {
        const QVariantMap track = m_playlistTracks->trackAt(i);
        if (track.value(QStringLiteral("resolved"), true).toBool()
            && !track.value(QStringLiteral("path")).toString().isEmpty()) {
            playPlaylistTrackIndex(i);
            return;
        }
    }
}

void AppController::playAlbum() {
    if (m_tracks->rowCount() == 0) {
        refreshTracks();
    }
    if (m_tracks->rowCount() > 0) {
        m_nowPlayingFocused = true;
        emit selectionChanged();
        playTrackIndex(0);
    }
}

void AppController::openLibrarySearch() {
    if (m_mainView != QStringLiteral("library")) {
        showLibrary();
    }
    if (m_librarySearchOpen) {
        emit librarySearchFocusRequested();
        return;
    }
    m_librarySearchOpen = true;
    emit librarySearchOpenChanged();
    emit librarySearchChanged();
    refreshArtists();
    refreshAlbums();
    refreshTracks();
}

void AppController::closeLibrarySearch() {
    if (!m_librarySearchOpen) {
        return;
    }
    m_librarySearchOpen = false;
    m_librarySearchQuery.clear();
    emit librarySearchOpenChanged();
    emit librarySearchChanged();
    refreshArtists();
    refreshAlbums();
    refreshTracks();
    // Re-emit so QML scrolls to the selection in the restored (unfiltered) lists.
    emit selectionChanged();
}

void AppController::syncLibraryFocusToSearchScope() {
    const QString column = m_librarySearchScope == QStringLiteral("albums")
                               ? QStringLiteral("albums")
                               : (m_librarySearchScope == QStringLiteral("tracks")
                                      ? QStringLiteral("tracks")
                                      : QStringLiteral("artists"));
    setLibraryFocusColumn(column);
}

void AppController::ensureLibrarySearchSelection() {
    if (!m_librarySearchOpen || m_librarySearchQuery.trimmed().isEmpty()) {
        return;
    }

    if (m_librarySearchScope == QStringLiteral("albums")) {
        if (m_albums.isEmpty()) {
            return;
        }
        if (m_albums.indexOf(m_selectedAlbum) < 0) {
            selectAlbum(m_albums.first());
        }
        return;
    }

    if (m_librarySearchScope == QStringLiteral("tracks")) {
        if (m_tracks->rowCount() == 0) {
            return;
        }
        if (m_selectedTrackIndex < 0 || m_selectedTrackIndex >= m_tracks->rowCount()) {
            clearMultiTrackSelectionInternal(false);
            m_multiSelectedTracks = {0};
            m_multiSelectAnchor = 0;
            setSelectedTrackIndex(0);
            emit multiSelectionChanged();
        }
        return;
    }

    if (m_artists->rowCount() == 0) {
        return;
    }
    bool found = false;
    if (!m_selectedArtist.isEmpty()) {
        for (int i = 0; i < m_artists->rowCount(); ++i) {
            if (m_artists->artistAt(i) == m_selectedArtist) {
                found = true;
                break;
            }
        }
    }
    if (!found) {
        selectArtist(m_artists->artistAt(0));
    }
}

void AppController::acceptLibrarySearch() {
    if (!m_librarySearchOpen) {
        return;
    }
    syncLibraryFocusToSearchScope();
    ensureLibrarySearchSelection();
    closeLibrarySearch();
}

void AppController::librarySearchMoveUp() {
    if (!m_librarySearchOpen) {
        return;
    }
    syncLibraryFocusToSearchScope();
    libraryMoveUp();
}

void AppController::librarySearchMoveDown() {
    if (!m_librarySearchOpen) {
        return;
    }
    syncLibraryFocusToSearchScope();
    libraryMoveDown();
}

void AppController::setLibrarySearchQuery(const QString &query) {
    if (m_librarySearchQuery == query) {
        return;
    }
    m_librarySearchQuery = query;
    emit librarySearchChanged();
    refreshArtists();
    refreshAlbums();
    refreshTracks();
    ensureLibrarySearchSelection();
}

void AppController::setLibrarySearchScope(const QString &scope) {
    const QString normalized = scope.trimmed().isEmpty() ? QStringLiteral("artists") : scope.trimmed();
    if (m_librarySearchScope == normalized) {
        return;
    }
    m_librarySearchScope = normalized;
    emit librarySearchChanged();
    if (m_librarySearchOpen && !m_librarySearchQuery.trimmed().isEmpty()) {
        refreshArtists();
        refreshAlbums();
        refreshTracks();
        ensureLibrarySearchSelection();
    }
}

void AppController::cycleLibrarySearchScope() {
    static const QStringList order = {QStringLiteral("artists"), QStringLiteral("albums"),
                                      QStringLiteral("tracks")};
    const int current = qMax(0, order.indexOf(m_librarySearchScope));
    setLibrarySearchScope(order.at((current + 1) % order.size()));
    if (!m_librarySearchOpen) {
        openLibrarySearch();
    } else {
        emit librarySearchFocusRequested();
    }
}

QString AppController::highlightSearchMatch(const QString &text,
                                            const QString &accentColor) const {
    QColor accent(accentColor);
    if (!accent.isValid()) {
        accent = QColor(QStringLiteral("#7aa2f7"));
    }
    if (!m_librarySearchOpen || m_librarySearchQuery.trimmed().isEmpty()) {
        return text.toHtmlEscaped();
    }
    const QList<int> positions = fuzzyMatchPositions(text, m_librarySearchQuery);
    if (positions.isEmpty()) {
        return text.toHtmlEscaped();
    }
    QSet<int> matched(positions.begin(), positions.end());
    QString out;
    out.reserve(text.size() * 2);
    for (int i = 0; i < text.size(); ++i) {
        if (matched.contains(i)) {
            out += QStringLiteral("<font color=\"%1\"><b>%2</b></font>")
                       .arg(accent.name(), QString(text.at(i)).toHtmlEscaped());
        } else {
            out += QString(text.at(i)).toHtmlEscaped();
        }
    }
    return out;
}

QPoint AppController::cursorPos() const {
    return QCursor::pos();
}


void AppController::createPlaylist(const QString &name) {
    if (m_playlists->createPlaylist(name)) {
        refreshPlaylistItems();
    }
}

void AppController::deleteSelectedPlaylist() {
    const QString name = m_playlists->selectedPlaylist();
    if (name.isEmpty()) {
        return;
    }
    if (m_playlists->deletePlaylist(name)) {
        refreshPlaylistItems();
        refreshPlaylistTracks();
    }
}

void AppController::renameSelectedPlaylist(const QString &name) {
    const QString oldName = m_playlists->selectedPlaylist();
    if (oldName.isEmpty()) {
        return;
    }
    if (m_playlists->renamePlaylist(oldName, name)) {
        refreshPlaylistItems();
        refreshPlaylistTracks();
    }
}

void AppController::removePlaylistTrack(int index) {
    const QString playlistName = m_playlists->selectedPlaylist();
    if (playlistName.isEmpty()) {
        return;
    }
    if (m_playlists->removeTrackFromPlaylist(playlistName, index)) {
        refreshPlaylistItems();
        refreshPlaylistTracks();
    }
}

void AppController::movePlaylistTrack(int fromIndex, int toIndex) {
    const QString playlistName = m_playlists->selectedPlaylist();
    if (playlistName.isEmpty()) {
        return;
    }
    if (!m_playlists->moveTrackInPlaylist(playlistName, fromIndex, toIndex)) {
        return;
    }
    refreshPlaylistTracks();
    setSelectedPlaylistTrackIndex(toIndex);
}

void AppController::addTrackToPlaylist(int trackIndex, const QString &playlistName) {
    const QString name = playlistName.trimmed();
    if (name.isEmpty()) {
        return;
    }
    const QVariantMap track = m_tracks->trackAt(trackIndex);
    if (track.value(QStringLiteral("path")).toString().isEmpty()) {
        notify(QStringLiteral("No track selected"), QStringLiteral("error"));
        return;
    }
    const int added = m_playlists->addTracksToPlaylist(name, QVariantList{track});
    if (added > 0) {
        refreshPlaylistItems();
        refreshPlaylistTracks();
        notify(m_playlists->lastDuplicateSkipCount() > 0
                   ? m_playlists->status()
                   : QStringLiteral("Added to \"%1\"").arg(name),
               QStringLiteral("success"));
    } else if (m_playlists->lastDuplicateSkipCount() > 0) {
        notify(m_playlists->status(), QStringLiteral("info"));
    } else {
        notify(QStringLiteral("Failed to add to \"%1\"").arg(name), QStringLiteral("error"));
    }
}

void AppController::addSelectedTracksToPlaylist(const QString &playlistName) {
    const QString name = playlistName.trimmed();
    if (name.isEmpty()) {
        return;
    }

    QList<int> indices = m_multiSelectedTracks;
    if (indices.isEmpty() && m_selectedTrackIndex >= 0) {
        indices = {m_selectedTrackIndex};
    }
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

    QVariantList tracks;
    tracks.reserve(indices.size());
    for (int index : indices) {
        const QVariantMap track = m_tracks->trackAt(index);
        if (!track.value(QStringLiteral("path")).toString().isEmpty()) {
            tracks << track;
        }
    }
    if (tracks.isEmpty()) {
        notify(QStringLiteral("No track selected"), QStringLiteral("error"));
        return;
    }

    const int added = m_playlists->addTracksToPlaylist(name, tracks);
    if (added > 0) {
        refreshPlaylistItems();
        refreshPlaylistTracks();
        notify(m_playlists->lastDuplicateSkipCount() > 0
                   ? m_playlists->status()
                   : (added == 1 ? QStringLiteral("Added 1 track to \"%1\"").arg(name)
                                 : QStringLiteral("Added %1 tracks to \"%2\"").arg(added).arg(name)),
               QStringLiteral("success"));
    } else if (m_playlists->lastDuplicateSkipCount() > 0) {
        notify(m_playlists->status(), QStringLiteral("info"));
    } else {
        notify(QStringLiteral("Failed to add to \"%1\"").arg(name), QStringLiteral("error"));
    }
}

void AppController::addAlbumToPlaylist(const QString &playlistName) {
    const QString name = playlistName.trimmed();
    if (name.isEmpty()) {
        return;
    }
    if (m_tracks->rowCount() == 0) {
        notify(QStringLiteral("No tracks in this album"), QStringLiteral("error"));
        return;
    }

    QVariantList tracks;
    tracks.reserve(m_tracks->rowCount());
    for (int i = 0; i < m_tracks->rowCount(); ++i) {
        tracks << m_tracks->trackAt(i);
    }

    const int added = m_playlists->addTracksToPlaylist(name, tracks);
    if (added > 0) {
        refreshPlaylistItems();
        refreshPlaylistTracks();
        notify(m_playlists->lastDuplicateSkipCount() > 0
                   ? m_playlists->status()
                   : (added == 1 ? QStringLiteral("Added 1 track to \"%1\"").arg(name)
                                 : QStringLiteral("Added %1 tracks to \"%2\"").arg(added).arg(name)),
               QStringLiteral("success"));
    } else if (m_playlists->lastDuplicateSkipCount() > 0) {
        notify(m_playlists->status(), QStringLiteral("info"));
    } else {
        notify(QStringLiteral("Failed to add to \"%1\"").arg(name), QStringLiteral("error"));
    }
}

void AppController::onTrackChanged() {
    m_nowPlaying->loadForTrack(m_playback->currentPath(), m_playback->artist(),
                               m_playback->album());
    m_nowPlaying->updateLyricPosition(m_playback->position());
}

void AppController::setTagEditor(bool open, const QString &mode, const QString &title,
                                 const QVariantMap &fields, const QString &path) {
    m_tagEditorOpen = open;
    m_tagEditorMode = mode;
    m_tagEditorTitle = title;
    m_tagEditorFields = fields;
    m_tagEditorPath = path;
    emit tagEditorChanged();
}

void AppController::openTrackTagEditor(int index) {
    const QVariantMap track = m_tracks->trackAt(index);
    const QString path = track.value(QStringLiteral("path")).toString();
    if (path.isEmpty()) {
        return;
    }
    setTagEditor(true, QStringLiteral("track"),
                 track.value(QStringLiteral("title")).toString(), m_tags->loadTags(path), path);
}

void AppController::openAlbumTagEditor() {
    if (m_selectedArtist.isEmpty() || m_selectedAlbum.isEmpty()) {
        return;
    }
    const QStringList paths = m_tags->albumTagPaths(m_selectedArtist, m_selectedAlbum);
    QVariantMap fields;
    if (!paths.isEmpty()) {
        fields = m_tags->loadTags(paths.first());
        fields.remove(QStringLiteral("title"));
        fields.remove(QStringLiteral("trackNumber"));
    } else {
        fields.insert(QStringLiteral("artist"), m_selectedArtist);
        fields.insert(QStringLiteral("album"), m_selectedAlbum);
    }
    setTagEditor(true, QStringLiteral("album"), m_selectedAlbum, fields);
}

void AppController::openArtistTagEditor() {
    if (m_selectedArtist.isEmpty()) {
        return;
    }
    const QStringList paths = m_tags->artistTagPaths(m_selectedArtist);
    QVariantMap fields;
    if (!paths.isEmpty()) {
        fields = m_tags->loadTags(paths.first());
        fields.remove(QStringLiteral("title"));
        fields.remove(QStringLiteral("trackNumber"));
        fields.remove(QStringLiteral("album"));
    } else {
        fields.insert(QStringLiteral("artist"), m_selectedArtist);
    }
    setTagEditor(true, QStringLiteral("artist"), m_selectedArtist, fields);
}

void AppController::closeTagEditor() {
    setTagEditor(false, {}, {}, {});
}

bool AppController::saveTagEditor(const QVariantMap &fields) {
    bool ok = false;
    if (m_tagEditorMode == QStringLiteral("album")) {
        ok = m_tags->saveAlbumTags(m_selectedArtist, m_selectedAlbum, fields);
    } else if (m_tagEditorMode == QStringLiteral("artist")) {
        ok = m_tags->saveArtistTags(m_selectedArtist, fields);
        if (ok) {
            const QString newArtist = fields.value(QStringLiteral("artist")).toString().trimmed();
            if (!newArtist.isEmpty() && newArtist != m_selectedArtist) {
                m_selectedArtist = newArtist;
                emit selectionChanged();
            }
        }
    } else if (!m_tagEditorPath.isEmpty()) {
        ok = m_tags->saveTrackTags(m_tagEditorPath, fields);
    }

    if (ok) {
        if (m_tagEditorMode == QStringLiteral("album")
            || m_tagEditorMode == QStringLiteral("artist")) {
            refreshArtists();
            refreshAlbums();
            refreshTracks();
        }
        closeTagEditor();
    }
    return ok;
}

int AppController::selectedAlbumTrackCount() const {
    if (m_selectedArtist.isEmpty() || m_selectedAlbum.isEmpty() || !m_library) {
        return 0;
    }
    return m_library->tracksForAlbum(m_selectedArtist, m_selectedAlbum).size();
}

void AppController::openTagFetch(const QVariantMap &editorFields) {
    QString artist;
    QString album;
    QString albumArtist;

    if (m_titleFixOpen) {
        closeTitleFix();
    }

    if (!editorFields.isEmpty()) {
        artist = editorFields.value(QStringLiteral("artist")).toString().trimmed();
        album = editorFields.value(QStringLiteral("album")).toString().trimmed();
        albumArtist = editorFields.value(QStringLiteral("albumArtist")).toString().trimmed();
    } else if (m_tagEditorMode == QStringLiteral("track")) {
        artist = m_tagEditorFields.value(QStringLiteral("artist")).toString().trimmed();
        album = m_tagEditorFields.value(QStringLiteral("album")).toString().trimmed();
        albumArtist = m_tagEditorFields.value(QStringLiteral("albumArtist")).toString().trimmed();
    } else if (m_tagEditorMode == QStringLiteral("album")) {
        artist = m_selectedArtist;
        album = m_selectedAlbum;
        albumArtist = m_tagEditorFields.value(QStringLiteral("albumArtist")).toString().trimmed();
    } else if (m_tagEditorMode == QStringLiteral("artist")) {
        artist = m_selectedArtist;
        albumArtist = m_tagEditorFields.value(QStringLiteral("albumArtist")).toString().trimmed();
        const QStringList albums = m_library->albumsForArtist(m_selectedArtist);
        album = albums.isEmpty() ? QString() : albums.first();
    } else {
        return;
    }

    m_tagFetchOpen = true;
    emit tagFetchChanged();
    m_metadataSearch->clear();

    if (album.isEmpty() && artist.isEmpty()) {
        m_metadataSearch->setStatus(QStringLiteral("Need artist or album info to search"));
        return;
    }

    QVariantMap current = m_tagEditorFields;
    if (m_tagEditorMode == QStringLiteral("track") && !m_tagEditorPath.isEmpty()) {
        current = m_tags->loadTags(m_tagEditorPath);
    }
    if (!editorFields.isEmpty()) {
        for (auto it = editorFields.constBegin(); it != editorFields.constEnd(); ++it) {
            current.insert(it.key(), it.value());
        }
    }
    current.insert(QStringLiteral("artist"), artist);
    current.insert(QStringLiteral("album"), album);
    if (!albumArtist.isEmpty()) {
        current.insert(QStringLiteral("albumArtist"), albumArtist);
    }
    m_metadataSearch->setCurrentFields(current);
    if (!m_selectedArtist.isEmpty() && !m_selectedAlbum.isEmpty()) {
        m_metadataSearch->setLocalTrackCount(
            m_library->tracksForAlbum(m_selectedArtist, m_selectedAlbum).size());
    } else if (m_tagEditorMode == QStringLiteral("track") && !album.isEmpty()) {
        const QString trackArtist = artist.isEmpty() ? albumArtist : artist;
        if (!trackArtist.isEmpty()) {
            m_metadataSearch->setLocalTrackCount(
                m_library->tracksForAlbum(trackArtist, album).size());
        }
    }
    m_metadataSearch->searchRelease(
        artist, album, albumArtist,
        QVariantList{QStringLiteral("CD"), QStringLiteral("Digital")}, {});
}

void AppController::closeTagFetch() {
    if (!m_tagFetchOpen) {
        return;
    }
    m_tagFetchOpen = false;
    m_metadataSearch->clear();
    emit tagFetchChanged();
}

bool AppController::applyTagFetch() {
    const QVariantMap allFields = m_metadataSearch->checkedFields();
    if (allFields.isEmpty()) {
        return false;
    }

    QVariantMap fileFields = allFields;
    fileFields.remove(QStringLiteral("mb_albumid"));

    bool ok = false;
    if (m_tagEditorMode == QStringLiteral("album")) {
        ok = m_tags->saveAlbumTags(m_selectedArtist, m_selectedAlbum, fileFields);
    } else if (m_tagEditorMode == QStringLiteral("artist")) {
        ok = m_tags->saveArtistTags(m_selectedArtist, fileFields);
        const QString newArtist = fileFields.value(QStringLiteral("artist")).toString().trimmed();
        if (ok && !newArtist.isEmpty() && newArtist != m_selectedArtist) {
            m_selectedArtist = newArtist;
            emit selectionChanged();
        }
    } else if (!m_tagEditorPath.isEmpty()) {
        ok = m_tags->saveTrackTags(m_tagEditorPath, fileFields);
    }

    if (ok) {
        refreshArtists();
        refreshAlbums();
        refreshTracks();

        if (m_tagEditorOpen) {
            if (m_tagEditorMode == QStringLiteral("track") && !m_tagEditorPath.isEmpty()) {
                setTagEditor(true, m_tagEditorMode, m_tagEditorTitle, m_tags->loadTags(m_tagEditorPath),
                             m_tagEditorPath);
            } else if (m_tagEditorMode == QStringLiteral("album")) {
                openAlbumTagEditor();
            } else if (m_tagEditorMode == QStringLiteral("artist")) {
                openArtistTagEditor();
            }
        }
        closeTagFetch();
    }
    return ok;
}

void AppController::openTitleFix() {
    if (m_selectedArtist.isEmpty() || m_selectedAlbum.isEmpty()) {
        notify(QStringLiteral("Select an album first"), QStringLiteral("error"));
        return;
    }

    if (m_tagFetchOpen) {
        closeTagFetch();
    }

    const QVariantList tracks = m_library->tracksForAlbum(m_selectedArtist, m_selectedAlbum);
    if (tracks.isEmpty()) {
        notify(QStringLiteral("No tracks in this album"), QStringLiteral("error"));
        return;
    }

    m_titleFixOpen = true;
    emit titleFixChanged();
    m_metadataSearch->clear();
    m_metadataSearch->setLocalTracksForTitleFix(tracks);

    QVariantMap current;
    current.insert(QStringLiteral("artist"), m_selectedArtist);
    current.insert(QStringLiteral("album"), m_selectedAlbum);
    m_metadataSearch->setCurrentFields(current);
    m_metadataSearch->searchRelease(
        m_selectedArtist, m_selectedAlbum, {},
        QVariantList{QStringLiteral("CD"), QStringLiteral("Digital")}, {});
}

void AppController::closeTitleFix() {
    if (!m_titleFixOpen) {
        return;
    }
    m_titleFixOpen = false;
    m_metadataSearch->clear();
    emit titleFixChanged();
}

bool AppController::applyTitleFix() {
    const QVariantList proposals = m_metadataSearch->checkedTitleFixProposals();
    if (proposals.isEmpty()) {
        notify(QStringLiteral("No title changes selected"), QStringLiteral("info"));
        return false;
    }

    int updated = 0;
    int failed = 0;
    for (const QVariant &proposalValue : proposals) {
        const QVariantMap proposal = proposalValue.toMap();
        const QString path = proposal.value(QStringLiteral("path")).toString();
        const QString title = proposal.value(QStringLiteral("proposed")).toString().trimmed();
        if (path.isEmpty() || title.isEmpty()) {
            ++failed;
            continue;
        }
        QVariantMap fields;
        fields.insert(QStringLiteral("title"), title);
        if (!m_tags->saveTrackTags(path, fields)) {
            ++failed;
            continue;
        }
        ++updated;
    }

    refreshTracks();

    const int unmatched = m_metadataSearch->titleFixUnmatched().size();
    QString message = QStringLiteral("Fixed %1 title%2")
                          .arg(updated)
                          .arg(updated == 1 ? QString() : QStringLiteral("s"));
    if (unmatched > 0) {
        message += QStringLiteral("; %1 unmatched — fix manually").arg(unmatched);
    }
    if (failed > 0) {
        message += QStringLiteral("; %1 failed").arg(failed);
    }
    notify(message, failed > 0 ? QStringLiteral("error") : QStringLiteral("success"));

    if (updated > 0) {
        closeTitleFix();
    }
    return updated > 0;
}

void AppController::fetchDiscogsForSelectedArtist() {
    if (m_selectedArtist.isEmpty()) {
        return;
    }
    const QString samplePath = m_library->sampleTrackPathForArtist(m_selectedArtist);
    const QString folder = m_library->artistFolderForTrack(samplePath);
    if (folder.isEmpty()) {
        return;
    }
    m_discogs->fetchArtist(m_selectedArtist, folder);
}

void AppController::fetchDiscogsForAllArtistAlbums() {
    if (m_selectedArtist.isEmpty() || m_bulkDiscogsAlbumsActive) {
        return;
    }
    if (m_discogs->busy()) {
        notify(QStringLiteral("Discogs is busy"), QStringLiteral("info"));
        return;
    }
    if (!m_discogs->hasToken()) {
        notify(QStringLiteral("Add a Discogs token in Settings first"),
               QStringLiteral("error"));
        return;
    }

    const QStringList albums = m_library->albumsForArtist(m_selectedArtist);
    if (albums.isEmpty()) {
        notify(QStringLiteral("No albums found for this artist"), QStringLiteral("info"));
        return;
    }

    m_bulkDiscogsAlbumsActive = true;
    m_bulkDiscogsArtist = m_selectedArtist;
    m_bulkDiscogsAlbums = albums;
    m_bulkDiscogsCurrentAlbum.clear();
    m_bulkDiscogsIndex = 0;
    m_bulkDiscogsSucceeded = 0;
    m_bulkDiscogsFailed = 0;
    emit albumDiscogsChanged();
    notify(QStringLiteral("Fetching Discogs info for %1 albums by %2")
               .arg(albums.size())
               .arg(m_bulkDiscogsArtist),
           QStringLiteral("info"));
    fetchNextBulkDiscogsAlbum();
}

void AppController::fetchNextBulkDiscogsAlbum() {
    if (!m_bulkDiscogsAlbumsActive) {
        return;
    }

    while (m_bulkDiscogsIndex < m_bulkDiscogsAlbums.size()) {
        m_bulkDiscogsCurrentAlbum = m_bulkDiscogsAlbums.at(m_bulkDiscogsIndex);
        const QVariantList tracks =
            m_library->tracksForAlbum(m_bulkDiscogsArtist, m_bulkDiscogsCurrentAlbum);
        if (!tracks.isEmpty()) {
            notify(QStringLiteral("Discogs %1/%2: searching %3")
                       .arg(m_bulkDiscogsIndex + 1)
                       .arg(m_bulkDiscogsAlbums.size())
                       .arg(m_bulkDiscogsCurrentAlbum),
                   QStringLiteral("info"));
            m_discogs->searchReleases(m_bulkDiscogsArtist, m_bulkDiscogsCurrentAlbum);
            return;
        }
        ++m_bulkDiscogsFailed;
        ++m_bulkDiscogsIndex;
    }

    const int total = m_bulkDiscogsAlbums.size();
    const int succeeded = m_bulkDiscogsSucceeded;
    const int failed = m_bulkDiscogsFailed;
    m_bulkDiscogsAlbumsActive = false;
    m_bulkDiscogsArtist.clear();
    m_bulkDiscogsAlbums.clear();
    m_bulkDiscogsCurrentAlbum.clear();
    emit albumDiscogsChanged();

    notify(failed == 0
               ? QStringLiteral("Fetched Discogs info for all %1 albums").arg(total)
               : QStringLiteral("Discogs album fetch complete: %1 saved, %2 unmatched/failed")
                     .arg(succeeded)
                     .arg(failed),
           failed == 0 ? QStringLiteral("success") : QStringLiteral("info"));
}

void AppController::handleBulkDiscogsSearch(const QVariantList &results) {
    if (!m_bulkDiscogsAlbumsActive || m_bulkDiscogsCurrentAlbum.isEmpty()) {
        return;
    }

    if (results.isEmpty()) {
        ++m_bulkDiscogsFailed;
        ++m_bulkDiscogsIndex;
        QTimer::singleShot(2000, this, &AppController::fetchNextBulkDiscogsAlbum);
        return;
    }

    const quint64 releaseId =
        results.first().toMap().value(QStringLiteral("id")).toULongLong();
    const QVariantList tracks =
        m_library->tracksForAlbum(m_bulkDiscogsArtist, m_bulkDiscogsCurrentAlbum);
    if (releaseId == 0 || tracks.isEmpty()) {
        ++m_bulkDiscogsFailed;
        ++m_bulkDiscogsIndex;
        QTimer::singleShot(2000, this, &AppController::fetchNextBulkDiscogsAlbum);
        return;
    }

    const QString folder =
        QFileInfo(tracks.first().toMap().value(QStringLiteral("path")).toString()).absolutePath();
    m_discogs->fetchRelease(m_bulkDiscogsArtist, m_bulkDiscogsCurrentAlbum, folder, releaseId);
}

void AppController::openDiscogsForSelectedAlbum() {
    if (m_selectedArtist.isEmpty() || m_selectedAlbum.isEmpty()) {
        return;
    }
    m_albumDiscogsCandidates.clear();
    m_albumDiscogsSelectedIndex = -1;
    m_albumDiscogsOpen = true;
    emit albumDiscogsChanged();
    m_discogs->searchReleases(m_selectedArtist, m_selectedAlbum,
                              QStringList{QStringLiteral("CD")}, {});
}

void AppController::searchAlbumDiscogs(const QString &artist, const QString &album,
                                       const QVariantList &formats, const QString &edition) {
    if (!m_albumDiscogsOpen) {
        return;
    }
    if (artist.trimmed().isEmpty() && album.trimmed().isEmpty()
        && edition.trimmed().isEmpty()) {
        notify(QStringLiteral("Enter an artist and/or album to search"), QStringLiteral("info"));
        return;
    }
    QStringList formatFilters;
    for (const QVariant &value : formats) {
        const QString format = value.toString().trimmed();
        if (!format.isEmpty()) {
            formatFilters << format;
        }
    }
    m_albumDiscogsCandidates.clear();
    m_albumDiscogsSelectedIndex = -1;
    emit albumDiscogsChanged();
    m_discogs->searchReleases(artist, album, formatFilters, edition);
}

void AppController::closeDiscogsForSelectedAlbum() {
    if (!m_albumDiscogsOpen) {
        return;
    }
    m_albumDiscogsOpen = false;
    emit albumDiscogsChanged();
}

void AppController::setAlbumDiscogsSelectedIndex(int index) {
    const int bounded = qBound(-1, index, m_albumDiscogsCandidates.size() - 1);
    if (m_albumDiscogsSelectedIndex == bounded) {
        return;
    }
    m_albumDiscogsSelectedIndex = bounded;
    emit albumDiscogsChanged();
}

void AppController::fetchSelectedDiscogsAlbum() {
    if (m_albumDiscogsSelectedIndex < 0
        || m_albumDiscogsSelectedIndex >= m_albumDiscogsCandidates.size()) {
        return;
    }
    const QVariantMap candidate =
        m_albumDiscogsCandidates.at(m_albumDiscogsSelectedIndex).toMap();
    const QVariantList tracks = m_library->tracksForAlbum(m_selectedArtist, m_selectedAlbum);
    if (tracks.isEmpty()) {
        return;
    }
    const QString folder =
        QFileInfo(tracks.first().toMap().value(QStringLiteral("path")).toString()).absolutePath();
    m_discogs->fetchRelease(m_selectedArtist, m_selectedAlbum, folder,
                            candidate.value(QStringLiteral("id")).toULongLong());
}

void AppController::onDiscogsArtistFetched(const QString &artistName, bool success) {
    if (!success || artistName != m_selectedArtist) {
        return;
    }
    const QString samplePath = m_library->sampleTrackPathForArtist(artistName);
    m_selectedArtistMedia->loadForArtist(artistName, samplePath);
}

void AppController::onDiscogsReleaseFetched(const QString &artist, const QString &album,
                                             bool success) {
    if (success && artist == m_selectedArtist && album == m_selectedAlbum) {
        const QVariantList tracks = m_library->tracksForAlbum(artist, album);
        if (!tracks.isEmpty()) {
            m_selectedAlbumArtUrl =
                m_nowPlaying->albumArtForTrack(tracks.first().toMap()
                                                   .value(QStringLiteral("path")).toString());
            QFile infoFile(QFileInfo(tracks.first().toMap().value(QStringLiteral("path"))
                                         .toString()).absolutePath()
                           + QStringLiteral("/album-info.txt"));
            if (infoFile.open(QIODevice::ReadOnly | QIODevice::Text))
                m_selectedAlbumInfo = QString::fromUtf8(infoFile.readAll()).trimmed();
        }
        emit selectionChanged();
    }
    closeDiscogsForSelectedAlbum();

    if (m_bulkDiscogsAlbumsActive && artist == m_bulkDiscogsArtist
        && album == m_bulkDiscogsCurrentAlbum) {
        if (success) {
            ++m_bulkDiscogsSucceeded;
        } else {
            ++m_bulkDiscogsFailed;
        }
        ++m_bulkDiscogsIndex;
        QTimer::singleShot(2000, this, &AppController::fetchNextBulkDiscogsAlbum);
    }
}

void AppController::applyUiFont() {
    QFont font = QGuiApplication::font();
    const QString preferred = m_config->uiFontFamily();
    QString family = preferred;
    if (preferred.isEmpty() || !QFontDatabase::hasFamily(preferred)) {
        const QStringList fallbacks = {
            QStringLiteral("CaskaydiaMono Nerd Font"),
            QStringLiteral("JetBrainsMono Nerd Font"),
            QStringLiteral("JetBrains Mono"),
            QStringLiteral("Sans Serif"),
        };
        family.clear();
        for (const QString &candidate : fallbacks) {
            if (QFontDatabase::hasFamily(candidate)) {
                family = candidate;
                break;
            }
        }
        if (family.isEmpty()) {
            family = QFont().defaultFamily();
        }
    }
    font.setFamily(family);
    if (m_config->uiFontSize() > 0) {
        font.setPointSize(m_config->uiFontSize());
    }
    QGuiApplication::setFont(font);
}

void AppController::applyLibraryPaths(const QString &paths) {
    const QStringList before = m_config->libraryPaths();
    m_config->setLibraryPaths(paths);
    if (m_config->libraryPaths() == before) {
        return;
    }
    m_config->save();
    m_library->ensureLibrary(m_config->libraryPaths());
    refreshArtists();
    reloadPlaylistsIfReady();
    notify(QStringLiteral("Music folders updated"), QStringLiteral("success"));
}

void AppController::applyImportInbox(const QString &path) {
    const QString before = m_config->importInbox();
    m_config->setImportInbox(path);
    if (m_config->importInbox() != before) {
        m_config->save();
    }
}

void AppController::applyLyricsDir(const QString &path) {
    const QString before = m_config->lyricsDir();
    m_config->setLyricsDir(path);
    if (m_config->lyricsDir() != before) {
        m_config->save();
    }
}

void AppController::applyScanning(bool scanOnLaunch, bool watchEnabled) {
    m_config->setScanOnLaunch(scanOnLaunch);
    m_config->setLibraryWatchEnabled(watchEnabled);
    m_config->save();
    m_library->setScanOnLaunch(scanOnLaunch);
    m_library->setWatchEnabled(watchEnabled);
}

void AppController::applyUiFontSettings(const QString &family, int size) {
    const QString trimmed = family.trimmed();
    if (!trimmed.isEmpty()) {
        m_config->setUiFontFamily(trimmed);
    }
    m_config->setUiFontSize(size);
    m_config->save();
    applyUiFont();
}

void AppController::applyInteraction(bool wasdNavigation, bool tooltipsEnabled) {
    m_config->setWasdNavigation(wasdNavigation);
    m_config->setTooltipsEnabled(tooltipsEnabled);
    m_config->save();
}

void AppController::applyLyricsProviders(bool netease, bool plain) {
    m_config->setLyricsNeteaseEnabled(netease);
    m_config->setLyricsPlainEnabled(plain);
    m_config->save();
}

void AppController::applyConvertOptions(int opusBitrateKbps, bool deleteSource) {
    m_config->setOpusBitrateKbps(opusBitrateKbps);
    m_config->setConvertDeleteSource(deleteSource);
    m_config->save();
}

void AppController::applyDiscogsToken(const QString &token) {
    const QString trimmed = token.trimmed();
    m_discogs->setToken(trimmed);
    notify(trimmed.isEmpty() ? QStringLiteral("Discogs token cleared")
                             : QStringLiteral("Discogs token saved"),
           QStringLiteral("success"));
}

void AppController::setDacPassthrough(bool enabled) {
    m_config->setDacPassthrough(enabled);
    m_playback->setDacPassthrough(enabled);
    if (enabled) {
        m_config->setVolume(100);
        if (!m_playback->audioDevice().isEmpty()) {
            m_config->setAudioDevice(m_playback->audioDevice());
        }
    } else {
        m_config->setAudioDevice(m_playback->audioDevice());
    }
    m_config->save();
}

void AppController::fetchLyricsForPlayingTrack() {
    const QString path = m_playback->currentPath();
    if (path.isEmpty()) {
        return;
    }
    QVariantMap track;
    track.insert(QStringLiteral("path"), path);
    track.insert(QStringLiteral("artist"), m_playback->artist());
    track.insert(QStringLiteral("title"), m_playback->title());
    track.insert(QStringLiteral("album"), m_playback->album());
    m_lyrics->retryForTrack(track);
}

void AppController::fetchLyricsForTrackIndex(int index) {
    const QVariantMap track = m_tracks->trackAt(index);
    if (track.isEmpty() || track.value(QStringLiteral("path")).toString().isEmpty()) {
        return;
    }
    m_lyrics->retryForTrack(track);
}

void AppController::fetchLyricsForAlbum(const QString &artist, const QString &album) {
    if (artist.isEmpty() || album.isEmpty()) {
        return;
    }
    const QVariantList tracks = m_library->tracksForAlbum(artist, album);
    if (tracks.isEmpty()) {
        return;
    }
    m_lyrics->fetchForTracks(tracks);
}

void AppController::fetchLyricsForArtist(const QString &artist) {
    if (artist.isEmpty()) {
        return;
    }
    const QVariantList tracks = m_library->tracksForArtist(artist);
    if (tracks.isEmpty()) {
        return;
    }
    m_lyrics->fetchForTracks(tracks);
}

void AppController::fetchLyricsForLibrary() {
    if (m_lyrics->busy()) {
        notify(QStringLiteral("Lyrics fetch is already running"), QStringLiteral("info"));
        return;
    }
    if (m_enrichment && m_enrichment->active()) {
        notify(QStringLiteral("Library enrichment is already running"), QStringLiteral("info"));
        return;
    }
    const QVariantList tracks = m_library->allTracks();
    if (tracks.isEmpty()) {
        notify(QStringLiteral("Library is empty — nothing to fetch"), QStringLiteral("info"));
        return;
    }
    notify(QStringLiteral("Fetching lyrics for %1 tracks").arg(tracks.size()),
           QStringLiteral("info"));
    m_lyrics->fetchForTracks(tracks);
}
