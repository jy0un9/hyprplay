#include "DiscogsService.h"

#include "ConfigService.h"
#include "SecretsStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>

namespace {

constexpr char kUserAgent[] = "qt-music/0.1 +local";
constexpr char kApiBase[] = "https://api.discogs.com";

QString readTokenFromLegacySecrets(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    const QString content = QString::fromUtf8(file.readAll());
    static const QRegularExpression re(
        QStringLiteral("\\[discogs\\][\\s\\S]*?token\\s*=\\s*\"([^\"]+)\""));
    const QRegularExpressionMatch match = re.match(content);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }
    return {};
}

QString stripBbCode(const QString &input) {
    QString text = input;
    text.replace(QRegularExpression(QStringLiteral("\\[[^\\]]+\\]")), QString());
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    return text.trimmed();
}

bool isRedirectProfile(const QString &profile) {
    const QString lower = profile.toLower();
    return lower.contains(QStringLiteral("please use")) || lower.contains(QStringLiteral("please see"))
           || lower.contains(QStringLiteral("see artist"));
}

QNetworkRequest discogsRequest(const QUrl &url, const QString &token) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromUtf8(kUserAgent));
    request.setTransferTimeout(20000);
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization", QByteArray("Discogs token=") + token.toUtf8());
    }
    return request;
}

QString pickPrimaryImageUrl(const QJsonArray &images) {
    QString imageUrl;
    for (const QJsonValue &value : images) {
        if (value.toObject().value(QStringLiteral("type")).toString() == QStringLiteral("primary")) {
            imageUrl = value.toObject().value(QStringLiteral("uri")).toString();
            break;
        }
    }
    if (imageUrl.isEmpty() && !images.isEmpty()) {
        imageUrl = images.first().toObject().value(QStringLiteral("uri")).toString();
    }
    return imageUrl;
}

QVariantList parseArtistSearchBody(const QByteArray &body) {
    QVariantList results;
    const QJsonArray items =
        QJsonDocument::fromJson(body).object().value(QStringLiteral("results")).toArray();
    for (const QJsonValue &value : items) {
        const QJsonObject item = value.toObject();
        QVariantMap row;
        row.insert(QStringLiteral("id"), item.value(QStringLiteral("id")).toVariant());
        row.insert(QStringLiteral("title"), item.value(QStringLiteral("title")).toString());
        QString detail;
        const QJsonArray formats = item.value(QStringLiteral("format")).toArray();
        if (!formats.isEmpty()) {
            detail = formats.first().toString();
        }
        row.insert(QStringLiteral("detail"), detail);
        results << row;
    }
    return results;
}

QVariantList parseReleaseSearchBody(const QByteArray &body) {
    QVariantList results;
    const QJsonArray items =
        QJsonDocument::fromJson(body).object().value(QStringLiteral("results")).toArray();
    for (const QJsonValue &value : items) {
        const QJsonObject item = value.toObject();
        QVariantMap row;
        row.insert(QStringLiteral("id"), item.value(QStringLiteral("id")).toVariant());
        row.insert(QStringLiteral("title"), item.value(QStringLiteral("title")).toString());
        row.insert(QStringLiteral("year"), item.value(QStringLiteral("year")).toInt());
        QStringList labels;
        for (const QJsonValue &label : item.value(QStringLiteral("label")).toArray()) {
            labels << label.toString();
        }
        row.insert(QStringLiteral("label"), labels.join(QStringLiteral(", ")));
        QStringList formats;
        for (const QJsonValue &format : item.value(QStringLiteral("format")).toArray()) {
            formats << format.toString();
        }
        const bool isCd = std::any_of(formats.cbegin(), formats.cend(), [](const QString &format) {
            return format.compare(QStringLiteral("CD"), Qt::CaseInsensitive) == 0;
        });
        if (!isCd) {
            continue;
        }
        row.insert(QStringLiteral("format"), formats.join(QStringLiteral(", ")));
        row.insert(QStringLiteral("country"), item.value(QStringLiteral("country")).toString());
        results << row;
    }
    return results;
}

} // namespace

DiscogsService::DiscogsService(ConfigService *config, QObject *parent)
    : QObject(parent), m_config(config) {
    m_network = new QNetworkAccessManager(this);
}

DiscogsService::~DiscogsService() {
    abortActiveReply();
}

bool DiscogsService::hasToken() const {
    return !loadToken().isEmpty();
}

