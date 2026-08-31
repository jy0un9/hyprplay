#include "MetadataSearchService.h"

#include "ConfigService.h"
#include "DiscogsService.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QtConcurrent>
#include <algorithm>

namespace {

constexpr char kMbUserAgent[] =
    "qt-music/0.1 ( https://musicbrainz.org/doc/MusicBrainz_API )";

bool isUsableName(const QString &name) {
    const QString trimmed = name.trimmed();
    return !trimmed.isEmpty() && trimmed.compare(QStringLiteral("Unknown Artist"), Qt::CaseInsensitive) != 0
           && trimmed.compare(QStringLiteral("Unknown Album"), Qt::CaseInsensitive) != 0;
}

QString firstArtistCredit(const QJsonObject &release) {
    const QJsonArray credits = release.value(QStringLiteral("artist-credit")).toArray();
    QString name;
    for (const QJsonValue &creditValue : credits) {
        const QJsonObject credit = creditValue.toObject();
        QString part = credit.value(QStringLiteral("name")).toString();
        if (part.isEmpty()) {
            part = credit.value(QStringLiteral("artist")).toObject().value(QStringLiteral("name")).toString();
        }
        name += part;
        name += credit.value(QStringLiteral("joinphrase")).toString();
    }
    return name.trimmed();
}

QString yearFromDate(const QString &date) {
    if (date.size() < 4) {
        return {};
    }
    bool ok = false;
    date.left(4).toUInt(&ok);
    return ok ? date.left(4) : QString();
}

QString splitDiscogsAlbum(const QString &rawTitle, const QString &fallbackArtist) {
    const int dash = rawTitle.indexOf(QStringLiteral(" - "));
    if (dash > 0) {
        const QString artist = rawTitle.left(dash).trimmed();
        const QString album = rawTitle.mid(dash + 3).trimmed();
        if (!artist.isEmpty() && !album.isEmpty()) {
            return album;
        }
    }
    Q_UNUSED(fallbackArtist);
    return rawTitle.trimmed();
}

QString splitDiscogsArtist(const QString &rawTitle, const QString &fallbackArtist) {
    const int dash = rawTitle.indexOf(QStringLiteral(" - "));
    if (dash > 0) {
        const QString artist = rawTitle.left(dash).trimmed();
        if (!artist.isEmpty()) {
            return artist;
        }
    }
    return fallbackArtist.trimmed();
}

int albumMatchScore(const QVariantMap &candidate, const QString &album, const QString &artist) {
    const QString title = candidate.value(QStringLiteral("title")).toString();
    const QString detail = candidate.value(QStringLiteral("detail")).toString();
    int score = 0;
    if (!album.isEmpty()) {
        if (title.compare(album, Qt::CaseInsensitive) == 0) {
            score += 1000;
        } else if (title.toLower().contains(album.toLower())) {
            score += 400;
        }
    }
    if (!artist.isEmpty()) {
        if (detail.toLower().contains(artist.toLower()) || title.compare(artist, Qt::CaseInsensitive) == 0) {
            score += 800;
        }
    }
    return score;
}

QVariantMap candidateRow(const QString &source, const QString &id, const QString &title,
                         const QString &detail, const QVariantMap &fields,
                         const QString &coverUrl = {}) {
    QVariantMap row;
    row.insert(QStringLiteral("source"), source);
    row.insert(QStringLiteral("id"), id);
    row.insert(QStringLiteral("title"), title);
    row.insert(QStringLiteral("detail"), detail);
    row.insert(QStringLiteral("fields"), fields);
    if (!coverUrl.isEmpty()) {
        row.insert(QStringLiteral("coverUrl"), coverUrl);
    }
    return row;
}

QByteArray httpGet(const QUrl &url, const QList<QPair<QByteArray, QByteArray>> &headers) {
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromUtf8(kMbUserAgent));
    for (const auto &header : headers) {
        request.setRawHeader(header.first, header.second);
    }
    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const QByteArray body = reply->readAll();
    reply->deleteLater();
    return body;
}

