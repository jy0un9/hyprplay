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
//   LRCLIB (synced) → NetEase (synced) → lyrics.ovh (plain) → Genius (plain, token).
// Strictly sequential with per-provider throttling, honours HTTP 429/5xx with
// exponential backoff, and records a global miss only when the whole chain
// misses, so each track hits the network at most once per retention window.
class LyricsService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int total READ total NOTIFY progressChanged)
    Q_PROPERTY(bool geniusTokenSet READ geniusTokenSet NOTIFY geniusTokenChanged)

public:
    explicit LyricsService(ConfigService *config, QObject *parent = nullptr);
    ~LyricsService() override;

    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    int progress() const { return m_done; }
    int total() const { return m_total; }
    bool geniusTokenSet() const;

    Q_INVOKABLE void fetchForTrack(const QVariantMap &track);
    Q_INVOKABLE void fetchForTracks(const QVariantList &tracks);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void setGeniusToken(const QString &token);

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
    static QString geniusPageFromSearch(const QJsonDocument &doc, const QString &artist,
                                        const QString &title);
    static QString scrapeGeniusHtml(const QString &html);
    static QString buildSummary(int fetched, int fetchedLrclib, int fetchedNetease,
                                int fetchedOv, int fetchedGenius, int skipped,
                                int knownMiss, int failed);

signals:
    void busyChanged();
    void statusChanged();
    void progressChanged();
    void geniusTokenChanged();
    void trackFetched(const QString &path, bool success);

private slots:
    void processNext();
    void onReplyFinished();

private:
    enum class Provider { Lrclib, Netease, LyricsOv, Genius };
    enum class StepResult { Done, Miss, Transient };

    struct Pending {
        QVariantMap track;
        Provider provider = Provider::Lrclib;
        // Stage 0 = lookup/search, 1 = follow-up (get-by-id / lyric / page).
        int stage = 0;
        qint64 externalId = 0;
        QString pageUrl;
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
    QString loadGeniusToken() const;

    StepResult handleLrclibReply(int httpStatus, bool netErr, const QByteArray &body);
    StepResult handleNeteaseReply(int httpStatus, bool netErr, const QByteArray &body);
    StepResult handleLyricsOvReply(int httpStatus, bool netErr, const QByteArray &body);
    StepResult handleGeniusReply(int httpStatus, bool netErr, const QByteArray &body);

    QNetworkRequest lrclibRequest(const QUrl &url) const;
    QNetworkRequest neteaseRequest(const QUrl &url) const;
    QNetworkRequest geniusApiRequest(const QUrl &url, const QString &token) const;
    QNetworkRequest geniusPageRequest(const QUrl &url) const;
    QNetworkRequest plainRequest(const QUrl &url) const;
    QUrl cachedUrl(const QVariantMap &track) const;
    QUrl searchUrl(const QVariantMap &track) const;
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
    bool m_geniusDead = false;
    QString m_status;

    int m_total = 0;
    int m_done = 0;
    int m_fetched = 0;
    int m_fetchedLrclib = 0;
    int m_fetchedNetease = 0;
    int m_fetchedOv = 0;
    int m_fetchedGenius = 0;
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