QString DiscogsService::loadToken() const {
    const QByteArray env = qgetenv("DISCOGS_TOKEN");
    if (!env.trimmed().isEmpty()) {
        return QString::fromUtf8(env.trimmed());
    }

    if (const QString token = SecretsStore::load(SecretsStore::Key::DiscogsToken);
        !token.isEmpty()) {
        return token;
    }

    // One-shot migration from the older tui-audio-player secrets file.
    const QString tuiSecrets =
        QDir::homePath() + QStringLiteral("/.config/tui-audio-player/secrets.toml");
    const QString legacy = readTokenFromLegacySecrets(tuiSecrets);
    if (!legacy.isEmpty()) {
        SecretsStore::store(SecretsStore::Key::DiscogsToken, legacy);
        return SecretsStore::load(SecretsStore::Key::DiscogsToken);
    }
    return {};
}

void DiscogsService::setToken(const QString &token) {
    SecretsStore::store(SecretsStore::Key::DiscogsToken, token.trimmed());
    emit tokenChanged();
}

void DiscogsService::abortActiveReply() {
    if (!m_reply) {
        return;
    }
    QObject::disconnect(m_reply, nullptr, this, nullptr);
    m_reply->abort();
    m_reply->deleteLater();
    m_reply = nullptr;
}

void DiscogsService::finishOp() {
    m_op = Op::None;
    setBusy(false);
}

void DiscogsService::failArtist(const QString &message) {
    setStatus(message);
    const QString name = m_artistName;
    finishOp();
    emit artistFetched(name, false);
}

void DiscogsService::failRelease(const QString &message) {
    setStatus(message);
    const QString artist = m_albumArtist;
    const QString album = m_albumName;
    finishOp();
    emit releaseFetched(artist, album, false);
}

void DiscogsService::startGet(const QUrl &url, bool authenticated) {
    abortActiveReply();
    const QString token = authenticated ? loadToken() : QString();
    m_reply = m_network->get(discogsRequest(url, token));
    connect(m_reply, &QNetworkReply::finished, this, &DiscogsService::onReplyFinished);
}

void DiscogsService::searchArtists(const QString &query) {
    const QString token = loadToken();
    if (token.isEmpty() || query.trimmed().isEmpty()) {
        emit artistsSearchFinished({});
        return;
    }

    m_op = Op::SearchArtists;
    setBusy(true);
    setStatus(QStringLiteral("Searching Discogs artists…"));
    const QUrl url(QString::fromUtf8(kApiBase) + QStringLiteral("/database/search?q=")
                   + QUrl::toPercentEncoding(query.trimmed())
                   + QStringLiteral("&type=artist&per_page=10"));
    startGet(url, true);
}

void DiscogsService::searchReleases(const QString &artist, const QString &album) {
    const QString token = loadToken();
    if (token.isEmpty() || artist.trimmed().isEmpty() || album.trimmed().isEmpty()) {
        emit releasesSearchFinished({});
        return;
    }

    m_op = Op::SearchReleases;
    setBusy(true);
    setStatus(QStringLiteral("Searching Discogs releases…"));
    const QUrl url(QString::fromUtf8(kApiBase) + QStringLiteral("/database/search?q=")
                   + QUrl::toPercentEncoding(artist.trimmed() + QStringLiteral(" ")
                                             + album.trimmed())
                   + QStringLiteral("&type=release&per_page=10"));
    startGet(url, true);
}

void DiscogsService::fetchArtist(const QString &artistName, const QString &artistFolder,
                                 quint64 discogsId) {
    if (artistName.isEmpty() || artistFolder.isEmpty()) {
        setStatus(QStringLiteral("No artist folder selected"));
        emit artistFetched(artistName, false);
        return;
    }

    if (loadToken().isEmpty()) {
        setStatus(QStringLiteral("Discogs token not configured — add it in Settings"));
        emit artistFetched(artistName, false);
        return;
    }

    m_artistName = artistName;
    m_artistFolder = artistFolder;
    m_artistProfileOk = false;
    m_pendingImageUrl.clear();
    setBusy(true);
    setStatus(QStringLiteral("Fetching %1 from Discogs…").arg(artistName));

    if (discogsId != 0) {
        startArtistDetail(discogsId);
        return;
    }

    m_op = Op::FetchArtistSearch;
    const QUrl url(QString::fromUtf8(kApiBase) + QStringLiteral("/database/search?q=")
                   + QUrl::toPercentEncoding(artistName.trimmed())
                   + QStringLiteral("&type=artist&per_page=10"));
    startGet(url, true);
}

