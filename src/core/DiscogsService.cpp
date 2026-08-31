#include "DiscogsService.h"

#include "ConfigService.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

namespace {

constexpr char kUserAgent[] = "qt-music/0.1 +local";
constexpr char kApiBase[] = "https://api.discogs.com";

QString secretsFilePath(ConfigService *config) {
    const QString appDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(appDir);
    Q_UNUSED(config);
    return appDir + QStringLiteral("/secrets.toml");
}

QString readTokenFromSecrets(const QString &path) {
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

bool writeTokenToSecrets(const QString &path, const QString &token) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    file.write(QStringLiteral("[discogs]\ntoken = \"%1\"\n").arg(token).toUtf8());
    return true;
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
    request.setRawHeader("Authorization", QByteArray("Discogs token=") + token.toUtf8());
    return request;
}

} // namespace

DiscogsService::DiscogsService(ConfigService *config, QObject *parent)
    : QObject(parent), m_config(config) {}

bool DiscogsService::hasToken() const {
    return !loadToken().isEmpty();
}

QString DiscogsService::loadToken() const {
    const QByteArray env = qgetenv("DISCOGS_TOKEN");
    if (!env.trimmed().isEmpty()) {
        return QString::fromUtf8(env.trimmed());
    }

    const QString appSecrets = secretsFilePath(m_config);
    if (const QString token = readTokenFromSecrets(appSecrets); !token.isEmpty()) {
        return token;
    }

    const QString tuiSecrets =
        QDir::homePath() + QStringLiteral("/.config/tui-audio-player/secrets.toml");
    return readTokenFromSecrets(tuiSecrets);
}

void DiscogsService::setToken(const QString &token) {
    writeTokenToSecrets(secretsFilePath(m_config), token.trimmed());
    emit tokenChanged();
}

QVariantList DiscogsService::searchArtists(const QString &query) const {
    QVariantList results;
    const QString token = loadToken();
    if (token.isEmpty() || query.trimmed().isEmpty()) {
        return results;
    }

    QNetworkAccessManager manager;
    const QUrl url(
        QString::fromUtf8(kApiBase)
        + QStringLiteral("/database/search?q=")
        + QUrl::toPercentEncoding(query.trimmed())
        + QStringLiteral("&type=artist&per_page=10"));
    QNetworkReply *reply = manager.get(discogsRequest(url, token));
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return results;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();
    const QJsonArray items = doc.object().value(QStringLiteral("results")).toArray();
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

void DiscogsService::fetchArtist(const QString &artistName, const QString &artistFolder,
                                 quint64 discogsId) {
    if (artistName.isEmpty() || artistFolder.isEmpty()) {
        setStatus(QStringLiteral("No artist folder selected"));
        emit artistFetched(artistName, false);
        return;
    }

    const QString token = loadToken();
    if (token.isEmpty()) {
        setStatus(QStringLiteral("Discogs token not configured — add it in Settings"));
        emit artistFetched(artistName, false);
        return;
    }

    setBusy(true);
    setStatus(QStringLiteral("Fetching %1 from Discogs…").arg(artistName));

    quint64 id = discogsId;
    if (id == 0) {
        const QVariantList matches = searchArtists(artistName);
        if (matches.isEmpty()) {
            setBusy(false);
            setStatus(QStringLiteral("No Discogs match for \"%1\"").arg(artistName));
            emit artistFetched(artistName, false);
            return;
        }
        id = matches.first().toMap().value(QStringLiteral("id")).toULongLong();
    }

    const bool ok = fetchArtistById(id, artistName, artistFolder);
    setBusy(false);
    setStatus(ok ? QStringLiteral("Saved Discogs profile for %1").arg(artistName)
                 : QStringLiteral("Discogs fetch failed"));
    emit artistFetched(artistName, ok);
}

bool DiscogsService::fetchArtistById(quint64 id, const QString &artistName,
                                      const QString &artistFolder) {
    const QString token = loadToken();
    QNetworkAccessManager manager;

    const QUrl artistUrl(QString::fromUtf8(kApiBase) + QStringLiteral("/artists/") + QString::number(id));
    QNetworkReply *artistReply = manager.get(discogsRequest(artistUrl, token));
    QEventLoop loop;
    QObject::connect(artistReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    if (artistReply->error() != QNetworkReply::NoError) {
        artistReply->deleteLater();
        return false;
    }

    const QJsonObject artist = QJsonDocument::fromJson(artistReply->readAll()).object();
    artistReply->deleteLater();

    const QString profile = stripBbCode(artist.value(QStringLiteral("profile")).toString());
    if (!profile.isEmpty() && !isRedirectProfile(profile)) {
        QFile profileFile(artistFolder + QStringLiteral("/profile.txt"));
        if (profileFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            profileFile.write(profile.toUtf8());
        }
    }

    const QJsonArray images = artist.value(QStringLiteral("images")).toArray();
    QString imageUrl;
    for (const QJsonValue &value : images) {
        const QJsonObject image = value.toObject();
        if (image.value(QStringLiteral("type")).toString() == QStringLiteral("primary")) {
            imageUrl = image.value(QStringLiteral("uri")).toString();
            break;
        }
    }
    if (imageUrl.isEmpty() && !images.isEmpty()) {
        imageUrl = images.first().toObject().value(QStringLiteral("uri")).toString();
    }
    if (imageUrl.isEmpty()) {
        return !profile.isEmpty();
    }

    const QUrl imageRequestUrl(imageUrl);
    if (imageRequestUrl.host() != QStringLiteral("i.discogs.com")
        && imageRequestUrl.host() != QStringLiteral("img.discogs.com")) {
        return !profile.isEmpty();
    }

    QNetworkReply *imageReply = manager.get(QNetworkRequest(imageRequestUrl));
    QObject::connect(imageReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    if (imageReply->error() != QNetworkReply::NoError) {
        imageReply->deleteLater();
        return !profile.isEmpty();
    }

    const QByteArray bytes = imageReply->readAll();
    imageReply->deleteLater();
    const QString ext = imageRequestUrl.path().endsWith(QStringLiteral(".png"))
                            ? QStringLiteral("png")
                            : QStringLiteral("jpg");
    const QString dest = artistFolder + QStringLiteral("/artist.") + ext;
    const QString temp = artistFolder + QStringLiteral("/artist.tmp.") + ext;
    QFile tempFile(temp);
    if (!tempFile.open(QIODevice::WriteOnly)) {
        return !profile.isEmpty();
    }
    tempFile.write(bytes);
    tempFile.close();
    QFile::remove(dest);
    if (!QFile::rename(temp, dest)) {
        QFile::remove(temp);
        return !profile.isEmpty();
    }

    Q_UNUSED(artistName);
    return true;
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
