#include "LyricsService.h"

#include "ConfigService.h"
#include "LyricsParsers.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace {

constexpr char kLrclibBase[] = "https://lrclib.net/api";
constexpr char kNeteaseSearch[] = "https://music.163.com/api/search/get";
constexpr char kNeteaseLyric[] = "https://music.163.com/api/song/lyric";
constexpr char kOvBase[] = "https://api.lyrics.ovh/v1";
constexpr char kUserAgent[] = "hyprplay/0.1.1 (+https://github.com/jy0un9/hyprplay)";
constexpr char kClientIdent[] = "hyprplay/0.1.1 (+https://github.com/jy0un9/hyprplay)";
// Accept a candidate only when durations agree within this window.
constexpr double kDurationToleranceSecs = 10.0;
constexpr double kNeteaseToleranceSecs = 8.0;
constexpr int kMaxBackoffSecs = 300;
constexpr int kMaxTransientAttempts = 4;
constexpr qint64 kNeteaseCooldownMs = 10 * 60 * 1000;

int retryAfterSecs(QNetworkReply *reply) {
    if (!reply) {
        return 0;
    }
    const QByteArray raw = reply->rawHeader("Retry-After");
    bool ok = false;
    const int secs = raw.trimmed().toInt(&ok);
    if (ok && secs > 0) {
        return secs;
    }
    return 0;
}

void applyTimeout(QNetworkRequest &request) {
    request.setTransferTimeout(15000);
}


} // namespace

LyricsService::LyricsService(ConfigService *config, QObject *parent)
    : QObject(parent), m_config(config) {
    m_network = new QNetworkAccessManager(this);
    m_network->setCookieJar(new QNetworkCookieJar(m_network));
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &LyricsService::processNext);
}

LyricsService::~LyricsService() {
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
    }
    if (m_cacheDbOpen) {
        const QString name = m_cacheDb.connectionName();
        m_cacheDb.close();
        m_cacheDb = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
    }
}

bool LyricsService::sidecarExists(const QString &trackPath) {
    return syncedSidecarExists(trackPath) || plainSidecarExists(trackPath);
}

bool LyricsService::syncedSidecarExists(const QString &trackPath) {
    if (trackPath.isEmpty()) {
        return false;
    }
    return QFile::exists(sidecarPath(trackPath));
}

bool LyricsService::plainSidecarExists(const QString &trackPath) {
    if (trackPath.isEmpty()) {
        return false;
    }
    return QFile::exists(plainSidecarPath(trackPath));
}

QString LyricsService::sidecarPath(const QString &trackPath) {
    const QFileInfo track(trackPath);
    return track.absolutePath() + QLatin1Char('/') + track.completeBaseName()
        + QStringLiteral(".lrc");
}

QString LyricsService::plainSidecarPath(const QString &trackPath) {
    const QFileInfo track(trackPath);
    return track.absolutePath() + QLatin1Char('/') + track.completeBaseName()
        + QStringLiteral(".txt");
}

void LyricsService::fetchForTrack(const QVariantMap &track) {
    fetchForTracks({track});
}

void LyricsService::fetchForTracks(const QVariantList &tracks) {
    QVariantList valid;
    for (const QVariant &value : tracks) {
        const QVariantMap track = value.toMap();
        if (!track.value(QStringLiteral("path")).toString().trimmed().isEmpty()) {
            valid << track;
        }
    }
    if (valid.isEmpty()) {
        return;
    }
    enqueue(valid);
}

void LyricsService::retryForTrack(const QVariantMap &track) {
    QVariantMap forced = track;
    forced.insert(QStringLiteral("__forceRetry"), true);
    fetchForTracks({forced});
}

void LyricsService::cancel() {
    if (!m_busy) {
        return;
    }
    m_cancelRequested = true;
    m_timer->stop();
    if (m_reply) {
        m_reply->abort();
    } else {
        m_queue.clear();
        finishJob(QStringLiteral("Lyrics fetch cancelled"));
    }
}