void DiscogsService::fetchRelease(const QString &artist, const QString &album,
                                  const QString &albumFolder, quint64 releaseId) {
    if (artist.isEmpty() || album.isEmpty() || albumFolder.isEmpty() || releaseId == 0) {
        setStatus(QStringLiteral("No album selected"));
        emit releaseFetched(artist, album, false);
        return;
    }
    if (loadToken().isEmpty()) {
        setStatus(QStringLiteral("Discogs token not configured — add it in Settings"));
        emit releaseFetched(artist, album, false);
        return;
    }

    m_albumArtist = artist;
    m_albumName = album;
    m_albumFolder = albumFolder;
    m_releaseInfoOk = false;
    m_pendingImageUrl.clear();
    setBusy(true);
    setStatus(QStringLiteral("Fetching %1 from Discogs…").arg(album));
    startReleaseDetail(releaseId);
}

void DiscogsService::startArtistDetail(quint64 id) {
    m_op = Op::FetchArtistDetail;
    const QUrl url(QString::fromUtf8(kApiBase) + QStringLiteral("/artists/") + QString::number(id));
    startGet(url, true);
}

void DiscogsService::startReleaseDetail(quint64 id) {
    m_op = Op::FetchReleaseDetail;
    const QUrl url(QString::fromUtf8(kApiBase) + QStringLiteral("/releases/") + QString::number(id));
    startGet(url, true);
}

