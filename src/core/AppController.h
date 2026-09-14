#pragma once

#include <QObject>
#include <QPoint>
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
#include "LyricsService.h"
#include "MetadataSearchService.h"
#include "LibraryEnrichmentService.h"
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
    Q_PROPERTY(LyricsService *lyrics READ lyrics CONSTANT)
    Q_PROPERTY(MetadataSearchService *metadataSearch READ metadataSearch CONSTANT)
    Q_PROPERTY(LibraryEnrichmentService *enrichment READ enrichment CONSTANT)
    Q_PROPERTY(bool tagFetchOpen READ tagFetchOpen NOTIFY tagFetchChanged)
    Q_PROPERTY(bool titleFixOpen READ titleFixOpen NOTIFY titleFixChanged)
    Q_PROPERTY(QString mainView READ mainView NOTIFY mainViewChanged)
    Q_PROPERTY(ArtistModel *artists READ artists CONSTANT)
    Q_PROPERTY(TrackListModel *tracks READ tracks CONSTANT)
    Q_PROPERTY(QStringList albums READ albums NOTIFY albumsChanged)
    Q_PROPERTY(QString selectedArtist READ selectedArtist NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedAlbum READ selectedAlbum NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedAlbumArtUrl READ selectedAlbumArtUrl NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedAlbumInfo READ selectedAlbumInfo NOTIFY selectionChanged)
    Q_PROPERTY(int selectedAlbumTrackCount READ selectedAlbumTrackCount NOTIFY selectionChanged)
    Q_PROPERTY(bool nowPlayingFocused READ nowPlayingFocused NOTIFY selectionChanged)
    Q_PROPERTY(bool albumDiscogsOpen READ albumDiscogsOpen NOTIFY albumDiscogsChanged)
    Q_PROPERTY(QVariantList albumDiscogsCandidates READ albumDiscogsCandidates NOTIFY albumDiscogsChanged)
    Q_PROPERTY(int albumDiscogsSelectedIndex READ albumDiscogsSelectedIndex
               WRITE setAlbumDiscogsSelectedIndex NOTIFY albumDiscogsChanged)
    Q_PROPERTY(bool bulkDiscogsAlbumsActive READ bulkDiscogsAlbumsActive
               NOTIFY albumDiscogsChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanStatusChanged)
    Q_PROPERTY(bool tagEditorOpen READ tagEditorOpen NOTIFY tagEditorChanged)
    Q_PROPERTY(QVariantMap tagEditorFields READ tagEditorFields NOTIFY tagEditorChanged)
    Q_PROPERTY(QString tagEditorMode READ tagEditorMode NOTIFY tagEditorChanged)
    Q_PROPERTY(QString tagEditorTitle READ tagEditorTitle NOTIFY tagEditorChanged)
    Q_PROPERTY(bool librarySearchOpen READ librarySearchOpen NOTIFY librarySearchOpenChanged)
    Q_PROPERTY(QString librarySearchQuery READ librarySearchQuery NOTIFY librarySearchChanged)
    Q_PROPERTY(QString librarySearchScope READ librarySearchScope NOTIFY librarySearchChanged)
    Q_PROPERTY(QString libraryFocusColumn READ libraryFocusColumn NOTIFY libraryFocusChanged)
    Q_PROPERTY(int selectedTrackIndex READ selectedTrackIndex NOTIFY selectionChanged)
    Q_PROPERTY(int multiSelectedTrackCount READ multiSelectedTrackCount NOTIFY multiSelectionChanged)
    Q_PROPERTY(QVariantList multiSelectedTrackIndices READ multiSelectedTrackIndices NOTIFY multiSelectionChanged)
    Q_PROPERTY(QString playlistFocusColumn READ playlistFocusColumn NOTIFY playlistFocusChanged)
    Q_PROPERTY(int selectedPlaylistTrackIndex READ selectedPlaylistTrackIndex NOTIFY playlistFocusChanged)
    Q_PROPERTY(QString noticeText READ noticeText NOTIFY noticeChanged)
    Q_PROPERTY(QString noticeKind READ noticeKind NOTIFY noticeChanged)
    Q_PROPERTY(int noticeSerial READ noticeSerial NOTIFY noticeChanged)

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
    LyricsService *lyrics() const { return m_lyrics; }
    MetadataSearchService *metadataSearch() const { return m_metadataSearch; }
    LibraryEnrichmentService *enrichment() const { return m_enrichment; }
    bool tagFetchOpen() const { return m_tagFetchOpen; }
    bool titleFixOpen() const { return m_titleFixOpen; }
    QString mainView() const { return m_mainView; }
    ArtistModel *artists() const { return m_artists; }
    TrackListModel *tracks() const { return m_tracks; }
    QStringList albums() const { return m_albums; }
    QString selectedArtist() const { return m_selectedArtist; }
    QString selectedAlbum() const { return m_selectedAlbum; }
    QString selectedAlbumArtUrl() const { return m_selectedAlbumArtUrl; }
    QString selectedAlbumInfo() const { return m_selectedAlbumInfo; }
    int selectedAlbumTrackCount() const;
    bool nowPlayingFocused() const { return m_nowPlayingFocused; }
    bool albumDiscogsOpen() const { return m_albumDiscogsOpen; }
    QVariantList albumDiscogsCandidates() const { return m_albumDiscogsCandidates; }
    int albumDiscogsSelectedIndex() const { return m_albumDiscogsSelectedIndex; }
    bool bulkDiscogsAlbumsActive() const { return m_bulkDiscogsAlbumsActive; }
    QString scanStatus() const { return m_library->scanStatus(); }
    bool tagEditorOpen() const { return m_tagEditorOpen; }
    QVariantMap tagEditorFields() const { return m_tagEditorFields; }
    QString tagEditorMode() const { return m_tagEditorMode; }
    QString tagEditorTitle() const { return m_tagEditorTitle; }
    bool librarySearchOpen() const { return m_librarySearchOpen; }
    QString librarySearchQuery() const { return m_librarySearchQuery; }
    QString librarySearchScope() const { return m_librarySearchScope; }
    QString libraryFocusColumn() const { return m_libraryFocusColumn; }
    int selectedTrackIndex() const { return m_selectedTrackIndex; }
    int multiSelectedTrackCount() const { return m_multiSelectedTracks.size(); }
    QVariantList multiSelectedTrackIndices() const;
    QString playlistFocusColumn() const { return m_playlistFocusColumn; }
    int selectedPlaylistTrackIndex() const { return m_selectedPlaylistTrackIndex; }
    QString noticeText() const { return m_noticeText; }
    QString noticeKind() const { return m_noticeKind; }
    int noticeSerial() const { return m_noticeSerial; }

    Q_INVOKABLE void initialize();
    Q_INVOKABLE void saveOnExit();
    Q_INVOKABLE void selectArtist(const QString &artist);
    Q_INVOKABLE void selectAlbum(const QString &album);
    Q_INVOKABLE void playTrackIndex(int index);
    Q_INVOKABLE void playAlbum();
    Q_INVOKABLE void openLibrarySearch();
    Q_INVOKABLE void closeLibrarySearch();
    Q_INVOKABLE void acceptLibrarySearch();
    Q_INVOKABLE void librarySearchMoveUp();
    Q_INVOKABLE void librarySearchMoveDown();
    Q_INVOKABLE void setLibrarySearchQuery(const QString &query);
    Q_INVOKABLE void setLibrarySearchScope(const QString &scope);
    Q_INVOKABLE void cycleLibrarySearchScope();
    Q_INVOKABLE QString highlightSearchMatch(const QString &text, const QString &accentColor) const;
    Q_INVOKABLE QPoint cursorPos() const;
    Q_INVOKABLE void rescanLibrary();
    Q_INVOKABLE void showLibrary();
    Q_INVOKABLE void showPlaylists();
    Q_INVOKABLE void showSettings();
    Q_INVOKABLE void libraryMoveUp();
    Q_INVOKABLE void libraryMoveDown();
    Q_INVOKABLE void libraryMoveLeft();
    Q_INVOKABLE void libraryMoveRight();
    Q_INVOKABLE void setLibraryFocusColumn(const QString &column);
    Q_INVOKABLE void setSelectedTrackIndex(int index);
    Q_INVOKABLE bool isTrackMultiSelected(int index) const;
    Q_INVOKABLE void handleTrackClick(int index, int modifiers);
    Q_INVOKABLE void prepareTrackContextMenu(int index);
    Q_INVOKABLE void selectAllVisibleTracks();
    Q_INVOKABLE void clearMultiTrackSelection();
    Q_INVOKABLE void playlistMoveUp();
    Q_INVOKABLE void playlistMoveDown();
    Q_INVOKABLE void playlistMoveLeft();
    Q_INVOKABLE void playlistMoveRight();
    Q_INVOKABLE void setPlaylistFocusColumn(const QString &column);
    Q_INVOKABLE void setSelectedPlaylistTrackIndex(int index);
    Q_INVOKABLE void notify(const QString &text, const QString &kind = {});
    Q_INVOKABLE void selectPlaylist(const QString &name);
    Q_INVOKABLE void playPlaylistTrackIndex(int index);
    Q_INVOKABLE void playPlaylist();
    Q_INVOKABLE void createPlaylist(const QString &name);
    Q_INVOKABLE void deleteSelectedPlaylist();
    Q_INVOKABLE void renameSelectedPlaylist(const QString &name);
    Q_INVOKABLE void removePlaylistTrack(int index);
    Q_INVOKABLE void movePlaylistTrack(int fromIndex, int toIndex);
    Q_INVOKABLE void addTrackToPlaylist(int trackIndex, const QString &playlistName);
    Q_INVOKABLE void addSelectedTracksToPlaylist(const QString &playlistName);
    Q_INVOKABLE void addAlbumToPlaylist(const QString &playlistName);
    Q_INVOKABLE void openTrackTagEditor(int index);
    Q_INVOKABLE void openAlbumTagEditor();
    Q_INVOKABLE void openArtistTagEditor();
    Q_INVOKABLE void closeTagEditor();
    Q_INVOKABLE bool saveTagEditor(const QVariantMap &fields);
    Q_INVOKABLE void openTagFetch(const QVariantMap &editorFields = {});
    Q_INVOKABLE void closeTagFetch();
    Q_INVOKABLE bool applyTagFetch(bool syncBeets);
    Q_INVOKABLE void openTitleFix();
    Q_INVOKABLE void closeTitleFix();
    Q_INVOKABLE bool applyTitleFix(bool syncBeets);
    Q_INVOKABLE void fetchDiscogsForSelectedArtist();
    Q_INVOKABLE void fetchDiscogsForAllArtistAlbums();
    Q_INVOKABLE void openDiscogsForSelectedAlbum();
    Q_INVOKABLE void searchAlbumDiscogs(const QString &artist, const QString &album,
                                        const QVariantList &formats = {},
                                        const QString &edition = {});
    Q_INVOKABLE void closeDiscogsForSelectedAlbum();
    Q_INVOKABLE void fetchSelectedDiscogsAlbum();
    Q_INVOKABLE void setAlbumDiscogsSelectedIndex(int index);
    Q_INVOKABLE void saveSettings(const QString &libraryPaths, const QString &importInbox,
                                  const QString &lyricsDir, const QString &beetsBinary,
                                  bool beetsNomove, const QString &discogsToken,
                                  const QString &uiFontFamily, int uiFontSize, bool scanOnLaunch,
                                  bool libraryWatchEnabled, bool wasdNavigation,
                                  bool tooltipsEnabled, bool lyricsNetease, bool lyricsPlain,
                                  int opusBitrateKbps, const QString &importMode);
    Q_INVOKABLE void setDacPassthrough(bool enabled);
    Q_INVOKABLE void fetchLyricsForPlayingTrack();
    Q_INVOKABLE void fetchLyricsForTrackIndex(int index);
    Q_INVOKABLE void fetchLyricsForAlbum(const QString &artist, const QString &album);
    Q_INVOKABLE void fetchLyricsForArtist(const QString &artist);
    Q_INVOKABLE void fetchLyricsForLibrary();

