#pragma once

#include <QObject>
#include <QPair>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

class AppController;
class BeetsService;
class ConfigService;
class DiscogsService;
class LibraryService;
class LyricsService;
class MetadataSearchService;
class TagService;

class LibraryEnrichmentService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY stateChanged)
    Q_PROPERTY(QString phase READ phase NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(int progress READ progress NOTIFY stateChanged)
    Q_PROPERTY(int total READ total NOTIFY stateChanged)
    Q_PROPERTY(int autoApplied READ autoApplied NOTIFY stateChanged)
    Q_PROPERTY(int resolved READ resolved NOTIFY stateChanged)
    Q_PROPERTY(int skipped READ skipped NOTIFY stateChanged)
    Q_PROPERTY(int failed READ failed NOTIFY stateChanged)
    Q_PROPERTY(QString currentArtist READ currentArtist NOTIFY stateChanged)
    Q_PROPERTY(QString currentAlbum READ currentAlbum NOTIFY stateChanged)
    Q_PROPERTY(QString resolveKind READ resolveKind NOTIFY stateChanged)
    Q_PROPERTY(QVariantList candidates READ candidates NOTIFY stateChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY stateChanged)
    Q_PROPERTY(QVariantList titleFixProposals READ titleFixProposals NOTIFY stateChanged)
    Q_PROPERTY(QVariantList titleFixUnmatched READ titleFixUnmatched NOTIFY stateChanged)
    Q_PROPERTY(int localTrackCount READ localTrackCount NOTIFY stateChanged)

public:
    explicit LibraryEnrichmentService(ConfigService *config, LibraryService *library,
                                      DiscogsService *discogs, MetadataSearchService *metadataSearch,
                                      TagService *tags, BeetsService *beets, LyricsService *lyrics,
                                      AppController *app, QObject *parent = nullptr);

    bool active() const { return m_active; }
    bool paused() const { return m_paused; }
    QString phase() const { return m_phase; }
    QString status() const { return m_status; }
    int progress() const { return m_progress; }
    int total() const { return m_total; }
    int autoApplied() const { return m_autoApplied; }
    int resolved() const { return m_resolved; }
    int skipped() const { return m_skipped; }
    int failed() const { return m_failed; }
    QString currentArtist() const { return m_currentArtist; }
    QString currentAlbum() const { return m_currentAlbum; }
    QString resolveKind() const { return m_resolveKind; }
    QVariantList candidates() const { return m_candidates; }
    int selectedIndex() const { return m_selectedIndex; }
    QVariantList titleFixProposals() const;
    QVariantList titleFixUnmatched() const;
    int localTrackCount() const { return m_localTrackCount; }

    Q_INVOKABLE bool start();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void chooseSelected();
    Q_INVOKABLE void skipCurrent();
    Q_INVOKABLE void setSelectedIndex(int index);
    Q_INVOKABLE void setTitleFixChecked(int index, bool checked);

    void handleAlbumSearchResults(const QVariantList &results);

signals:
    void stateChanged();

private:
    enum class WaitKind {
        None,
        ArtistSearch,
        ArtistFetch,
        AlbumSearch,
        AlbumFetch,
        TitleFixSearch,
        TitleFixDetail,
        Lyrics,
    };

    void setStatus(const QString &status);
    void emitState();
    void clearResolveUi();
    void finishRun();
    void bumpFailedAndAdvance();
    void advanceAfterSuccess(bool autoApplied);
    void scheduleAdvance(std::function<void()> next);

    void startArtistPhase();
    void processNextArtist();
    void onArtistsSearchFinished(const QVariantList &results);
    void applyArtistChoice(quint64 discogsId, bool autoApplied);

    void startAlbumPhase();
    void processNextAlbum();
    void onAlbumSearchResults(const QVariantList &results);
    void applyAlbumChoice(quint64 releaseId, bool autoApplied);

    void startTitleFixPhase();
    void processNextTitleFix();
    void onTitleFixSearchFinished(bool success);
    void onTitleFixDetailMaybeReady();
    void decideTitleFixAutoOrPause();
    bool titleFixIsExactAutoApply() const;
    void applyTitleFixProposals(bool countAsAuto);
    void mirrorTitleFixCandidates();

    void startLyricsPhase();
    void onLyricsBusyChanged();
    void onLyricsProgressChanged();

    void onArtistFetched(const QString &artistName, bool success);
    void onReleaseFetched(const QString &artist, const QString &album, bool success);

    static QString albumTitleKey(const QString &title, const QString &artist);
    static QVariantList rankArtistCandidates(const QVariantList &results, const QString &artist);
    static bool artistFolderHasInfo(const QString &folder);
    static bool albumFolderHasInfo(const QString &folder);
    static bool isRedirectProfile(const QString &profile);

    ConfigService *m_config = nullptr;
    LibraryService *m_library = nullptr;
    DiscogsService *m_discogs = nullptr;
    MetadataSearchService *m_metadataSearch = nullptr;
    TagService *m_tags = nullptr;
    BeetsService *m_beets = nullptr;
    LyricsService *m_lyrics = nullptr;
    AppController *m_app = nullptr;

    bool m_active = false;
    bool m_paused = false;
    bool m_cancelRequested = false;
    QString m_phase = QStringLiteral("idle");
    QString m_status;
    int m_progress = 0;
    int m_total = 0;
    int m_autoApplied = 0;
    int m_resolved = 0;
    int m_skipped = 0;
    int m_failed = 0;
    QString m_currentArtist;
    QString m_currentAlbum;
    QString m_resolveKind = QStringLiteral("none");
    QVariantList m_candidates;
    int m_selectedIndex = -1;
    int m_localTrackCount = 0;

    QStringList m_artistQueue;
    QList<QPair<QString, QString>> m_albumQueue;
    QList<QPair<QString, QString>> m_titleFixQueue;
    int m_artistIndex = 0;
    int m_albumIndex = 0;
    int m_titleFixIndex = 0;

    WaitKind m_wait = WaitKind::None;
    bool m_pendingAutoApply = false;
    QString m_pendingFolder;
};