void LyricsService::enqueue(const QVariantList &tracks) {
    const bool wasIdle = !m_busy && !m_hasCurrent && m_queue.isEmpty();
    for (const QVariant &value : tracks) {
        m_queue.append(value.toMap());
    }
    if (wasIdle) {
        m_total = m_queue.size();
        m_done = 0;
        m_fetched = 0;
        m_fetchedLrclib = 0;
        m_fetchedNetease = 0;
        m_fetchedOv = 0;
        m_skipped = 0;
        m_knownMiss = 0;
        m_failed = 0;
        m_backoffSecs = 0;
        m_netease403s = 0;
        m_cancelRequested = false;
        setBusy(true);
        emit progressChanged();
        setStatus(QStringLiteral("Fetching lyrics (0/%1)…").arg(m_total));
        QTimer::singleShot(0, this, &LyricsService::processNext);
    } else {
        m_total += tracks.size();
        emit progressChanged();
    }
}

void LyricsService::setBusy(bool busy) {
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

void LyricsService::setStatus(const QString &status) {
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}

int LyricsService::intervalMs() const {
    const int secs = m_config ? m_config->lyricsFetchIntervalSecs() : 2;
    return qBound(1, secs, 60) * 1000;
}

int LyricsService::slowIntervalMs() const {
    const int secs = m_config ? m_config->lyricsSlowIntervalSecs() : 4;
    return qBound(2, secs, 120) * 1000;
}

void LyricsService::scheduleNext(int delayMs) {
    m_timer->start(delayMs);
}

void LyricsService::processNext() {
    if (m_cancelRequested) {
        m_queue.clear();
        m_hasCurrent = false;
        m_providerResumePending = false;
        finishJob(QStringLiteral("Lyrics fetch cancelled"));
        return;
    }
    if (m_reply) {
        return; // request still in flight; its finished slot drives us
    }
    if (m_providerResumePending) {
        m_providerResumePending = false;
        if (m_hasCurrent) {
            startProviderRequest();
        }
        return;
    }
    if (m_queue.isEmpty()) {
        m_hasCurrent = false;
        finishJob(buildSummary(m_fetched, m_fetchedLrclib, m_fetchedNetease, m_fetchedOv,
                               m_skipped, m_knownMiss, m_failed));
        return;
    }

    m_current.track = m_queue.takeFirst();
    m_current.provider = Provider::Lrclib;
    // Debug hook (also handy for manual provider tests): start the chain at a
    // later provider. Values: netease, ov.
    if (const QByteArray startAt = qgetenv("QT_MUSIC_LYRICS_START_PROVIDER");
        !startAt.isEmpty()) {
        if (startAt == "netease") {
            m_current.provider = Provider::Netease;
        } else if (startAt == "ov") {
            m_current.provider = Provider::LyricsOv;
        }
    }
    m_current.stage = 0;
    m_current.searchVariant = 0;
    m_current.externalId = 0;
    m_hasCurrent = true;

    const QString path = m_current.track.value(QStringLiteral("path")).toString();
    setStatus(QStringLiteral("Fetching lyrics (%1/%2)…").arg(m_done + 1).arg(m_total));

    if (syncedSidecarExists(path)) {
        finishTrack(false, true);
        return;
    }
    // Explicit single-track fetch (karaoke "Fetch lyrics" button, track menu)
    // retries even known misses — the DB grows and tags get fixed.
    const bool forceRetry =
        m_current.track.value(QStringLiteral("__forceRetry")).toBool();
    if (!forceRetry && negativeHit(path)) {
        // Known miss from an earlier full-chain check: no network, and the
        // summary must not claim lyrics are present.
        finishTrack(false, false, {}, true);
        return;
    }
    const QString artist = m_current.track.value(QStringLiteral("artist")).toString().trimmed();
    const QString title = m_current.track.value(QStringLiteral("title")).toString().trimmed();
    if (artist.isEmpty() || title.isEmpty()) {
        finishTrack(false, false);
        return;
    }
    startProviderRequest();
}

bool LyricsService::providerUsable(Provider provider, const QString &trackPath) const {
    switch (provider) {
    case Provider::Lrclib:
        return true;
    case Provider::Netease: {
        const bool enabled = m_config ? m_config->lyricsNeteaseEnabled() : true;
        if (!enabled) {
            return false;
        }
        return QDateTime::currentMSecsSinceEpoch() >= m_neteaseCooldownUntilMs;
    }
    case Provider::LyricsOv: {
        const bool enabled = m_config ? m_config->lyricsPlainEnabled() : true;
        return enabled && !plainSidecarExists(trackPath);
    }
    }
    return false;
}

void LyricsService::startProviderRequest() {
    if (!m_hasCurrent || m_cancelRequested) {
        return;
    }
    const QString path = m_current.track.value(QStringLiteral("path")).toString();

    // Advance past providers that cannot run for this track.
    while (!providerUsable(m_current.provider, path)) {
        if (m_current.provider == Provider::LyricsOv) {
            recordNegative(path);
            finishTrack(false, false);
            return;
        }
        m_current.provider = static_cast<Provider>(static_cast<int>(m_current.provider) + 1);
        m_current.stage = 0;
        m_current.searchVariant = 0;
        m_current.externalId = 0;
    }

    // Slow providers (unofficial APIs) get a wider gap since the previous slow
    // request. Same-track follow-ups are exempt: one extra call right after a
    // search is normal client behaviour.
    if (m_current.stage == 0 && m_current.provider == Provider::Netease) {
        const qint64 elapsed =
            QDateTime::currentMSecsSinceEpoch() - m_lastSlowRequestMs;
        if (elapsed < slowIntervalMs()) {
            m_providerResumePending = true;
            scheduleNext(static_cast<int>(slowIntervalMs() - elapsed));
            return;
        }
    }

    const QVariantMap &track = m_current.track;
    switch (m_current.provider) {
    case Provider::Lrclib:
        startRequest(cachedUrl(track), lrclibRequest(cachedUrl(track)));
        break;
    case Provider::Netease: {
        QUrl url(QString::fromUtf8(kNeteaseSearch));
        QUrlQuery form;
        form.addQueryItem(QStringLiteral("s"),
                          track.value(QStringLiteral("artist")).toString().trimmed()
                              + QLatin1Char(' ')
                              + track.value(QStringLiteral("title")).toString().trimmed());
        form.addQueryItem(QStringLiteral("type"), QStringLiteral("1"));
        form.addQueryItem(QStringLiteral("offset"), QStringLiteral("0"));
        form.addQueryItem(QStringLiteral("limit"), QStringLiteral("10"));
        form.addQueryItem(QStringLiteral("total"), QStringLiteral("true"));
        QNetworkRequest request = neteaseRequest(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          "application/x-www-form-urlencoded");
        m_lastSlowRequestMs = QDateTime::currentMSecsSinceEpoch();
        startPostRequest(url, form.query(QUrl::FullyEncoded).toUtf8(), request);
        break;
    }
    case Provider::LyricsOv: {
        const QString artist = track.value(QStringLiteral("artist")).toString().trimmed();
        const QString title = track.value(QStringLiteral("title")).toString().trimmed();
        QUrl url(QString::fromUtf8(kOvBase) + QLatin1Char('/')
                 + QUrl::toPercentEncoding(artist) + QLatin1Char('/')
                 + QUrl::toPercentEncoding(title));
        startRequest(url, plainRequest(url));
        break;
    }
    }
}

void LyricsService::advanceProvider() {
    if (!m_hasCurrent) {
        return;
    }
    // Step to the next provider; startProviderRequest skips unusable ones and
    // records the global miss when the chain is exhausted.
    if (m_current.provider == Provider::LyricsOv) {
        const QString path = m_current.track.value(QStringLiteral("path")).toString();
        recordNegative(path);
        finishTrack(false, false);
        return;
    }
    m_current.provider = static_cast<Provider>(static_cast<int>(m_current.provider) + 1);
    m_current.stage = 0;
    m_current.searchVariant = 0;
    m_current.externalId = 0;
    startProviderRequest();
}

void LyricsService::startRequest(const QUrl &url, const QNetworkRequest &request) {
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    Q_UNUSED(url);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &LyricsService::onReplyFinished);
}