signals:
    void albumsChanged();
    void selectionChanged();
    void scanStatusChanged();
    void mainViewChanged();
    void tagEditorChanged();
    void tagFetchChanged();
    void titleFixChanged();
    void librarySearchChanged();
    void librarySearchOpenChanged();
    void librarySearchFocusRequested();
    void libraryFocusChanged();
    void noticeChanged();
    void albumDiscogsChanged();
    void playlistFocusChanged();
    void multiSelectionChanged();

private:
    void refreshArtists();
    void refreshAlbums();
    void refreshTracks();
    void refreshPlaylistItems();
    void refreshPlaylistTracks();
    void ensureLibrarySearchSelection();
    void syncLibraryFocusToSearchScope();
    void clearMultiTrackSelectionInternal(bool emitSignal);
    void reloadPlaylistsIfReady();
    void onTrackChanged();
    void onDiscogsArtistFetched(const QString &artistName, bool success);
    void onDiscogsReleaseFetched(const QString &artist, const QString &album, bool success);
    void fetchNextBulkDiscogsAlbum();
    void handleBulkDiscogsSearch(const QVariantList &results);
    void syncFetchedTagsToBeets(const QVariantMap &fields, const QString &libraryArtist,
                                const QString &libraryAlbum);
    void clearLibrarySelection();
    void selectAlbumDrillOut();
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
    LyricsService *m_lyrics = nullptr;
    MetadataSearchService *m_metadataSearch = nullptr;
    LibraryEnrichmentService *m_enrichment = nullptr;

    QStringList m_albums;
    QString m_selectedArtist;
    QString m_selectedAlbum;
    QString m_selectedAlbumArtUrl;
    QString m_selectedAlbumInfo;
    bool m_nowPlayingFocused = false;
    bool m_albumDiscogsOpen = false;
    QVariantList m_albumDiscogsCandidates;
    int m_albumDiscogsSelectedIndex = -1;
    bool m_bulkDiscogsAlbumsActive = false;
    QString m_bulkDiscogsArtist;
    QStringList m_bulkDiscogsAlbums;
    QString m_bulkDiscogsCurrentAlbum;
    int m_bulkDiscogsIndex = 0;
    int m_bulkDiscogsSucceeded = 0;
    int m_bulkDiscogsFailed = 0;
    QString m_mainView = QStringLiteral("library");
    bool m_pendingPlaylistReload = false;

    bool m_tagEditorOpen = false;
    QVariantMap m_tagEditorFields;
    QString m_tagEditorMode;
    QString m_tagEditorTitle;
    QString m_tagEditorPath;
    bool m_tagFetchOpen = false;
    bool m_titleFixOpen = false;

    bool m_librarySearchOpen = false;
    QString m_librarySearchQuery;
    QString m_librarySearchScope = QStringLiteral("artists");
    QString m_libraryFocusColumn = QStringLiteral("artists");
    int m_selectedTrackIndex = -1;
    QList<int> m_multiSelectedTracks;
    int m_multiSelectAnchor = -1;
    QString m_playlistFocusColumn = QStringLiteral("playlists");
    int m_selectedPlaylistTrackIndex = -1;
    QString m_noticeText;
    QString m_noticeKind;
    int m_noticeSerial = 0;
    bool m_scanToastArmed = false;
};
