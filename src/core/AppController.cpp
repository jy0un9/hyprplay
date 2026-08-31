#include "AppController.h"

#include "FuzzyMatch.h"

#include <QFont>
#include <QGuiApplication>

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
    m_metadataSearch = new MetadataSearchService(m_config, m_discogs, this);
    m_artists = new ArtistModel(this);
    m_tracks = new TrackListModel(this);
    m_playlistItems = new PlaylistListModel(this);
    m_playlistTracks = new TrackListModel(this);

    connect(m_library, &LibraryService::scanFinished, this, &AppController::refreshArtists);
    connect(m_library, &LibraryService::scanStatusChanged, this, &AppController::scanStatusChanged);
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
    connect(m_tags, &TagService::tagsSaved, this, [this]() {
        refreshArtists();
        refreshTracks();
    });
    connect(m_config, &ConfigService::configChanged, this, &AppController::applyUiFont);
}

void AppController::initialize() {
    m_config->load();
    if (!m_playback->ensureMpv()) {
        qWarning("Audio engine failed to start — playback will not work until mpv initializes");
    }
    m_playback->setVolume(m_config->volume());
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

void AppController::clearLibrarySelection() {
    m_selectedArtist.clear();
    m_selectedAlbum.clear();
    m_albums.clear();
    m_tracks->setTracks({});
    m_selectedArtistMedia->clearMedia();
    emit albumsChanged();
    emit selectionChanged();
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
        return;
    }
    m_selectedArtist = artist;
    m_selectedAlbum.clear();
    emit selectionChanged();
    refreshAlbums();
    m_tracks->setTracks({});

    const QString samplePath = m_library->sampleTrackPathForArtist(artist);
    m_selectedArtistMedia->loadForArtist(artist, samplePath);
}

void AppController::selectAlbum(const QString &album) {
    if (m_selectedAlbum == album) {
        return;
    }
    m_selectedAlbum = album;
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
}

void AppController::playTrackIndex(int index) {
    const QVariantMap track = m_tracks->trackAt(index);
    if (track.isEmpty()) {
        return;
    }

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

void AppController::onDiscogsArtistFetched(const QString &artistName, bool success) {
    if (!success || artistName != m_selectedArtist) {
        return;
    }
    const QString samplePath = m_library->sampleTrackPathForArtist(artistName);
    m_selectedArtistMedia->loadForArtist(artistName, samplePath);
}

void AppController::applyUiFont() {
    QFont font = QGuiApplication::font();
    font.setFamily(m_config->uiFontFamily());
    QGuiApplication::setFont(font);
}

void AppController::saveSettings(const QString &importInbox, const QString &beetsBinary,
                                   bool beetsNomove, const QString &discogsToken,
                                   const QString &uiFontFamily) {
    m_config->setImportInbox(importInbox);
    m_config->setBeetsBinary(beetsBinary);
    m_config->setBeetsNomove(beetsNomove);
    if (!uiFontFamily.trimmed().isEmpty()) {
        m_config->setUiFontFamily(uiFontFamily.trimmed());
    }
    if (!discogsToken.trimmed().isEmpty()) {
        m_discogs->setToken(discogsToken.trimmed());
    }
    m_config->save();
    applyUiFont();
}
