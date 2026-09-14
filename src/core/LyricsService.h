#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class ConfigService;
class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class QTimer;
class QUrl;

// Multi-provider lyrics fetcher. Chain (first hit wins):
//   LRCLIB (synced) → NetEase (synced, optional) → lyrics.ovh (plain, optional).
// Strictly sequential with per-provider throttling, honours HTTP 429/5xx with
// exponential backoff, and records a global miss only when the whole chain
// misses, so each track hits the network at most once per retention window.
class LyricsService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int total READ total NOTIFY progressChanged)

public:
    explicit LyricsService(ConfigService *config, QObject *parent = nullptr);
    ~LyricsService() override;

    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    int progress() const { return m_done; }
    int total() const { return m_total; }

    Q_INVOKABLE void fetchForTrack(const QVariantMap &track);
    Q_INVOKABLE void fetchForTracks(const QVariantList &tracks);
    // Explicit single-track retry: bypasses the negative cache.
    Q_INVOKABLE void retryForTrack(const QVariantMap &track);
    Q_INVOKABLE void cancel();

    static bool sidecarExists(const QString &trackPath);
    static bool syncedSidecarExists(const QString &trackPath);
    static bool plainSidecarExists(const QString &trackPath);
    static QString sidecarPath(const QString &trackPath);
    static QString plainSidecarPath(const QString &trackPath);

    // Pure response parsers (public for testing).
    static qint64 pickNeteaseMatch(const QJsonDocument &doc, const QString &artist,
                                   const QString &title, double wantSecs);
    static QString neteaseSyncedFromDoc(const QJsonDocument &doc);
    static QString ovPlainFromDoc(const QJsonDocument &doc);
    static QString buildSummary(int fetched, int fetchedLrclib, int fetchedNetease,
                                int fetchedOv, int skipped, int knownMiss, int failed);

signals:
    void busyChanged();
    void statusChanged();
    void progressChanged();
    void trackFetched(const QString &path, bool success);

private slots:
    void processNext();
    void onReplyFinished();

private:
    enum class Provider { Lrclib, Netease, LyricsOv };
    enum class StepResult { Done, Miss, Transient };

    struct Pending {
        QVariantMap track;
        Provider provider = Provider::Lrclib;
        // Stage 0 = lookup/search, 1 = follow-up (get-by-id / lyric).
        // For LRCLIB, stage 1 walks search query variants via searchVariant.
        int stage = 0;
        int searchVariant = 0;
        qint64 externalId = 0;
    };

    void setBusy(bool busy);
    void setStatus(const QString &status);
    void enqueue(const QVariantList &tracks);
    void finishTrack(bool fetched, bool skipped, const QString &source = {},
                     bool knownMiss = false);
    void completeSave(const QString &sidecarFile, const QString &text,
                      const QString &source, int *counter);
    void finishJob(const QString &summary);
    void startRequest(const QUrl &url, const QNetworkRequest &request);
    void startPostRequest(const QUrl &url, const QByteArray &body,
                          const QNetworkRequest &request);
    bool requeueForRetry(int retryAfterSecs);
    void handleRateLimited(int retryAfterSecs);
    void scheduleNext(int delayMs);
    int intervalMs() const;
    int slowIntervalMs() const;
    void startProviderRequest();
    void advanceProvider();
    bool providerUsable(Provider provider, const QString &trackPath) const;

    StepResult handleLrclibReply(int httpStatus, bool netErr, const QByteArray &body);
    StepResult handleNeteaseReply(int httpStatus, bool netErr, const QByteArray &body);
    StepResult handleLyricsOvReply(int httpStatus, bool netErr, const QByteArray &body);
    QNetworkRequest lrclibRequest(const QUrl &url) const;
    QNetworkRequest neteaseRequest(const QUrl &url) const;
    QNetworkRequest plainRequest(const QUrl &url) const;
    QUrl cachedUrl(const QVariantMap &track) const;
    // LRCLIB /search query walk: 0 = artist+title, 1 = stripped (no feat/
    // parenthetical/track-number), 2 = title only, 3 = structured
    // track_name+artist_name. Empty query = variants exhausted.
    QString lrclibSearchQuery(const QVariantMap &track, int variant) const;
    QUrl searchUrl(const QVariantMap &track, int variant = 0) const;
    // Next LRCLIB /search variant; false when the walk is exhausted.
    bool requestNextLrclibSearch();
    static double trackDurationSecs(const QVariantMap &track);
    static bool syncedUsable(const QString &synced);
    static bool plainUsable(const QString &plain);
    static bool durationMatches(double haveSecs, double wantSecs);
    static bool durationMatchesMs(qint64 haveMs, double wantSecs);
    bool writeSidecar(const QString &filePath, const QString &text);
    bool negativeHit(const QString &trackPath) const;
    void recordNegative(const QString &trackPath) const;
    void openCacheDb() const;

    ConfigService *m_config = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QTimer *m_timer = nullptr;
    QNetworkReply *m_reply = nullptr;

    QList<QVariantMap> m_queue;
    Pending m_current;
    bool m_hasCurrent = false;
    bool m_providerResumePending = false;
    bool m_busy = false;
    bool m_cancelRequested = false;
    QString m_status;

    int m_total = 0;
    int m_done = 0;
    int m_fetched = 0;
    int m_fetchedLrclib = 0;
    int m_fetchedNetease = 0;
    int m_fetchedOv = 0;
    int m_skipped = 0;
    int m_knownMiss = 0;
    int m_failed = 0;
    int m_backoffSecs = 0;
    int m_netease403s = 0;
    qint64 m_neteaseCooldownUntilMs = 0;
    qint64 m_lastSlowRequestMs = 0;

    mutable QSqlDatabase m_cacheDb;
    bool m_cacheDbOpen = false;
};