void LyricsService::startPostRequest(const QUrl &url, const QByteArray &body,
                                     const QNetworkRequest &request) {
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    Q_UNUSED(url);
    m_reply = m_network->post(request, body);
    connect(m_reply, &QNetworkReply::finished, this, &LyricsService::onReplyFinished);
}

void LyricsService::onReplyFinished() {
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (!reply) {
        return;
    }
    reply->deleteLater();

    if (m_cancelRequested) {
        m_queue.clear();
        m_hasCurrent = false;
        m_providerResumePending = false;
        finishJob(QStringLiteral("Lyrics fetch cancelled"));
        return;
    }
    if (!m_hasCurrent) {
        scheduleNext(intervalMs());
        return;
    }
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        // Cancel path already handled above; treat stray cancels as failures.
        finishTrack(false, false);
        return;
    }

    const QByteArray body = reply->readAll();
    const int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const int retryAfter = retryAfterSecs(reply);
    const bool netErr = reply->error() != QNetworkReply::NoError;

    StepResult result = StepResult::Miss;
    switch (m_current.provider) {
    case Provider::Lrclib:
        result = handleLrclibReply(httpStatus, netErr, body);
        break;
    case Provider::Netease:
        result = handleNeteaseReply(httpStatus, netErr, body);
        break;
    case Provider::LyricsOv:
        result = handleLyricsOvReply(httpStatus, netErr, body);
        break;
    }

    if (result == StepResult::Done) {
        return; // track finished or a follow-up request is in flight
    }
    if (result == StepResult::Miss) {
        advanceProvider();
        return;
    }
    if (!requeueForRetry(retryAfter)) {
        const QString path = m_current.track.value(QStringLiteral("path")).toString();
        recordNegative(path);
        finishTrack(false, false);
    }
}
LyricsService::StepResult LyricsService::handleLrclibReply(int httpStatus, bool netErr,
                                                           const QByteArray &body) {
    const QString path = m_current.track.value(QStringLiteral("path")).toString();
    const double wantSecs = trackDurationSecs(m_current.track);

    if (netErr) {
        if (httpStatus == 429 || (httpStatus >= 500 && httpStatus < 600)) {
            return StepResult::Transient;
        }
        if (httpStatus == 404 && m_current.stage == 0) {
            // /get is duration-strict; fall through to the /search walk.
            m_current.stage = 1;
            m_current.searchVariant = -1;
            if (requestNextLrclibSearch()) {
                return StepResult::Done;
            }
            return StepResult::Miss;
        }
        if (httpStatus == 404 && m_current.stage == 1) {
            // A search variant 404'd — try the next variant.
            if (requestNextLrclibSearch()) {
                return StepResult::Done;
            }
            return StepResult::Miss;
        }
        return StepResult::Miss;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (m_current.stage == 0) {
        const QJsonObject obj = doc.object();
        const QString synced = obj.value(QStringLiteral("syncedLyrics")).toString();
        const double haveSecs = obj.value(QStringLiteral("duration")).toDouble(-1.0);
        if (syncedUsable(synced) && durationMatches(haveSecs, wantSecs)) {
            completeSave(sidecarPath(path), synced, QStringLiteral("LRCLIB"),
                         &m_fetchedLrclib);
        } else {
            m_current.stage = 1;
            m_current.searchVariant = -1;
            if (!requestNextLrclibSearch()) {
                return StepResult::Miss;
            }
        }
        return StepResult::Done;
    }

    if (m_current.stage == 1) {
        const QJsonArray items = doc.isArray() ? doc.array() : QJsonArray();
        // /search returns full records inline — save the scored hit directly
        // instead of a second round-trip.
        const qint64 pickId = LyricsParsers::pickLrclibMatch(
            items, m_current.track.value(QStringLiteral("artist")).toString(),
            m_current.track.value(QStringLiteral("title")).toString(),
            m_current.track.value(QStringLiteral("album")).toString(), wantSecs);
        if (pickId > 0) {
            for (const QJsonValue &value : items) {
                const QJsonObject obj = value.toObject();
                if (obj.value(QStringLiteral("id")).toVariant().toLongLong() != pickId) {
                    continue;
                }
                const QString synced = obj.value(QStringLiteral("syncedLyrics")).toString();
                if (syncedUsable(synced)) {
                    completeSave(sidecarPath(path), synced, QStringLiteral("LRCLIB"),
                                 &m_fetchedLrclib);
                    return StepResult::Done;
                }
                break;
            }
            // Scored hit had no usable synced text — fetch the full record.
            m_current.stage = 2;
            m_current.externalId = pickId;
            const QUrl url(QString::fromUtf8(kLrclibBase) + QStringLiteral("/get/")
                           + QString::number(pickId));
            startRequest(url, lrclibRequest(url));
            return StepResult::Done;
        }
        if (requestNextLrclibSearch()) {
            return StepResult::Done;
        }
        return StepResult::Miss;
    }

    const QJsonObject obj = doc.object();
    const QString synced = obj.value(QStringLiteral("syncedLyrics")).toString();
    if (syncedUsable(synced)) {
        completeSave(sidecarPath(path), synced, QStringLiteral("LRCLIB"), &m_fetchedLrclib);
        return StepResult::Done;
    }
    // get-by-id had no synced lyrics — keep walking remaining variants.
    m_current.stage = 1;
    if (requestNextLrclibSearch()) {
        return StepResult::Done;
    }
    return StepResult::Miss;
}

LyricsService::StepResult LyricsService::handleNeteaseReply(int httpStatus, bool netErr,
                                                            const QByteArray &body) {
    const QString path = m_current.track.value(QStringLiteral("path")).toString();

    if (netErr) {
        if (httpStatus == 403) {
            // WAF block: cool the provider down for the rest of the session
            // window instead of burning retries on every track.
            if (++m_netease403s >= 2) {
                m_neteaseCooldownUntilMs =
                    QDateTime::currentMSecsSinceEpoch() + kNeteaseCooldownMs;
                setStatus(QStringLiteral("NetEase blocked — cooling down, continuing…"));
            }
            return StepResult::Miss;
        }
        if (httpStatus == 429 || (httpStatus >= 500 && httpStatus < 600)) {
            return StepResult::Transient;
        }
        return StepResult::Miss;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (m_current.stage == 0) {
        const qint64 songId = pickNeteaseMatch(
            doc, m_current.track.value(QStringLiteral("artist")).toString(),
            m_current.track.value(QStringLiteral("title")).toString(),
            trackDurationSecs(m_current.track));
        if (songId <= 0) {
            return StepResult::Miss;
        }
        m_current.stage = 1;
        m_current.externalId = songId;
        QUrl url(QString::fromUtf8(kNeteaseLyric));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("os"), QStringLiteral("osx"));
        query.addQueryItem(QStringLiteral("id"), QString::number(songId));
        query.addQueryItem(QStringLiteral("lv"), QStringLiteral("-1"));
        query.addQueryItem(QStringLiteral("kv"), QStringLiteral("-1"));
        query.addQueryItem(QStringLiteral("tv"), QStringLiteral("-1"));
        url.setQuery(query);
        m_lastSlowRequestMs = QDateTime::currentMSecsSinceEpoch();
        startRequest(url, neteaseRequest(url));
        return StepResult::Done;
    }

    const QString synced = neteaseSyncedFromDoc(doc);
    if (syncedUsable(synced)) {
        completeSave(sidecarPath(path), synced, QStringLiteral("NetEase"), &m_fetchedNetease);
    } else {
        return StepResult::Miss;
    }
    return StepResult::Done;
}

LyricsService::StepResult LyricsService::handleLyricsOvReply(int httpStatus, bool netErr,
                                                             const QByteArray &body) {
    if (netErr) {
        if (httpStatus == 429 || (httpStatus >= 500 && httpStatus < 600)) {
            return StepResult::Transient;
        }
        return StepResult::Miss;
    }
    const QString plain =
        ovPlainFromDoc(QJsonDocument::fromJson(body));
    if (!plainUsable(plain)) {
        return StepResult::Miss;
    }
    const QString path = m_current.track.value(QStringLiteral("path")).toString();
    completeSave(plainSidecarPath(path), plain, QStringLiteral("lyrics.ovh"), &m_fetchedOv);
    return StepResult::Done;
}

bool LyricsService::requeueForRetry(int retryAfterSecs) {
    const int attempts = m_current.track.value(QStringLiteral("__attempts")).toInt();
    if (attempts >= kMaxTransientAttempts || !m_hasCurrent) {
        return false;
    }
    handleRateLimited(retryAfterSecs);
    return true;
}

void LyricsService::handleRateLimited(int retryAfter) {
    // Requeue the current track (bumping its attempt counter) and back off.
    // The job pauses, never aborts.
    if (m_hasCurrent) {
        QVariantMap retry = m_current.track;
        retry.insert(QStringLiteral("__attempts"),
                     retry.value(QStringLiteral("__attempts")).toInt() + 1);
        m_queue.prepend(retry);
        m_hasCurrent = false;
    }
    m_providerResumePending = false;
    m_backoffSecs = qBound(intervalMs() / 1000, qMax(m_backoffSecs * 2, 2), kMaxBackoffSecs);
    if (retryAfter > 0) {
        m_backoffSecs = qMin(qMax(retryAfter, m_backoffSecs), kMaxBackoffSecs);
    }
    setStatus(QStringLiteral("Rate limited — retrying in %1s…").arg(m_backoffSecs));
    scheduleNext(m_backoffSecs * 1000);
}

void LyricsService::finishTrack(bool fetched, bool skipped, const QString &source,
                                 bool knownMiss) {
    Q_UNUSED(source);
    m_hasCurrent = false;
    ++m_done;
    if (fetched) {
        ++m_fetched;
        m_backoffSecs = 0;
        emit trackFetched(m_current.track.value(QStringLiteral("path")).toString(), true);
    } else if (knownMiss) {
        ++m_knownMiss;
    } else if (skipped) {
        ++m_skipped;
    } else {
        ++m_failed;
    }
    emit progressChanged();
    if (!m_queue.isEmpty()) {
        setStatus(QStringLiteral("Fetching lyrics (%1/%2)…").arg(m_done + 1).arg(m_total));
    }
    scheduleNext(m_queue.isEmpty() ? 0 : intervalMs());
}

QString LyricsService::buildSummary(int fetched, int fetchedLrclib, int fetchedNetease,
                                    int fetchedOv, int skipped, int knownMiss, int failed) {
    return LyricsParsers::buildSummary(fetched, fetchedLrclib, fetchedNetease, fetchedOv,
                                       skipped, knownMiss, failed);
}

void LyricsService::completeSave(const QString &sidecarFile, const QString &text,
                                 const QString &source, int *counter) {
    if (writeSidecar(sidecarFile, text)) {
        if (counter) {
            ++(*counter);
        }
        finishTrack(true, false, source);
    } else {
        finishTrack(false, false);
    }
}

void LyricsService::finishJob(const QString &summary) {
    m_hasCurrent = false;
    m_providerResumePending = false;
    m_backoffSecs = 0;
    m_cancelRequested = false;
    // Status first so busyChanged listeners already see the final summary.
    setStatus(summary);
    setBusy(false);
    emit progressChanged();
}

QNetworkRequest LyricsService::lrclibRequest(const QUrl &url) const {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromUtf8(kUserAgent));
    // LRCLIB identifies API clients via these headers (same convention as its
    // web client); identified clients are rate-limited less aggressively.
    request.setRawHeader("X-User-Agent", kClientIdent);
    request.setRawHeader("Lrclib-Client", kClientIdent);
    applyTimeout(request);
    return request;
}

