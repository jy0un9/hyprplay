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

#include <algorithm>

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

QVariantList DiscogsService::searchReleases(const QString &artist, const QString &album) const {
    QVariantList results;
    const QString token = loadToken();
    if (token.isEmpty() || artist.trimmed().isEmpty() || album.trimmed().isEmpty()) {
        return results;
    }

    QNetworkAccessManager manager;
    const QUrl url(QString::fromUtf8(kApiBase) + QStringLiteral("/database/search?q=")
                   + QUrl::toPercentEncoding(artist.trimmed() + QStringLiteral(" ")
                                              + album.trimmed())
                   + QStringLiteral("&type=release&per_page=10"));
    QNetworkReply *reply = manager.get(discogsRequest(url, token));
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return results;
    }

    const QJsonArray items = QJsonDocument::fromJson(reply->readAll())
                                 .object().value(QStringLiteral("results")).toArray();
    reply->deleteLater();
    for (const QJsonValue &value : items) {
        const QJsonObject item = value.toObject();
        QVariantMap row;
        row.insert(QStringLiteral("id"), item.value(QStringLiteral("id")).toVariant());
        row.insert(QStringLiteral("title"), item.value(QStringLiteral("title")).toString());
        row.insert(QStringLiteral("year"), item.value(QStringLiteral("year")).toInt());
        QStringList labels;
        for (const QJsonValue &label : item.value(QStringLiteral("label")).toArray())
            labels << label.toString();
        row.insert(QStringLiteral("label"), labels.join(QStringLiteral(", ")));
        QStringList formats;
        for (const QJsonValue &format : item.value(QStringLiteral("format")).toArray())
            formats << format.toString();
        const bool isCd = std::any_of(formats.cbegin(), formats.cend(),
                                      [](const QString &format) {
                                          return format.compare(QStringLiteral("CD"),
                                                                Qt::CaseInsensitive) == 0;
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

void DiscogsService::fetchRelease(const QString &artist, const QString &album,
                                  const QString &albumFolder, quint64 releaseId) {
    if (artist.isEmpty() || album.isEmpty() || albumFolder.isEmpty() || releaseId == 0) {
        setStatus(QStringLiteral("No album selected"));
        emit releaseFetched(artist, album, false);
        return;
    }
    const QString token = loadToken();
    if (token.isEmpty()) {
        setStatus(QStringLiteral("Discogs token not configured — add it in Settings"));
        emit releaseFetched(artist, album, false);
        return;
    }

    setBusy(true);
    setStatus(QStringLiteral("Fetching %1 from Discogs…").arg(album));
    const bool ok = fetchReleaseById(releaseId, artist, album, albumFolder);
    setBusy(false);
    setStatus(ok ? QStringLiteral("Saved Discogs album information for %1").arg(album)
                 : QStringLiteral("Discogs album fetch failed"));
    emit releaseFetched(artist, album, ok);
}

bool DiscogsService::fetchReleaseById(quint64 id, const QString &artist,
                                      const QString &album, const QString &albumFolder) {
    QNetworkAccessManager manager;
    const QUrl url(QString::fromUtf8(kApiBase) + QStringLiteral("/releases/")
                   + QString::number(id));
    QNetworkReply *reply = manager.get(discogsRequest(url, loadToken()));
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return false;
    }
    const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
    reply->deleteLater();

    QStringList text;
    text << release.value(QStringLiteral("title")).toString();
    const int year = release.value(QStringLiteral("year")).toInt();
    if (year > 0) text << QStringLiteral("Year: %1").arg(year);
    const QJsonArray labels = release.value(QStringLiteral("labels")).toArray();
    if (!labels.isEmpty()) {
        QStringList names;
        for (const QJsonValue &value : labels)
            names << value.toObject().value(QStringLiteral("name")).toString();
        text << QStringLiteral("Label: %1").arg(names.join(QStringLiteral(", ")));
    }
    QStringList genres;
    for (const QJsonValue &genre : release.value(QStringLiteral("genres")).toArray())
        genres << genre.toString();
    if (!genres.isEmpty()) text << QStringLiteral("Genres: %1").arg(genres.join(QStringLiteral(", ")));
    const QString notes = stripBbCode(release.value(QStringLiteral("notes")).toString());
    if (!notes.isEmpty()) text << QString() << notes;
    const QJsonArray tracklist = release.value(QStringLiteral("tracklist")).toArray();
    if (!tracklist.isEmpty()) {
        text << QString() << QStringLiteral("Tracklist");
        for (const QJsonValue &value : tracklist) {
            const QJsonObject track = value.toObject();
            text << QStringLiteral("%1. %2").arg(track.value(QStringLiteral("position")).toString(),
                                                  track.value(QStringLiteral("title")).toString());
        }
    }
    QFile infoFile(albumFolder + QStringLiteral("/album-info.txt"));
    if (!infoFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    infoFile.write(text.join(QLatin1Char('\n')).toUtf8());
    infoFile.close();

    const QJsonArray images = release.value(QStringLiteral("images")).toArray();
    QString imageUrl;
    for (const QJsonValue &value : images) {
        if (value.toObject().value(QStringLiteral("type")).toString() == QStringLiteral("primary")) {
            imageUrl = value.toObject().value(QStringLiteral("uri")).toString();
            break;
        }
    }
    if (imageUrl.isEmpty() && !images.isEmpty())
        imageUrl = images.first().toObject().value(QStringLiteral("uri")).toString();
    if (!imageUrl.isEmpty()) {
        QNetworkReply *imageReply = manager.get(QNetworkRequest(QUrl(imageUrl)));
        QObject::connect(imageReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        if (imageReply->error() == QNetworkReply::NoError) {
            QFile imageFile(albumFolder + QStringLiteral("/album.jpg"));
            if (imageFile.open(QIODevice::WriteOnly)) {
                imageFile.write(imageReply->readAll());
                imageFile.close();
            }
        }
        imageReply->deleteLater();
    }
    Q_UNUSED(artist);
    Q_UNUSED(album);
    return true;
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