QVariantList searchMusicBrainz(const QString &artist, const QString &album) {
    QVariantList candidates;
    QStringList parts;
    if (isUsableName(album)) {
        QString escaped = album;
        parts << QStringLiteral("release:\"") + escaped.replace(QLatin1Char('"'), QString()) + QLatin1Char('"');
    }
    if (isUsableName(artist)) {
        QString escaped = artist;
        parts << QStringLiteral("artist:\"") + escaped.replace(QLatin1Char('"'), QString()) + QLatin1Char('"');
    }
    if (parts.isEmpty()) {
        return candidates;
    }

    QUrl url(QStringLiteral("https://musicbrainz.org/ws/2/release/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("query"), parts.join(QStringLiteral(" AND ")));
    query.addQueryItem(QStringLiteral("fmt"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("10"));
    url.setQuery(query);

    const QJsonDocument doc = QJsonDocument::fromJson(httpGet(url, {}));
    const QJsonArray releases = doc.object().value(QStringLiteral("releases")).toArray();
    for (const QJsonValue &value : releases) {
        const QJsonObject release = value.toObject();
        const QString id = release.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            continue;
        }
        const QString title = release.value(QStringLiteral("title")).toString();
        const QString artistName = firstArtistCredit(release);
        const QString year = yearFromDate(release.value(QStringLiteral("date")).toString());
        QVariantMap fields;
        fields.insert(QStringLiteral("album"), title);
        fields.insert(QStringLiteral("artist"), artistName);
        fields.insert(QStringLiteral("albumArtist"), artistName);
        fields.insert(QStringLiteral("mb_albumid"), id);
        if (!year.isEmpty()) {
            fields.insert(QStringLiteral("year"), year.toInt());
        }
        const QString detail = year.isEmpty() ? artistName : artistName + QStringLiteral(" · ") + year;
        candidates << candidateRow(QStringLiteral("MusicBrainz"), id, title, detail, fields,
                                   QStringLiteral("https://coverartarchive.org/release/") + id
                                       + QStringLiteral("/front"));
    }
    return candidates;
}

QVariantList searchDiscogs(const QString &artist, const QString &album, const QString &token) {
    QVariantList candidates;
    if (token.isEmpty()) {
        return candidates;
    }

    QUrl url(QStringLiteral("https://api.discogs.com/database/search"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("type"), QStringLiteral("release"));
    query.addQueryItem(QStringLiteral("per_page"), QStringLiteral("8"));
    if (isUsableName(artist)) {
        query.addQueryItem(QStringLiteral("artist"), artist);
    }
    if (isUsableName(album)) {
        query.addQueryItem(QStringLiteral("release_title"), album);
    }
    url.setQuery(query);

    const QList<QPair<QByteArray, QByteArray>> headers = {
        {QByteArray("Authorization"), QByteArray("Discogs token=") + token.toUtf8()}};
    const QJsonDocument doc = QJsonDocument::fromJson(httpGet(url, headers));
    const QJsonArray results = doc.object().value(QStringLiteral("results")).toArray();
    for (const QJsonValue &value : results) {
        const QJsonObject result = value.toObject();
        const QString id = QString::number(result.value(QStringLiteral("id")).toVariant().toULongLong());
        const QString rawTitle = result.value(QStringLiteral("title")).toString();
        const QString artistName = splitDiscogsArtist(rawTitle, artist);
        const QString albumTitle = splitDiscogsAlbum(rawTitle, artist);
        QString year = result.value(QStringLiteral("year")).toString();
        if (year.isEmpty()) {
            year = QString::number(result.value(QStringLiteral("year")).toInt());
            if (year == QStringLiteral("0")) {
                year.clear();
            }
        }
        QVariantMap fields;
        fields.insert(QStringLiteral("album"), albumTitle);
        if (!artistName.isEmpty()) {
            fields.insert(QStringLiteral("artist"), artistName);
            fields.insert(QStringLiteral("albumArtist"), artistName);
        }
        if (!year.isEmpty()) {
            fields.insert(QStringLiteral("year"), year.toInt());
        }
        const QJsonArray genres = result.value(QStringLiteral("genre")).toArray();
        if (!genres.isEmpty()) {
            fields.insert(QStringLiteral("genre"), genres.first().toString());
        }
        const QString coverUrl = result.value(QStringLiteral("cover_image")).toString();
        const QString detail = year.isEmpty() ? artistName : artistName + QStringLiteral(" · ") + year;
        candidates << candidateRow(QStringLiteral("Discogs"), id, albumTitle, detail, fields, coverUrl);
    }
    return candidates;
}

struct SearchInput {
    QString artist;
    QString album;
    QString discogsToken;
};

QVariantList runSearch(const SearchInput &input) {
    QVariantList candidates = searchMusicBrainz(input.artist, input.album);
    candidates.append(searchDiscogs(input.artist, input.album, input.discogsToken));

    std::sort(candidates.begin(), candidates.end(), [&](const QVariant &left, const QVariant &right) {
        const int leftScore = albumMatchScore(left.toMap(), input.album, input.artist);
        const int rightScore = albumMatchScore(right.toMap(), input.album, input.artist);
        return leftScore > rightScore;
    });
    return candidates;
}

QString fieldLabel(const QString &key) {
    static const QHash<QString, QString> labels = {
        {QStringLiteral("title"), QStringLiteral("Title")},
        {QStringLiteral("artist"), QStringLiteral("Artist")},
        {QStringLiteral("albumArtist"), QStringLiteral("Album artist")},
        {QStringLiteral("album"), QStringLiteral("Album")},
        {QStringLiteral("trackNumber"), QStringLiteral("Track #")},
        {QStringLiteral("year"), QStringLiteral("Year")},
        {QStringLiteral("genre"), QStringLiteral("Genre")},
        {QStringLiteral("label"), QStringLiteral("Label")},
        {QStringLiteral("country"), QStringLiteral("Country")},
        {QStringLiteral("catalognum"), QStringLiteral("Catalog #")},
        {QStringLiteral("mb_albumid"), QStringLiteral("MB album ID")},
    };
    return labels.value(key, key);
}

const QStringList &fetchFieldOrder() {
    static const QStringList keys = {
        QStringLiteral("title"),
        QStringLiteral("artist"),
        QStringLiteral("albumArtist"),
        QStringLiteral("album"),
        QStringLiteral("trackNumber"),
        QStringLiteral("year"),
        QStringLiteral("genre"),
        QStringLiteral("label"),
        QStringLiteral("country"),
        QStringLiteral("catalognum"),
        QStringLiteral("mb_albumid"),
    };
    return keys;
}

bool fieldsEqual(const QVariant &current, const QVariant &proposed) {
    if (current.typeId() == QMetaType::Int && proposed.typeId() == QMetaType::Int) {
        return current.toInt() == proposed.toInt();
    }
    return current.toString().trimmed().compare(proposed.toString().trimmed(), Qt::CaseInsensitive) == 0;
}

bool proposedFieldEmpty(const QString &key, const QVariant &proposed) {
    if (key == QStringLiteral("trackNumber")) {
        return proposed.toInt() <= 0;
    }
    return proposed.toString().trimmed().isEmpty();
}

bool defaultCheckedField(const QString &key, const QVariant &current, const QVariant &proposed) {
    const bool empty = current.toString().trimmed().isEmpty()
                       && (key != QStringLiteral("trackNumber") || current.toInt() <= 0);
    const bool differs = !fieldsEqual(current, proposed);

    if (key == QStringLiteral("album") || key == QStringLiteral("artist")
        || key == QStringLiteral("albumArtist") || key == QStringLiteral("title")) {
        return empty && differs;
    }
    return empty || differs;
}

QString firstTagName(const QJsonArray &tags) {
    for (const QJsonValue &value : tags) {
        const QString name = value.toObject().value(QStringLiteral("name")).toString().trimmed();
        if (!name.isEmpty()) {
            return name;
        }
    }
    return {};
}

int parseTrackNumber(const QString &number) {
    const QString trimmed = number.trimmed();
    if (trimmed.isEmpty()) {
        return 0;
    }
    int slash = trimmed.indexOf(QLatin1Char('/'));
    const QString head = slash > 0 ? trimmed.left(slash) : trimmed;
    bool ok = false;
    const int parsed = head.toInt(&ok);
    return ok ? parsed : 0;
}

bool trackTitleMatches(const QString &candidateTitle, const QString &targetTitle) {
    const QString candidate = candidateTitle.trimmed();
    const QString target = targetTitle.trimmed();
    if (candidate.isEmpty() || target.isEmpty()) {
        return false;
    }
    if (candidate.compare(target, Qt::CaseInsensitive) == 0) {
        return true;
    }
    return candidate.contains(target, Qt::CaseInsensitive)
           || target.contains(candidate, Qt::CaseInsensitive);
}

QVariantMap fetchMusicBrainzReleaseDetail(const QString &releaseId, const QString &trackTitle) {
    QVariantMap extra;
    if (releaseId.isEmpty()) {
        return extra;
    }

    QUrl url(QStringLiteral("https://musicbrainz.org/ws/2/release/") + releaseId);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("fmt"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("inc"),
                       QStringLiteral("artist-credits+labels+recordings+release-groups+tags"));
    url.setQuery(query);

    const QJsonObject release = QJsonDocument::fromJson(httpGet(url, {})).object();
    if (release.isEmpty()) {
        return extra;
    }

    QString genre = firstTagName(release.value(QStringLiteral("tags")).toArray());
    if (genre.isEmpty()) {
        genre = firstTagName(
            release.value(QStringLiteral("release-group")).toObject().value(QStringLiteral("tags")).toArray());
    }
    if (!genre.isEmpty()) {
        extra.insert(QStringLiteral("genre"), genre);
    }

    const QString country = release.value(QStringLiteral("country")).toString().trimmed();
    if (!country.isEmpty()) {
        extra.insert(QStringLiteral("country"), country);
    }

    const QJsonArray labelInfo = release.value(QStringLiteral("label-info")).toArray();
    if (!labelInfo.isEmpty()) {
        const QJsonObject firstLabel = labelInfo.first().toObject();
        const QString label =
            firstLabel.value(QStringLiteral("label")).toObject().value(QStringLiteral("name")).toString().trimmed();
        if (!label.isEmpty()) {
            extra.insert(QStringLiteral("label"), label);
        }
        const QString catalog =
            firstLabel.value(QStringLiteral("catalog-number")).toString().trimmed();
        if (!catalog.isEmpty()) {
            extra.insert(QStringLiteral("catalognum"), catalog);
        }
    }

    if (!trackTitle.trimmed().isEmpty()) {
        const QJsonArray media = release.value(QStringLiteral("media")).toArray();
        for (const QJsonValue &mediumValue : media) {
            const QJsonArray tracks = mediumValue.toObject().value(QStringLiteral("tracks")).toArray();
            for (const QJsonValue &trackValue : tracks) {
                const QJsonObject track = trackValue.toObject();
                const QString title = track.value(QStringLiteral("title")).toString();
                if (!trackTitleMatches(title, trackTitle)) {
                    continue;
                }
                extra.insert(QStringLiteral("title"), title.trimmed());
                const int trackNumber = parseTrackNumber(track.value(QStringLiteral("number")).toString());
                if (trackNumber > 0) {
                    extra.insert(QStringLiteral("trackNumber"), trackNumber);
                }
                break;
            }
            if (extra.contains(QStringLiteral("title"))) {
                break;
            }
        }
    }

    return extra;
}

QVariantMap fetchDiscogsReleaseDetail(const QString &releaseId, const QString &trackTitle,
                                      const QString &token) {
    QVariantMap extra;
    if (releaseId.isEmpty() || token.isEmpty()) {
        return extra;
    }

    QUrl url(QStringLiteral("https://api.discogs.com/releases/") + releaseId);
    const QList<QPair<QByteArray, QByteArray>> headers = {
        {QByteArray("Authorization"), QByteArray("Discogs token=") + token.toUtf8()}};
    const QJsonObject release = QJsonDocument::fromJson(httpGet(url, headers)).object();
    if (release.isEmpty()) {
        return extra;
    }

    const QJsonArray genres = release.value(QStringLiteral("genres")).toArray();
    if (!genres.isEmpty()) {
        extra.insert(QStringLiteral("genre"), genres.first().toString());
    } else {
        const QJsonArray styles = release.value(QStringLiteral("styles")).toArray();
        if (!styles.isEmpty()) {
            extra.insert(QStringLiteral("genre"), styles.first().toString());
        }
    }

    const QString country = release.value(QStringLiteral("country")).toString().trimmed();
    if (!country.isEmpty()) {
        extra.insert(QStringLiteral("country"), country);
    }

    const QString year = release.value(QStringLiteral("year")).toString().trimmed();
    if (!year.isEmpty()) {
        extra.insert(QStringLiteral("year"), year.toInt());
    }

    const QJsonArray labels = release.value(QStringLiteral("labels")).toArray();
    if (!labels.isEmpty()) {
        const QJsonObject firstLabel = labels.first().toObject();
        const QString label = firstLabel.value(QStringLiteral("name")).toString().trimmed();
        if (!label.isEmpty()) {
            extra.insert(QStringLiteral("label"), label);
        }
        const QString catalog = firstLabel.value(QStringLiteral("catno")).toString().trimmed();
        if (!catalog.isEmpty()) {
            extra.insert(QStringLiteral("catalognum"), catalog);
        }
    }

    if (!trackTitle.trimmed().isEmpty()) {
        const QJsonArray tracklist = release.value(QStringLiteral("tracklist")).toArray();
        for (const QJsonValue &trackValue : tracklist) {
            const QJsonObject track = trackValue.toObject();
            if (track.value(QStringLiteral("type_")).toString() == QStringLiteral("track")
                && trackTitleMatches(track.value(QStringLiteral("title")).toString(), trackTitle)) {
                extra.insert(QStringLiteral("title"),
                              track.value(QStringLiteral("title")).toString().trimmed());
                const int trackNumber = parseTrackNumber(track.value(QStringLiteral("position")).toString());
                if (trackNumber > 0) {
                    extra.insert(QStringLiteral("trackNumber"), trackNumber);
                }
                break;
            }
        }
    }

    return extra;
}

} // namespace

MetadataSearchService::MetadataSearchService(ConfigService *config, DiscogsService *discogs,
                                               QObject *parent)
    : QObject(parent), m_config(config), m_discogs(discogs) {
    Q_UNUSED(m_config);
}

QVariantMap MetadataSearchService::normalizeCandidateFields(const QVariantMap &fields) {
    QVariantMap normalized = fields;
    if (normalized.contains(QStringLiteral("albumartist"))) {
        normalized.insert(QStringLiteral("albumArtist"), normalized.take(QStringLiteral("albumartist")));
    }
    return normalized;
}

void MetadataSearchService::clear() {
    ++m_detailFetchId;
    m_candidates.clear();
    m_selectedIndex = -1;
    m_currentFields.clear();
    m_fieldChecked.clear();
    emit candidatesChanged();
    emit selectedIndexChanged();
    emit fieldChoicesChanged();
}

void MetadataSearchService::setCurrentFields(const QVariantMap &fields) {
    m_currentFields = fields;
    emit fieldChoicesChanged();
}

void MetadataSearchService::searchRelease(const QString &artist, const QString &album,
                                          const QString &albumArtist) {
    const QString searchArtist = albumArtist.trimmed().isEmpty() ? artist.trimmed() : albumArtist.trimmed();
    if (!isUsableName(searchArtist) && !isUsableName(album)) {
        setStatus(QStringLiteral("Need an artist or album name to search"));
        emit searchFinished(false);
        return;
    }

    m_currentFields.insert(QStringLiteral("artist"), artist);
    m_currentFields.insert(QStringLiteral("album"), album);
    m_currentFields.insert(QStringLiteral("albumArtist"), searchArtist);

    setSearching(true);
    setStatus(QStringLiteral("Searching MusicBrainz and Discogs…"));
    m_candidates.clear();
    m_selectedIndex = -1;
    m_fieldChecked.clear();
    emit candidatesChanged();
    emit selectedIndexChanged();

    SearchInput input;
    input.artist = searchArtist;
    input.album = album.trimmed();
    input.discogsToken = m_discogs ? m_discogs->loadToken() : QString();

    MetadataSearchService *self = this;
    (void)QtConcurrent::run([self, input]() {
        const QVariantList results = runSearch(input);
        QMetaObject::invokeMethod(self, [self, results]() {
            self->m_candidates = results;
            self->setSearching(false);
            if (results.isEmpty()) {
                self->setStatus(QStringLiteral("No matching releases found"));
                emit self->searchFinished(false);
            } else {
                self->setStatus(QStringLiteral("Found %1 release(s) — select one").arg(results.size()));
                self->setSelectedIndex(0);
                emit self->searchFinished(true);
            }
            emit self->candidatesChanged();
        }, Qt::QueuedConnection);
    });
}

void MetadataSearchService::setSelectedIndex(int index) {
    if (index < -1 || index >= m_candidates.size()) {
        index = -1;
    }
    if (m_selectedIndex == index) {
        return;
    }
    m_selectedIndex = index;
    rebuildFieldChoices();
    emit selectedIndexChanged();
    beginReleaseDetailFetch(index);
}

void MetadataSearchService::beginReleaseDetailFetch(int index) {
    if (index < 0 || index >= m_candidates.size()) {
        return;
    }

    const QVariantMap row = m_candidates.at(index).toMap();
    const QString source = row.value(QStringLiteral("source")).toString();
    const QString id = row.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) {
        return;
    }

    const QString trackTitle = m_currentFields.value(QStringLiteral("title")).toString();
    const int fetchId = ++m_detailFetchId;
    setStatus(QStringLiteral("Loading release details…"));

    MetadataSearchService *self = this;
    if (source == QStringLiteral("MusicBrainz")) {
        (void)QtConcurrent::run([self, index, fetchId, id, trackTitle]() {
            const QVariantMap extra = fetchMusicBrainzReleaseDetail(id, trackTitle);
            QMetaObject::invokeMethod(self, [self, index, fetchId, extra]() {
                self->applyReleaseDetailFields(index, fetchId, extra);
            }, Qt::QueuedConnection);
        });
        return;
    }

    if (source == QStringLiteral("Discogs")) {
        const QString token = self->m_discogs ? self->m_discogs->loadToken() : QString();
        (void)QtConcurrent::run([self, index, fetchId, id, trackTitle, token]() {
            const QVariantMap extra = fetchDiscogsReleaseDetail(id, trackTitle, token);
            QMetaObject::invokeMethod(self, [self, index, fetchId, extra]() {
                self->applyReleaseDetailFields(index, fetchId, extra);
            }, Qt::QueuedConnection);
        });
    }
}

void MetadataSearchService::applyReleaseDetailFields(int index, int fetchId,
                                                     const QVariantMap &extra) {
    if (fetchId != m_detailFetchId || index != m_selectedIndex || index < 0
        || index >= m_candidates.size()) {
        return;
    }

    QVariantMap row = m_candidates.at(index).toMap();
    QVariantMap fields = normalizeCandidateFields(row.value(QStringLiteral("fields")).toMap());
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
        if (proposedFieldEmpty(it.key(), it.value())) {
            continue;
        }
        fields.insert(it.key(), it.value());
    }
    row.insert(QStringLiteral("fields"), fields);
    m_candidates[index] = row;
    emit candidatesChanged();
    rebuildFieldChoices();

    if (!m_searching) {
        setStatus(QStringLiteral("Found %1 release(s) — select one").arg(m_candidates.size()));
    }
}

