#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

#include "ConfigService.h"
#include "LibraryService.h"
#include "PlaybackService.h"
#include "ArtistMediaService.h"
#include "PlaylistService.h"
#include "TrackMediaService.h"
#include "TagService.h"
#include "DiscogsService.h"
#include "BeetsService.h"
#include "ImportService.h"
#include "MetadataSearchService.h"
#include "../models/ArtistModel.h"
#include "../models/PlaylistListModel.h"
#include "../models/TrackListModel.h"

class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(ConfigService *config READ config CONSTANT)
    Q_PROPERTY(LibraryService *library READ library CONSTANT)
    Q_PROPERTY(PlaybackService *playback READ playback CONSTANT)
    Q_PROPERTY(TrackMediaService *nowPlaying READ nowPlaying CONSTANT)
    Q_PROPERTY(ArtistMediaService *selectedArtistMedia READ selectedArtistMedia CONSTANT)
    Q_PROPERTY(PlaylistService *playlists READ playlists CONSTANT)
    Q_PROPERTY(PlaylistListModel *playlistItems READ playlistItems CONSTANT)
    Q_PROPERTY(TrackListModel *playlistTracks READ playlistTracks CONSTANT)
    Q_PROPERTY(TagService *tags READ tags CONSTANT)
    Q_PROPERTY(DiscogsService *discogs READ discogs CONSTANT)
    Q_PROPERTY(BeetsService *beets READ beets CONSTANT)
    Q_PROPERTY(ImportService *importInbox READ importInbox CONSTANT)
    Q_PROPERTY(MetadataSearchService *metadataSearch READ metadataSearch CONSTANT)
    Q_PROPERTY(bool tagFetchOpen READ tagFetchOpen NOTIFY tagFetchChanged)
    Q_PROPERTY(QString mainView READ mainView NOTIFY mainViewChanged)
    Q_PROPERTY(ArtistModel *artists READ artists CONSTANT)
    Q_PROPERTY(TrackListModel *tracks READ tracks CONSTANT)
    Q_PROPERTY(QStringList albums READ albums NOTIFY albumsChanged)
    Q_PROPERTY(QString selectedArtist READ selectedArtist NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedAlbum READ selectedAlbum NOTIFY selectionChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanStatusChanged)
    Q_PROPERTY(bool tagEditorOpen READ tagEditorOpen NOTIFY tagEditorChanged)
    Q_PROPERTY(QVariantMap tagEditorFields READ tagEditorFields NOTIFY tagEditorChanged)
    Q_PROPERTY(QString tagEditorMode READ tagEditorMode NOTIFY tagEditorChanged)
    Q_PROPERTY(QString tagEditorTitle READ tagEditorTitle NOTIFY tagEditorChanged)
    Q_PROPERTY(bool librarySearchOpen READ librarySearchOpen NOTIFY librarySearchOpenChanged)
    Q_PROPERTY(QString librarySearchQuery READ librarySearchQuery NOTIFY librarySearchChanged)
    Q_PROPERTY(QString librarySearchScope READ librarySearchScope NOTIFY librarySearchChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    ConfigService *config() const { return m_config; }
    LibraryService *library() const { return m_library; }
    PlaybackService *playback() const { return m_playback; }
    TrackMediaService *nowPlaying() const { return m_nowPlaying; }
    ArtistMediaService *selectedArtistMedia() const { return m_selectedArtistMedia; }
    PlaylistService *playlists() const { return m_playlists; }
    PlaylistListModel *playlistItems() const { return m_playlistItems; }
    TrackListModel *playlistTracks() const { return m_playlistTracks; }
    TagService *tags() const { return m_tags; }
    DiscogsService *discogs() const { return m_discogs; }
    BeetsService *beets() const { return m_beets; }
    ImportService *importInbox() const { return m_importInbox; }
    MetadataSearchService *metadataSearch() const { return m_metadataSearch; }
    bool tagFetchOpen() const { return m_tagFetchOpen; }
    QString mainView() const { return m_mainView; }
    ArtistModel *artists() const { return m_artists; }
    TrackListModel *tracks() const { return m_tracks; }
    QStringList albums() const { return m_albums; }
    QString selectedArtist() const { return m_selectedArtist; }
    QString selectedAlbum() const { return m_selectedAlbum; }
    QString scanStatus() const { return m_library->scanStatus(); }
    bool tagEditorOpen() const { return m_tagEditorOpen; }
    QVariantMap tagEditorFields() const { return m_tagEditorFields; }
    QString tagEditorMode() const { return m_tagEditorMode; }
    QString tagEditorTitle() const { return m_tagEditorTitle; }
    bool librarySearchOpen() const { return m_librarySearchOpen; }
    QString librarySearchQuery() const { return m_librarySearchQuery; }
    QString librarySearchScope() const { return m_librarySearchScope; }

    Q_INVOKABLE void initialize();
    Q_INVOKABLE void saveOnExit();
    Q_INVOKABLE void selectArtist(const QString &artist);
    Q_INVOKABLE void selectAlbum(const QString &album);
    Q_INVOKABLE void playTrackIndex(int index);
    Q_INVOKABLE void playAlbum();
    Q_INVOKABLE void openLibrarySearch();
    Q_INVOKABLE void closeLibrarySearch();
    Q_INVOKABLE void setLibrarySearchQuery(const QString &query);
    Q_INVOKABLE void setLibrarySearchScope(const QString &scope);
    Q_INVOKABLE void rescanLibrary();
    Q_INVOKABLE void showLibrary();
    Q_INVOKABLE void showPlaylists();
    Q_INVOKABLE void showSettings();
    Q_INVOKABLE void showImport();
    Q_INVOKABLE void selectPlaylist(const QString &name);
    Q_INVOKABLE void playPlaylistTrackIndex(int index);
    Q_INVOKABLE void playPlaylist();
    Q_INVOKABLE void createPlaylist(const QString &name);
    Q_INVOKABLE void deleteSelectedPlaylist();
    Q_INVOKABLE void openTrackTagEditor(int index);
    Q_INVOKABLE void openAlbumTagEditor();
    Q_INVOKABLE void openArtistTagEditor();
    Q_INVOKABLE void closeTagEditor();
    Q_INVOKABLE bool saveTagEditor(const QVariantMap &fields);
    Q_INVOKABLE void openTagFetch(const QVariantMap &editorFields = {});
    Q_INVOKABLE void closeTagFetch();
    Q_INVOKABLE bool applyTagFetch(bool syncBeets);
    Q_INVOKABLE void fetchDiscogsForSelectedArtist();
    Q_INVOKABLE void saveSettings(const QString &importInbox, const QString &beetsBinary,
                                  bool beetsNomove, const QString &discogsToken,
                                  const QString &uiFontFamily = {});

signals:
    void albumsChanged();
    void selectionChanged();
    void scanStatusChanged();
    void mainViewChanged();
    void tagEditorChanged();
    void tagFetchChanged();
    void librarySearchChanged();
    void librarySearchOpenChanged();
    void librarySearchFocusRequested();

private:
    void refreshArtists();
    void refreshAlbums();
    void refreshTracks();
    void refreshPlaylistItems();
    void refreshPlaylistTracks();
    void reloadPlaylistsIfReady();
    void onTrackChanged();
    void onDiscogsArtistFetched(const QString &artistName, bool success);
    void syncFetchedTagsToBeets(const QVariantMap &fields, const QString &libraryArtist,
                                const QString &libraryAlbum);
    void clearLibrarySelection();
    void applyUiFont();
    void setTagEditor(bool open, const QString &mode, const QString &title,
                      const QVariantMap &fields, const QString &path = {});

    ConfigService *m_config = nullptr;
    LibraryService *m_library = nullptr;
    PlaybackService *m_playback = nullptr;
    TrackMediaService *m_nowPlaying = nullptr;
    ArtistMediaService *m_selectedArtistMedia = nullptr;
    PlaylistService *m_playlists = nullptr;
    ArtistModel *m_artists = nullptr;
    TrackListModel *m_tracks = nullptr;
    PlaylistListModel *m_playlistItems = nullptr;
    TrackListModel *m_playlistTracks = nullptr;
    TagService *m_tags = nullptr;
    DiscogsService *m_discogs = nullptr;
    BeetsService *m_beets = nullptr;
    ImportService *m_importInbox = nullptr;
    MetadataSearchService *m_metadataSearch = nullptr;

    QStringList m_albums;
    QString m_selectedArtist;
    QString m_selectedAlbum;
    QString m_mainView = QStringLiteral("library");
    bool m_pendingPlaylistReload = false;

    bool m_tagEditorOpen = false;
    QVariantMap m_tagEditorFields;
    QString m_tagEditorMode;
    QString m_tagEditorTitle;
    QString m_tagEditorPath;
    bool m_tagFetchOpen = false;

    bool m_librarySearchOpen = false;
    QString m_librarySearchQuery;
    QString m_librarySearchScope = QStringLiteral("artists");
};
