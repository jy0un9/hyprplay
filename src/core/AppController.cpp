#include "AppController.h"

#include "FuzzyMatch.h"

#include <QColor>
#include <QFont>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSet>

AppController::AppController(QObject *parent) : QObject(parent) {
    m_config = new ConfigService(this);
    m_library = new LibraryService(this);
    m_playback = new PlaybackService(this);
    m_nowPlaying = new TrackMediaService(m_config, this);
    m_selectedArtistMedia = new ArtistMediaService(m_config, this);
    m_playlists = new PlaylistService(m_config, m_library, this);
    m_tags = new TagService(m_library, this);
    m_discogs = new DiscogsService(m_config, this);
    m_beets = new BeetsService(m_config, this);
    m_importInbox = new ImportService(m_config, m_library, m_tags, m_beets, this);
    m_lyrics = new LyricsService(m_config, this);
    m_metadataSearch = new MetadataSearchService(m_config, m_discogs, this);
    m_artists = new ArtistModel(this);
    m_tracks = new TrackListModel(this);
    m_playlistItems = new PlaylistListModel(this);
    m_playlistTracks = new TrackListModel(this);

    connect(m_library, &LibraryService::scanFinished, this, &AppController::refreshArtists);
    connect(m_library, &LibraryService::scanStatusChanged, this, &AppController::scanStatusChanged);
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
    connect(m_beets, &BeetsService::statusChanged, this, [this]() {
        const QString status = m_beets->status();
        if (status.isEmpty() || m_beets->busy()) {
            return;
        }
        const QString lowered = status.toLower();
        const bool isError = lowered.contains(QStringLiteral("fail"))
                             || lowered.contains(QStringLiteral("error"))
                             || lowered.contains(QStringLiteral("timed out"))
                             || lowered.contains(QStringLiteral("not found"));
        if (isError) {
            notify(QStringLiteral("beets: ") + status, QStringLiteral("error"));
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
    if (!m_playback->ensureMpv()) {
        qWarning("Audio engine failed to start — playback will not work until mpv initializes");
    }
    m_playback->setVolume(m_config->volume());
    m_library->setScanOnLaunch(m_config->scanOnLaunch());
    m_library->ensureLibrary(m_config->libraryPaths());
    applyUiFont();
    refreshArtists();
    reloadPlaylistsIfReady();
}

void AppController::saveOnExit() {
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
    reloadPlaylistsIfReady();
}

void AppController::showSettings() {
    if (m_mainView != QStringLiteral("settings")) {
        m_mainView = QStringLiteral("settings");
        emit mainViewChanged();
    }
}

void AppController::showImport() {
    if (m_mainView != QStringLiteral("import")) {
        m_mainView = QStringLiteral("import");
        emit mainViewChanged();
    }
    m_importInbox->scanInbox();
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
        setSelectedTrackIndex((current - 1 + count) % count);
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
        setSelectedTrackIndex((current + 1) % count);
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
            playTrackIndex(m_selectedTrackIndex);
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

void AppController::clearLibrarySelection() {
    m_selectedArtist.clear();
    m_selectedAlbum.clear();
    m_selectedAlbumArtUrl.clear();
    m_selectedAlbumInfo.clear();
    m_albums.clear();
    m_selectedTrackIndex = -1;
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

void AppController::openTagFetch(const QVariantMap &editorFields) {
    QString artist;
    QString album;
    QString albumArtist;

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
    m_metadataSearch->searchRelease(artist, album, albumArtist);
}

void AppController::closeTagFetch() {
    if (!m_tagFetchOpen) {
        return;
    }
    m_tagFetchOpen = false;
    m_metadataSearch->clear();
    emit tagFetchChanged();
}

bool AppController::applyTagFetch(bool syncBeets) {
    const QVariantMap allFields = m_metadataSearch->checkedFields();
    if (allFields.isEmpty()) {
        return false;
    }

    QVariantMap fileFields = allFields;
    fileFields.remove(QStringLiteral("mb_albumid"));

    const QString libraryArtist = m_tagEditorMode == QStringLiteral("track")
                                      ? m_tagEditorFields.value(QStringLiteral("artist")).toString()
                                      : m_selectedArtist;
    const QString libraryAlbum = m_tagEditorMode == QStringLiteral("track")
                                     ? m_tagEditorFields.value(QStringLiteral("album")).toString()
                                     : m_selectedAlbum;

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
        if (syncBeets && m_beets->available()) {
            syncFetchedTagsToBeets(allFields, libraryArtist, libraryAlbum);
        }
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

void AppController::syncFetchedTagsToBeets(const QVariantMap &fields, const QString &libraryArtist,
                                           const QString &libraryAlbum) {
    if (m_tagEditorMode == QStringLiteral("track") && !m_tagEditorPath.isEmpty()) {
        m_beets->modifyByPath(m_tagEditorPath, fields);
        return;
    }

    const QStringList paths =
        m_tagEditorMode == QStringLiteral("artist")
            ? m_tags->artistTagPaths(libraryArtist)
            : m_tags->albumTagPaths(libraryArtist, libraryAlbum);

    if (m_tagEditorMode == QStringLiteral("album") && !libraryArtist.isEmpty() && !libraryAlbum.isEmpty()) {
        m_beets->syncAlbumTags(libraryArtist, libraryAlbum, fields, paths);
    } else if (m_tagEditorMode == QStringLiteral("artist") && !libraryArtist.isEmpty()) {
        m_beets->syncArtistTags(libraryArtist, fields, paths);
    }
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

void AppController::openDiscogsForSelectedAlbum() {
    if (m_selectedArtist.isEmpty() || m_selectedAlbum.isEmpty()) {
        return;
    }
    m_albumDiscogsCandidates = m_discogs->searchReleases(m_selectedArtist, m_selectedAlbum);
    m_albumDiscogsSelectedIndex =
        m_albumDiscogsCandidates.isEmpty() ? -1 : 0;
    m_albumDiscogsOpen = true;
    emit albumDiscogsChanged();
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
}

void AppController::applyUiFont() {
    QFont font = QGuiApplication::font();
    font.setFamily(m_config->uiFontFamily());
    QGuiApplication::setFont(font);
}

void AppController::saveSettings(const QString &libraryPaths, const QString &importInbox,
                                   const QString &lyricsDir, const QString &beetsBinary,
                                   bool beetsNomove, const QString &discogsToken,
                                   const QString &uiFontFamily, int uiFontSize, bool scanOnLaunch,
                                   bool wasdNavigation, bool tooltipsEnabled, bool lyricsNetease,
                                   bool lyricsPlain) {
    m_config->setLibraryPaths(libraryPaths);
    m_config->setImportInbox(importInbox);
    m_config->setLyricsDir(lyricsDir);
    m_config->setBeetsBinary(beetsBinary);
    m_config->setBeetsNomove(beetsNomove);
    if (!uiFontFamily.trimmed().isEmpty()) {
        m_config->setUiFontFamily(uiFontFamily.trimmed());
    }
    m_config->setUiFontSize(uiFontSize);
    m_config->setScanOnLaunch(scanOnLaunch);
    m_config->setWasdNavigation(wasdNavigation);
    m_config->setTooltipsEnabled(tooltipsEnabled);
    m_config->setLyricsNeteaseEnabled(lyricsNetease);
    m_config->setLyricsPlainEnabled(lyricsPlain);
    if (!discogsToken.trimmed().isEmpty()) {
        m_discogs->setToken(discogsToken.trimmed());
    }
    m_config->save();
    applyUiFont();
    m_library->setScanOnLaunch(m_config->scanOnLaunch());
    m_library->ensureLibrary(m_config->libraryPaths());
    refreshArtists();
    reloadPlaylistsIfReady();
    notify(QStringLiteral("Settings saved"), QStringLiteral("success"));
}

void AppController::setDacPassthrough(bool enabled) {
    m_config->setDacPassthrough(enabled);
    m_playback->setDacPassthrough(enabled);
    if (enabled) {
        m_config->setVolume(100);
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
    m_lyrics->fetchForTrack(track);
}

void AppController::fetchLyricsForTrackIndex(int index) {
    const QVariantMap track = m_tracks->trackAt(index);
    if (track.isEmpty() || track.value(QStringLiteral("path")).toString().isEmpty()) {
        return;
    }
    m_lyrics->fetchForTrack(track);
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