QNetworkRequest LyricsService::neteaseRequest(const QUrl &url) const {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromUtf8(kUserAgent));
    request.setRawHeader("Referer", "https://music.163.com");
    applyTimeout(request);
    return request;
}

QNetworkRequest LyricsService::plainRequest(const QUrl &url) const {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromUtf8(kUserAgent));
    applyTimeout(request);
    return request;
}

QUrl LyricsService::cachedUrl(const QVariantMap &track) const {
    QUrl url(QString::fromUtf8(kLrclibBase) + QStringLiteral("/get"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("artist_name"),
                       track.value(QStringLiteral("artist")).toString().trimmed());
    query.addQueryItem(QStringLiteral("track_name"),
                       track.value(QStringLiteral("title")).toString().trimmed());
    const QString album = track.value(QStringLiteral("album")).toString().trimmed();
    if (!album.isEmpty()) {
        query.addQueryItem(QStringLiteral("album_name"), album);
    }
    const double secs = trackDurationSecs(track);
    if (secs > 0) {
        query.addQueryItem(QStringLiteral("duration"), QString::number(qRound(secs)));
    }
    url.setQuery(query);
    return url;
}

QString LyricsService::lrclibSearchQuery(const QVariantMap &track, int variant) const {
    const QString artist = track.value(QStringLiteral("artist")).toString().trimmed();
    const QString title = track.value(QStringLiteral("title")).toString().trimmed();
    if (artist.isEmpty() || title.isEmpty()) {
        return {};
    }
    if (variant == 0) {
        return artist + QLatin1Char(' ') + title;
    }
    const QString strippedArtist = LyricsParsers::normalizeQueryText(artist);
    const QString strippedTitle = LyricsParsers::normalizeQueryText(title);
    if (variant == 1) {
        if (strippedArtist.isEmpty() || strippedTitle.isEmpty()) {
            return {};
        }
        // Skip when stripping changed nothing — variant 0 already tried it.
        if (strippedArtist.compare(artist, Qt::CaseInsensitive) == 0
            && strippedTitle.compare(title, Qt::CaseInsensitive) == 0) {
            return {};
        }
        return strippedArtist + QLatin1Char(' ') + strippedTitle;
    }
    if (variant == 2) {
        return strippedTitle.isEmpty() ? QString() : strippedTitle;
    }
    return {};
}

QUrl LyricsService::searchUrl(const QVariantMap &track, int variant) const {
    QUrl url(QString::fromUtf8(kLrclibBase) + QStringLiteral("/search"));
    QUrlQuery query;
    if (variant <= 2) {
        const QString q = lrclibSearchQuery(track, variant);
        if (q.isEmpty()) {
            return QUrl();
        }
        query.addQueryItem(QStringLiteral("q"), q);
    } else if (variant == 3) {
        // Structured fallback: no `q`, so track_name/artist_name apply.
        const QString title = LyricsParsers::normalizeQueryText(
            track.value(QStringLiteral("title")).toString());
        const QString artist =
            track.value(QStringLiteral("artist")).toString().trimmed();
        if (title.isEmpty() || artist.isEmpty()) {
            return QUrl();
        }
        query.addQueryItem(QStringLiteral("track_name"), title);
        query.addQueryItem(QStringLiteral("artist_name"), artist);
        const QString album =
            track.value(QStringLiteral("album")).toString().trimmed();
        if (!album.isEmpty()) {
            query.addQueryItem(QStringLiteral("album_name"), album);
        }
    } else {
        return QUrl();
    }
    url.setQuery(query);
    return url;
}

bool LyricsService::requestNextLrclibSearch() {
    while (++m_current.searchVariant <= 3) {
        const QUrl url = searchUrl(m_current.track, m_current.searchVariant);
        if (url.isEmpty()) {
            continue; // duplicate or unusable variant — skip without a request
        }
        startRequest(url, lrclibRequest(url));
        return true;
    }
    return false;
}

double LyricsService::trackDurationSecs(const QVariantMap &track) {
    const double ms = track.value(QStringLiteral("durationMs")).toDouble();
    return ms > 0 ? ms / 1000.0 : -1.0;
}

bool LyricsService::syncedUsable(const QString &synced) {
    if (synced.trimmed().isEmpty()) {
        return false;
    }
    // Require at least one [mm:ss.xx] timestamp line.
    static const QRegularExpression kTimestamp(QStringLiteral("^\\s*\\[\\d{1,3}:\\d{2}"),
                                                              QRegularExpression::MultilineOption);
    return kTimestamp.match(synced).hasMatch();
}

bool LyricsService::plainUsable(const QString &plain) {
    // Plain lyrics must carry real content; tiny fragments are usually
    // "Instrumental" markers or error text.
    return plain.trimmed().length() >= 30;
}

bool LyricsService::durationMatches(double haveSecs, double wantSecs) {
    if (haveSecs < 0 || wantSecs < 0) {
        return true; // unknown duration: accept on timestamp validity alone
    }
    return qAbs(haveSecs - wantSecs) <= kDurationToleranceSecs;
}

bool LyricsService::durationMatchesMs(qint64 haveMs, double wantSecs) {
    if (haveMs <= 0 || wantSecs < 0) {
        return true;
    }
    return qAbs(haveMs / 1000.0 - wantSecs) <= kNeteaseToleranceSecs;
}

qint64 LyricsService::pickNeteaseMatch(const QJsonDocument &doc, const QString &artist,
                                       const QString &title, double wantSecs) {
    return LyricsParsers::pickNeteaseMatch(doc, artist, title, wantSecs);
}

QString LyricsService::neteaseSyncedFromDoc(const QJsonDocument &doc) {
    return LyricsParsers::neteaseSyncedFromDoc(doc);
}

QString LyricsService::ovPlainFromDoc(const QJsonDocument &doc) {
    return LyricsParsers::ovPlainFromDoc(doc);
}

bool LyricsService::writeSidecar(const QString &filePath, const QString &text) {
    if (filePath.isEmpty() || text.trimmed().isEmpty()) {
        return false;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    if (!normalized.endsWith(QLatin1Char('\n'))) {
        normalized.append(QLatin1Char('\n'));
    }
    const bool ok = file.write(normalized.toUtf8()) >= 0;
    file.close();
    return ok && file.error() == QFile::NoError;
}

void LyricsService::openCacheDb() const {
    if (m_cacheDbOpen) {
        return;
    }
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    auto *self = const_cast<LyricsService *>(this);
    self->m_cacheDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                QStringLiteral("lyrics"));
    self->m_cacheDb.setDatabaseName(dir + QStringLiteral("/lyrics.db"));
    if (!self->m_cacheDb.open()) {
        return;
    }
    self->m_cacheDbOpen = true;
    QSqlQuery create(self->m_cacheDb);
    create.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS misses ("
                               "path TEXT PRIMARY KEY, mtime INTEGER NOT NULL, "
                               "checked_at INTEGER NOT NULL)"));
    // Opportunistically drop entries older than the retention window.
    const int days = m_config ? m_config->lyricsNegativeCacheDays() : 30;
    if (days > 0) {
        QSqlQuery purge(self->m_cacheDb);
        purge.exec(QStringLiteral("DELETE FROM misses WHERE checked_at < %1")
                       .arg(QDateTime::currentSecsSinceEpoch()
                            - static_cast<qint64>(days) * 86400));
    }
}