void MetadataSearchService::rebuildFieldChoices() {
    m_fieldChecked.clear();
    if (m_selectedIndex < 0 || m_selectedIndex >= m_candidates.size()) {
        emit fieldChoicesChanged();
        return;
    }

    const QVariantMap candidate = normalizeCandidateFields(
        m_candidates.at(m_selectedIndex).toMap().value(QStringLiteral("fields")).toMap());

    for (const QString &key : fetchFieldOrder()) {
        if (!candidate.contains(key)) {
            continue;
        }
        const QVariant proposed = candidate.value(key);
        if (proposedFieldEmpty(key, proposed)) {
            continue;
        }
        const QVariant current = m_currentFields.value(key);
        m_fieldChecked.insert(key, defaultCheckedField(key, current, proposed));
    }
    emit fieldChoicesChanged();
}

QVariantList MetadataSearchService::fieldChoices() const {
    QVariantList rows;
    if (m_selectedIndex < 0 || m_selectedIndex >= m_candidates.size()) {
        return rows;
    }

    const QVariantMap candidate = normalizeCandidateFields(
        m_candidates.at(m_selectedIndex).toMap().value(QStringLiteral("fields")).toMap());

    for (const QString &key : fetchFieldOrder()) {
        if (!m_fieldChecked.contains(key)) {
            continue;
        }
        QVariantMap row;
        row.insert(QStringLiteral("key"), key);
        row.insert(QStringLiteral("label"), fieldLabel(key));
        row.insert(QStringLiteral("current"), m_currentFields.value(key).toString());
        row.insert(QStringLiteral("proposed"), candidate.value(key).toString());
        row.insert(QStringLiteral("checked"), m_fieldChecked.value(key));
        rows << row;
    }
    return rows;
}

void MetadataSearchService::setFieldChecked(const QString &key, bool checked) {
    if (!m_fieldChecked.contains(key) || m_fieldChecked.value(key) == checked) {
        return;
    }
    m_fieldChecked.insert(key, checked);
    emit fieldChoicesChanged();
}

QVariantMap MetadataSearchService::checkedFields() const {
    if (m_selectedIndex < 0 || m_selectedIndex >= m_candidates.size()) {
        return {};
    }

    const QVariantMap candidate = normalizeCandidateFields(
        m_candidates.at(m_selectedIndex).toMap().value(QStringLiteral("fields")).toMap());

    QVariantMap fields;
    for (auto it = m_fieldChecked.constBegin(); it != m_fieldChecked.constEnd(); ++it) {
        if (!it.value() || !candidate.contains(it.key())) {
            continue;
        }
        fields.insert(it.key(), candidate.value(it.key()));
    }
    return fields;
}

void MetadataSearchService::setSearching(bool searching) {
    if (m_searching == searching) {
        return;
    }
    m_searching = searching;
    emit searchingChanged();
}

void MetadataSearchService::setStatus(const QString &status) {
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}