void DiscogsService::onReplyFinished() {
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (!reply) {
        return;
    }
    reply->deleteLater();

    const Op op = m_op;
    const bool netOk = reply->error() == QNetworkReply::NoError;
    const QByteArray body = reply->readAll();

    switch (op) {
    case Op::SearchArtists: {
        const QVariantList results = netOk ? parseArtistSearchBody(body) : QVariantList{};
        finishOp();
        setStatus(results.isEmpty() ? QStringLiteral("No Discogs artists found")
                                    : QStringLiteral("Found %1 artist(s)").arg(results.size()));
        emit artistsSearchFinished(results);
        break;
    }
    case Op::SearchReleases: {
        const QVariantList results = netOk ? parseReleaseSearchBody(body) : QVariantList{};
        finishOp();
        setStatus(results.isEmpty() ? QStringLiteral("No Discogs releases found")
                                    : QStringLiteral("Found %1 release(s)").arg(results.size()));
        emit releasesSearchFinished(results);
        break;
    }
    case Op::FetchArtistSearch: {
        if (!netOk) {
            failArtist(QStringLiteral("Discogs fetch failed"));
            break;
        }
        const QVariantList matches = parseArtistSearchBody(body);
        if (matches.isEmpty()) {
            failArtist(QStringLiteral("No Discogs match for \"%1\"").arg(m_artistName));
            break;
        }
        const quint64 id = matches.first().toMap().value(QStringLiteral("id")).toULongLong();
        startArtistDetail(id);
        break;
    }
    case Op::FetchArtistDetail: {
        if (!netOk) {
            failArtist(QStringLiteral("Discogs fetch failed"));
            break;
        }
        const QJsonObject artist = QJsonDocument::fromJson(body).object();
        const QString profile = stripBbCode(artist.value(QStringLiteral("profile")).toString());
        if (!profile.isEmpty() && !isRedirectProfile(profile)) {
            QFile profileFile(m_artistFolder + QStringLiteral("/profile.txt"));
            if (profileFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                profileFile.write(profile.toUtf8());
                m_artistProfileOk = true;
            }
        }

        const QString imageUrl = pickPrimaryImageUrl(artist.value(QStringLiteral("images")).toArray());
        if (imageUrl.isEmpty()) {
            const bool ok = m_artistProfileOk;
            setStatus(ok ? QStringLiteral("Saved Discogs profile for %1").arg(m_artistName)
                         : QStringLiteral("Discogs fetch failed"));
            const QString name = m_artistName;
            finishOp();
            emit artistFetched(name, ok);
            break;
        }

        const QUrl imageRequestUrl(imageUrl);
        if (imageRequestUrl.host() != QStringLiteral("i.discogs.com")
            && imageRequestUrl.host() != QStringLiteral("img.discogs.com")) {
            const bool ok = m_artistProfileOk;
            setStatus(ok ? QStringLiteral("Saved Discogs profile for %1").arg(m_artistName)
                         : QStringLiteral("Discogs fetch failed"));
            const QString name = m_artistName;
            finishOp();
            emit artistFetched(name, ok);
            break;
        }

        m_pendingImageUrl = imageUrl;
        m_op = Op::FetchArtistImage;
        startGet(imageRequestUrl, false);
        break;
    }
    case Op::FetchArtistImage: {
        bool imageOk = false;
        if (netOk) {
            const QUrl imageRequestUrl(m_pendingImageUrl);
            const QString ext = imageRequestUrl.path().endsWith(QStringLiteral(".png"))
                                    ? QStringLiteral("png")
                                    : QStringLiteral("jpg");
            const QString dest = m_artistFolder + QStringLiteral("/artist.") + ext;
            const QString temp = m_artistFolder + QStringLiteral("/artist.tmp.") + ext;
            QFile tempFile(temp);
            if (tempFile.open(QIODevice::WriteOnly)) {
                tempFile.write(body);
                tempFile.close();
                QFile::remove(dest);
                imageOk = QFile::rename(temp, dest);
                if (!imageOk) {
                    QFile::remove(temp);
                }
            }
        }
        const bool ok = m_artistProfileOk || imageOk;
        setStatus(ok ? QStringLiteral("Saved Discogs profile for %1").arg(m_artistName)
                     : QStringLiteral("Discogs fetch failed"));
        const QString name = m_artistName;
        finishOp();
        emit artistFetched(name, ok);
        break;
    }
    case Op::FetchReleaseDetail: {
        if (!netOk) {
            failRelease(QStringLiteral("Discogs album fetch failed"));
            break;
        }
        const QJsonObject release = QJsonDocument::fromJson(body).object();
        QStringList text;
        text << release.value(QStringLiteral("title")).toString();
        const int year = release.value(QStringLiteral("year")).toInt();
        if (year > 0) {
            text << QStringLiteral("Year: %1").arg(year);
        }
        const QJsonArray labels = release.value(QStringLiteral("labels")).toArray();
        if (!labels.isEmpty()) {
            QStringList names;
            for (const QJsonValue &value : labels) {
                names << value.toObject().value(QStringLiteral("name")).toString();
            }
            text << QStringLiteral("Label: %1").arg(names.join(QStringLiteral(", ")));
        }
        QStringList genres;
        for (const QJsonValue &genre : release.value(QStringLiteral("genres")).toArray()) {
            genres << genre.toString();
        }
        if (!genres.isEmpty()) {
            text << QStringLiteral("Genres: %1").arg(genres.join(QStringLiteral(", ")));
        }
        const QString notes = stripBbCode(release.value(QStringLiteral("notes")).toString());
        if (!notes.isEmpty()) {
            text << QString() << notes;
        }
        const QJsonArray tracklist = release.value(QStringLiteral("tracklist")).toArray();
        if (!tracklist.isEmpty()) {
            text << QString() << QStringLiteral("Tracklist");
            for (const QJsonValue &value : tracklist) {
                const QJsonObject track = value.toObject();
                text << QStringLiteral("%1. %2")
                            .arg(track.value(QStringLiteral("position")).toString(),
                                 track.value(QStringLiteral("title")).toString());
            }
        }
        QFile infoFile(m_albumFolder + QStringLiteral("/album-info.txt"));
        if (infoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            infoFile.write(text.join(QLatin1Char('\n')).toUtf8());
            infoFile.close();
            m_releaseInfoOk = true;
        } else {
            failRelease(QStringLiteral("Discogs album fetch failed"));
            break;
        }

        const QString imageUrl = pickPrimaryImageUrl(release.value(QStringLiteral("images")).toArray());
        if (imageUrl.isEmpty()) {
            setStatus(QStringLiteral("Saved Discogs album information for %1").arg(m_albumName));
            const QString artist = m_albumArtist;
            const QString album = m_albumName;
            finishOp();
            emit releaseFetched(artist, album, true);
            break;
        }

        m_pendingImageUrl = imageUrl;
        m_op = Op::FetchReleaseImage;
        startGet(QUrl(imageUrl), false);
        break;
    }
    case Op::FetchReleaseImage: {
        if (netOk) {
            QFile imageFile(m_albumFolder + QStringLiteral("/album.jpg"));
            if (imageFile.open(QIODevice::WriteOnly)) {
                imageFile.write(body);
                imageFile.close();
            }
        }
        setStatus(QStringLiteral("Saved Discogs album information for %1").arg(m_albumName));
        const QString artist = m_albumArtist;
        const QString album = m_albumName;
        finishOp();
        emit releaseFetched(artist, album, m_releaseInfoOk);
        break;
    }
    case Op::None:
        break;
    }
}

void DiscogsService::setBusy(bool busy) {
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

void DiscogsService::setStatus(const QString &status) {
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}