bool LyricsService::negativeHit(const QString &trackPath) const {
    const int days = m_config ? m_config->lyricsNegativeCacheDays() : 30;
    if (days <= 0 || trackPath.isEmpty()) {
        return false;
    }
    openCacheDb();
    if (!m_cacheDbOpen) {
        return false;
    }
    QSqlQuery query(m_cacheDb);
    query.prepare(QStringLiteral("SELECT mtime, checked_at FROM misses WHERE path = ?"));
    query.addBindValue(trackPath);
    if (!query.exec() || !query.next()) {
        return false;
    }
    const qint64 mtime = query.value(0).toLongLong();
    const qint64 checkedAt = query.value(1).toLongLong();
    const qint64 currentMtime = QFileInfo(trackPath).lastModified().toSecsSinceEpoch();
    if (mtime != currentMtime) {
        return false; // file changed since the miss; allow a retry
    }
    const qint64 age = QDateTime::currentSecsSinceEpoch() - checkedAt;
    return age < static_cast<qint64>(days) * 86400;
}

void LyricsService::recordNegative(const QString &trackPath) const {
    if (trackPath.isEmpty()) {
        return;
    }
    openCacheDb();
    if (!m_cacheDbOpen) {
        return;
    }
    QSqlQuery query(m_cacheDb);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO misses (path, mtime, checked_at) "
                                 "VALUES (?, ?, ?)"));
    query.addBindValue(trackPath);
    query.addBindValue(QFileInfo(trackPath).lastModified().toSecsSinceEpoch());
    query.addBindValue(QDateTime::currentSecsSinceEpoch());
    query.exec();
}
