#include "MetadataSearchService.h"

#include "ConfigService.h"
#include "DiscogsService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QVector>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>

namespace {

constexpr char kMbUserAgent[] =
    "hyprplay/0.1.2 ( https://github.com/jy0un9/hyprplay )";

bool isUsableName(const QString &name) {
    const QString trimmed = name.trimmed();
    return !trimmed.isEmpty()
           && trimmed.compare(QStringLiteral("Unknown Artist"), Qt::CaseInsensitive) != 0
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

int albumMatchScore(const QVariantMap &candidate, const QString &album, const QString &artist,
                    const QString &edition = {}, int localTrackCount = 0) {
    const QString title = candidate.value(QStringLiteral("title")).toString();
    const QString detail = candidate.value(QStringLiteral("detail")).toString();
    const QString titleLower = title.toLower();
    const QString detailLower = detail.toLower();
    const QString albumLower = album.toLower();
    const QString editionLower = edition.trimmed().toLower();
    int score = 0;
    if (!album.isEmpty()) {
        if (title.compare(album, Qt::CaseInsensitive) == 0) {
            score += 1000;
        } else if (titleLower.contains(albumLower)) {
            score += 400;
        }
    }
    if (!artist.isEmpty()) {
        if (detailLower.contains(artist.toLower())
            || title.compare(artist, Qt::CaseInsensitive) == 0) {
            score += 800;
        }
    }
    if (!editionLower.isEmpty()) {
        const QStringList tokens =
            editionLower.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        for (const QString &token : tokens) {
            if (token.size() < 3) {
                continue;
            }
            if (titleLower.contains(token) || detailLower.contains(token)) {
                score += 250;
            }
        }
    } else {
        static const QStringList editionWords = {
            QStringLiteral("deluxe"),      QStringLiteral("expanded"),
            QStringLiteral("anniversary"), QStringLiteral("remaster"),
            QStringLiteral("special"),     QStringLiteral("explicit"),
            QStringLiteral("extended"),    QStringLiteral("bonus")};
        for (const QString &word : editionWords) {
            const bool want = albumLower.contains(word);
            const bool have = titleLower.contains(word) || detailLower.contains(word);
            if (want && have) {
                score += 300;
            } else if (!want && have) {
                score -= 40;
            }
        }
    }

    int remoteTracks = candidate.value(QStringLiteral("trackCount")).toInt();
    if (remoteTracks <= 0) {
        remoteTracks = candidate.value(QStringLiteral("fields")).toMap()
                           .value(QStringLiteral("trackCount")).toInt();
    }
    if (localTrackCount > 0 && remoteTracks > 0) {
        const int delta = qAbs(remoteTracks - localTrackCount);
        if (delta == 0) {
            score += 2000;
        } else if (delta == 1) {
            score += 1200;
        } else if (delta == 2) {
            score += 700;
        } else if (delta <= 4) {
            score += 300;
        } else if (delta <= 8) {
            score -= 100;
        } else {
            score -= 400;
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

QString musicBrainzFormatClause(const QStringList &formats) {
    QStringList clauses;
    for (const QString &raw : formats) {
        const QString format = raw.trimmed().toLower();
        if (format == QLatin1String("cd")) {
            clauses << QStringLiteral("format:CD");
        } else if (format == QLatin1String("vinyl")) {
            clauses << QStringLiteral("format:Vinyl");
        } else if (format == QLatin1String("cassette") || format == QLatin1String("tape")) {
            clauses << QStringLiteral("format:Cassette");
        } else if (format == QLatin1String("digital")) {
            clauses << QStringLiteral("format:\"Digital Media\"");
        }
    }
    if (clauses.isEmpty()) {
        return {};
    }
    if (clauses.size() == 1) {
        return clauses.first();
    }
    return QLatin1Char('(') + clauses.join(QStringLiteral(" OR ")) + QLatin1Char(')');
}

QUrl musicBrainzSearchUrl(const QString &artist, const QString &album,
                          const QStringList &formats, const QString &edition) {
    QStringList parts;
    if (isUsableName(album)) {
        QString escaped = album;
        parts << QStringLiteral("release:\"") + escaped.replace(QLatin1Char('"'), QString())
                    + QLatin1Char('"');
    }
    if (isUsableName(artist)) {
        QString escaped = artist;
        parts << QStringLiteral("artist:\"") + escaped.replace(QLatin1Char('"'), QString())
                    + QLatin1Char('"');
    }
    const QString formatClause = musicBrainzFormatClause(formats);
    if (!formatClause.isEmpty()) {
        parts << formatClause;
    }
    const QString trimmedEdition = edition.trimmed();
    if (!trimmedEdition.isEmpty()) {
        QString escaped = trimmedEdition;
        escaped.replace(QLatin1Char('"'), QString());
        parts << QLatin1Char('(') + escaped + QLatin1Char(')');
    }
    if (parts.isEmpty()) {
        return {};
    }

    QUrl url(QStringLiteral("https://musicbrainz.org/ws/2/release/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("query"), parts.join(QStringLiteral(" AND ")));
    query.addQueryItem(QStringLiteral("fmt"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("25"));
    url.setQuery(query);
    return url;
}

QUrl discogsSearchUrl(const QString &artist, const QString &album,
                      const QStringList &formats, const QString &edition) {
    QStringList parts;
    if (isUsableName(artist)) {
        parts << artist;
    }
    if (isUsableName(album)) {
        parts << album;
    }
    if (!edition.trimmed().isEmpty()) {
        parts << edition.trimmed();
    }
    if (parts.isEmpty()) {
        return {};
    }

    QUrl url(QStringLiteral("https://api.discogs.com/database/search"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), parts.join(QLatin1Char(' ')));
    query.addQueryItem(QStringLiteral("type"), QStringLiteral("release"));
    query.addQueryItem(QStringLiteral("per_page"), QStringLiteral("50"));
    if (formats.size() == 1) {
        const QString format = formats.first().trimmed().toLower();
        if (format == QLatin1String("cd")) {
            query.addQueryItem(QStringLiteral("format"), QStringLiteral("CD"));
        } else if (format == QLatin1String("vinyl")) {
            query.addQueryItem(QStringLiteral("format"), QStringLiteral("Vinyl"));
        } else if (format == QLatin1String("cassette") || format == QLatin1String("tape")) {
            query.addQueryItem(QStringLiteral("format"), QStringLiteral("Cassette"));
        } else if (format == QLatin1String("digital")) {
            query.addQueryItem(QStringLiteral("format"), QStringLiteral("File"));
        }
    }
    url.setQuery(query);
    return url;
}

QVariantList parseMusicBrainzSearch(const QByteArray &body) {
    QVariantList candidates;
    const QJsonArray releases =
        QJsonDocument::fromJson(body).object().value(QStringLiteral("releases")).toArray();
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

        QStringList detailParts;
        if (!artistName.isEmpty()) {
            detailParts << artistName;
        }
        if (!year.isEmpty()) {
            detailParts << year;
        }
        int trackCount = 0;
        QStringList formats;
        const QJsonArray media = release.value(QStringLiteral("media")).toArray();
        for (const QJsonValue &mediumValue : media) {
            const QJsonObject medium = mediumValue.toObject();
            const QString format = medium.value(QStringLiteral("format")).toString().trimmed();
            if (!format.isEmpty() && !formats.contains(format, Qt::CaseInsensitive)) {
                formats << format;
            }
            trackCount += medium.value(QStringLiteral("track-count")).toInt();
        }
        if (!formats.isEmpty()) {
            detailParts << formats.join(QStringLiteral("/"));
            fields.insert(QStringLiteral("format"), formats.join(QStringLiteral(", ")));
        }
        if (trackCount > 0) {
            detailParts << QStringLiteral("%1 tracks").arg(trackCount);
            fields.insert(QStringLiteral("trackCount"), trackCount);
        }
        const QString country = release.value(QStringLiteral("country")).toString().trimmed();
        if (!country.isEmpty()) {
            detailParts << country;
            fields.insert(QStringLiteral("country"), country);
        }

        QVariantMap row = candidateRow(QStringLiteral("MusicBrainz"), id, title,
                                       detailParts.join(QStringLiteral(" · ")), fields,
                                       QStringLiteral("https://coverartarchive.org/release/") + id
                                           + QStringLiteral("/front"));
        if (trackCount > 0) {
            row.insert(QStringLiteral("trackCount"), trackCount);
        }
        if (!formats.isEmpty()) {
            row.insert(QStringLiteral("format"), formats.join(QStringLiteral(", ")));
        }
        candidates << row;
    }
    return candidates;
}

QVariantList parseDiscogsSearch(const QByteArray &body, const QString &artist,
                                const QStringList &formatFilters = {}) {
    QVariantList candidates;
    const QJsonArray results =
        QJsonDocument::fromJson(body).object().value(QStringLiteral("results")).toArray();
    for (const QJsonValue &value : results) {
        const QJsonObject result = value.toObject();
        QStringList formats;
        for (const QJsonValue &format : result.value(QStringLiteral("format")).toArray()) {
            formats << format.toString();
        }
        if (!formatFilters.isEmpty()) {
            bool matched = false;
            for (const QString &filter : formatFilters) {
                const QString needle = filter.trimmed().toLower();
                for (const QString &raw : formats) {
                    const QString format = raw.toLower();
                    if (needle == QLatin1String("cd")
                        && (format == QLatin1String("cd") || format.contains(QLatin1String("cd")))) {
                        matched = true;
                    } else if (needle == QLatin1String("vinyl")
                               && (format.contains(QLatin1String("vinyl"))
                                   || format == QLatin1String("lp"))) {
                        matched = true;
                    } else if ((needle == QLatin1String("cassette") || needle == QLatin1String("tape"))
                               && (format.contains(QLatin1String("cassette"))
                                   || format.contains(QLatin1String("tape")))) {
                        matched = true;
                    } else if (needle == QLatin1String("digital")
                               && (format.contains(QLatin1String("file"))
                                   || format.contains(QLatin1String("flac"))
                                   || format.contains(QLatin1String("mp3"))
                                   || format.contains(QLatin1String("digital")))) {
                        matched = true;
                    }
                    if (matched) {
                        break;
                    }
                }
                if (matched) {
                    break;
                }
            }
            if (!matched) {
                continue;
            }
        }

        const QString id =
            QString::number(result.value(QStringLiteral("id")).toVariant().toULongLong());
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
        if (!formats.isEmpty()) {
            fields.insert(QStringLiteral("format"), formats.join(QStringLiteral(", ")));
        }
        const QString coverUrl = result.value(QStringLiteral("cover_image")).toString();
        QStringList detailParts;
        if (!artistName.isEmpty()) {
            detailParts << artistName;
        }
        if (!year.isEmpty()) {
            detailParts << year;
        }
        if (!formats.isEmpty()) {
            detailParts << formats.join(QStringLiteral("/"));
        }
        const QString country = result.value(QStringLiteral("country")).toString().trimmed();
        if (!country.isEmpty()) {
            detailParts << country;
        }
        QVariantMap row = candidateRow(QStringLiteral("Discogs"), id, albumTitle,
                                       detailParts.join(QStringLiteral(" · ")), fields, coverUrl);
        if (!formats.isEmpty()) {
            row.insert(QStringLiteral("format"), formats.join(QStringLiteral(", ")));
        }
        const int mediaCount = result.value(QStringLiteral("format_quantity")).toInt();
        if (mediaCount > 0) {
            row.insert(QStringLiteral("mediaCount"), mediaCount);
            detailParts << QStringLiteral("%1 media").arg(mediaCount);
            row.insert(QStringLiteral("detail"), detailParts.join(QStringLiteral(" · ")));
        }
        candidates << row;
    }
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
        QStringLiteral("title"),      QStringLiteral("artist"),
        QStringLiteral("albumArtist"), QStringLiteral("album"),
        QStringLiteral("trackNumber"), QStringLiteral("year"),
        QStringLiteral("genre"),      QStringLiteral("label"),
        QStringLiteral("country"),    QStringLiteral("catalognum"),
        QStringLiteral("mb_albumid"),
    };
    return keys;
}

bool fieldsEqual(const QVariant &current, const QVariant &proposed) {
    if (current.typeId() == QMetaType::Int && proposed.typeId() == QMetaType::Int) {
        return current.toInt() == proposed.toInt();
    }
    return current.toString().trimmed().compare(proposed.toString().trimmed(),
                                                Qt::CaseInsensitive)
           == 0;
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
    const int slash = trimmed.indexOf(QLatin1Char('/'));
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

QString stripTitleDecorations(QString title) {
    static const QRegularExpression inner(QStringLiteral("[\\(\\[{][^()\\[\\]{}]*[\\)\\]}]"));
    QString previous;
    do {
        previous = title;
        title.remove(inner);
    } while (title != previous);
    return title.simplified();
}

QString normalizeTitleForMatch(const QString &title) {
    QString normalized = stripTitleDecorations(title).toLower();
    normalized.remove(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N} ]"),
                                         QRegularExpression::UseUnicodePropertiesOption));
    return normalized.simplified();
}

int titleMatchScore(const QString &localTitle, const QString &officialTitle) {
    const QString local = localTitle.trimmed();
    const QString official = officialTitle.trimmed();
    if (local.isEmpty() || official.isEmpty()) {
        return 0;
    }
    if (local.compare(official, Qt::CaseInsensitive) == 0) {
        return 100;
    }
    const QString localNorm = normalizeTitleForMatch(local);
    const QString officialNorm = normalizeTitleForMatch(official);
    if (localNorm.isEmpty() || officialNorm.isEmpty()) {
        return 0;
    }
    if (localNorm == officialNorm) {
        return 95;
    }
    if (localNorm.contains(officialNorm) || officialNorm.contains(localNorm)) {
        return 80;
    }
    if (trackTitleMatches(local, official)) {
        return 70;
    }
    return 0;
}

QVariantMap parseMusicBrainzDetail(const QByteArray &body, const QString &trackTitle) {
    QVariantMap extra;
    const QJsonObject release = QJsonDocument::fromJson(body).object();
    if (release.isEmpty()) {
        return extra;
    }

    QString genre = firstTagName(release.value(QStringLiteral("tags")).toArray());
    if (genre.isEmpty()) {
        genre = firstTagName(release.value(QStringLiteral("release-group"))
                                .toObject()
                                .value(QStringLiteral("tags"))
                                .toArray());
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
        const QString label = firstLabel.value(QStringLiteral("label"))
                                  .toObject()
                                  .value(QStringLiteral("name"))
                                  .toString()
                                  .trimmed();
        if (!label.isEmpty()) {
            extra.insert(QStringLiteral("label"), label);
        }
        const QString catalog =
            firstLabel.value(QStringLiteral("catalog-number")).toString().trimmed();
        if (!catalog.isEmpty()) {
            extra.insert(QStringLiteral("catalognum"), catalog);
        }
    }

    QVariantList tracklist;
    int absolutePosition = 0;
    const QJsonArray media = release.value(QStringLiteral("media")).toArray();
    for (const QJsonValue &mediumValue : media) {
        const QJsonArray tracks = mediumValue.toObject().value(QStringLiteral("tracks")).toArray();
        for (const QJsonValue &trackValue : tracks) {
            const QJsonObject track = trackValue.toObject();
            QString title = track.value(QStringLiteral("title")).toString().trimmed();
            if (title.isEmpty()) {
                title = track.value(QStringLiteral("recording"))
                            .toObject()
                            .value(QStringLiteral("title"))
                            .toString()
                            .trimmed();
            }
            if (title.isEmpty()) {
                continue;
            }
            ++absolutePosition;
            const int trackNumber =
                parseTrackNumber(track.value(QStringLiteral("number")).toString());
            QVariantMap row;
            row.insert(QStringLiteral("title"), title);
            row.insert(QStringLiteral("trackNumber"), trackNumber > 0 ? trackNumber : absolutePosition);
            row.insert(QStringLiteral("position"), absolutePosition);
            tracklist << row;

            if (!trackTitle.trimmed().isEmpty() && !extra.contains(QStringLiteral("title"))
                && trackTitleMatches(title, trackTitle)) {
                extra.insert(QStringLiteral("title"), title);
                if (trackNumber > 0) {
                    extra.insert(QStringLiteral("trackNumber"), trackNumber);
                }
            }
        }
    }
    if (!tracklist.isEmpty()) {
        extra.insert(QStringLiteral("tracklist"), tracklist);
    }

    return extra;
}

QVariantMap parseDiscogsDetail(const QByteArray &body, const QString &trackTitle) {
    QVariantMap extra;
    const QJsonObject release = QJsonDocument::fromJson(body).object();
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

    QVariantList tracklist;
    int absolutePosition = 0;
    const QJsonArray rawTracklist = release.value(QStringLiteral("tracklist")).toArray();
    for (const QJsonValue &trackValue : rawTracklist) {
        const QJsonObject track = trackValue.toObject();
        if (track.value(QStringLiteral("type_")).toString() != QStringLiteral("track")) {
            continue;
        }
        const QString title = track.value(QStringLiteral("title")).toString().trimmed();
        if (title.isEmpty()) {
            continue;
        }
        ++absolutePosition;
        const int trackNumber =
            parseTrackNumber(track.value(QStringLiteral("position")).toString());
        QVariantMap row;
        row.insert(QStringLiteral("title"), title);
        row.insert(QStringLiteral("trackNumber"), trackNumber > 0 ? trackNumber : absolutePosition);
        row.insert(QStringLiteral("position"), absolutePosition);
        tracklist << row;

        if (!trackTitle.trimmed().isEmpty() && !extra.contains(QStringLiteral("title"))
            && trackTitleMatches(title, trackTitle)) {
            extra.insert(QStringLiteral("title"), title);
            if (trackNumber > 0) {
                extra.insert(QStringLiteral("trackNumber"), trackNumber);
            }
        }
    }
    if (!tracklist.isEmpty()) {
        extra.insert(QStringLiteral("tracklist"), tracklist);
    }

    return extra;
}

} // namespace

MetadataSearchService::MetadataSearchService(ConfigService *config, DiscogsService *discogs,
                                             QObject *parent)
    : QObject(parent), m_config(config), m_discogs(discogs) {
    Q_UNUSED(m_config);
    m_network = new QNetworkAccessManager(this);
}

MetadataSearchService::~MetadataSearchService() {
    abortNetwork();
}

QVariantMap MetadataSearchService::normalizeCandidateFields(const QVariantMap &fields) {
    QVariantMap normalized = fields;
    if (normalized.contains(QStringLiteral("albumartist"))) {
        normalized.insert(QStringLiteral("albumArtist"),
                          normalized.take(QStringLiteral("albumartist")));
    }
    return normalized;
}

void MetadataSearchService::abortNetwork() {
    if (!m_reply) {
        return;
    }
    QObject::disconnect(m_reply, nullptr, this, nullptr);
    m_reply->abort();
    m_reply->deleteLater();
    m_reply = nullptr;
    m_pendingKind = PendingKind::None;
}

void MetadataSearchService::startGet(const QUrl &url,
                                     const QList<QPair<QByteArray, QByteArray>> &headers,
                                     PendingKind kind) {
    abortNetwork();
    if (!url.isValid()) {
        m_pendingKind = PendingKind::None;
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromUtf8(kMbUserAgent));
    request.setTransferTimeout(20000);
    for (const auto &header : headers) {
        request.setRawHeader(header.first, header.second);
    }
    m_pendingKind = kind;
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &MetadataSearchService::onReplyFinished);
}

void MetadataSearchService::clear() {
    ++m_detailFetchId;
    ++m_searchId;
    abortNetwork();
    m_searchesRemaining = 0;
    m_searchAccum.clear();
    m_candidates.clear();
    m_selectedIndex = -1;
    m_currentFields.clear();
    m_fieldChecked.clear();
    m_localTracksForTitleFix.clear();
    if (m_localTrackCount != 0) {
        m_localTrackCount = 0;
        emit localTrackCountChanged();
    }
    m_titleFixProposals.clear();
    m_titleFixUnmatched.clear();
    m_titleFixChecked.clear();
    setSearching(false);
    emit candidatesChanged();
    emit selectedIndexChanged();
    emit fieldChoicesChanged();
    emit titleFixProposalsChanged();
}

void MetadataSearchService::setCurrentFields(const QVariantMap &fields) {
    m_currentFields = fields;
    emit fieldChoicesChanged();
}

void MetadataSearchService::searchRelease(const QString &artist, const QString &album,
                                          const QString &albumArtist, const QVariantList &formats,
                                          const QString &edition) {
    const QString searchArtist =
        albumArtist.trimmed().isEmpty() ? artist.trimmed() : albumArtist.trimmed();
    const QString trimmedEdition = edition.trimmed();
    if (!isUsableName(searchArtist) && !isUsableName(album) && trimmedEdition.isEmpty()) {
        setStatus(QStringLiteral("Need an artist or album name to search"));
        emit searchFinished(false);
        return;
    }

    m_searchFormats.clear();
    for (const QVariant &value : formats) {
        const QString format = value.toString().trimmed();
        if (!format.isEmpty()) {
            m_searchFormats << format;
        }
    }
    m_searchEdition = trimmedEdition;

    m_currentFields.insert(QStringLiteral("artist"), artist);
    m_currentFields.insert(QStringLiteral("album"), album);
    m_currentFields.insert(QStringLiteral("albumArtist"), searchArtist);

    ++m_searchId;
    ++m_detailFetchId;
    abortNetwork();
    m_searchArtist = searchArtist;
    m_searchAlbum = album.trimmed();
    m_searchAccum.clear();
    m_searchesRemaining = 0;

    setSearching(true);
    setStatus(QStringLiteral("Searching MusicBrainz and Discogs…"));
    m_candidates.clear();
    m_selectedIndex = -1;
    m_fieldChecked.clear();
    emit candidatesChanged();
    emit selectedIndexChanged();

    const QUrl mbUrl =
        musicBrainzSearchUrl(m_searchArtist, m_searchAlbum, m_searchFormats, m_searchEdition);
    const QString token = m_discogs ? m_discogs->loadToken() : QString();
    const QUrl discogsUrl =
        token.isEmpty() ? QUrl()
                        : discogsSearchUrl(m_searchArtist, m_searchAlbum, m_searchFormats,
                                           m_searchEdition);

    // Sequential: MusicBrainz first, then Discogs — one in-flight reply at a time.
    if (mbUrl.isValid()) {
        m_searchesRemaining = discogsUrl.isValid() ? 2 : 1;
        startGet(mbUrl, {}, PendingKind::SearchMusicBrainz);
        return;
    }
    if (discogsUrl.isValid()) {
        m_searchesRemaining = 1;
        const QList<QPair<QByteArray, QByteArray>> headers = {
            {QByteArray("Authorization"), QByteArray("Discogs token=") + token.toUtf8()}};
        startGet(discogsUrl, headers, PendingKind::SearchDiscogs);
        return;
    }

    setSearching(false);
    setStatus(QStringLiteral("No matching releases found"));
    emit searchFinished(false);
}

void MetadataSearchService::finishSearchIfReady() {
    if (m_searchesRemaining > 0) {
        return;
    }

    QVariantList results = m_searchAccum;
    std::sort(results.begin(), results.end(), [&](const QVariant &left, const QVariant &right) {
        return albumMatchScore(left.toMap(), m_searchAlbum, m_searchArtist, m_searchEdition,
                                m_localTrackCount)
               > albumMatchScore(right.toMap(), m_searchAlbum, m_searchArtist, m_searchEdition,
                                 m_localTrackCount);
    });

    m_candidates = results;
    setSearching(false);
    if (results.isEmpty()) {
        setStatus(QStringLiteral("No matching releases found"));
        emit searchFinished(false);
    } else {
        if (m_localTrackCount > 0) {
            setStatus(QStringLiteral("Found %1 release(s) — ranked by closeness to your %2 tracks")
                          .arg(results.size())
                          .arg(m_localTrackCount));
        } else {
            setStatus(QStringLiteral("Found %1 release(s) — select one").arg(results.size()));
        }
        setSelectedIndex(0);
        emit searchFinished(true);
    }
    emit candidatesChanged();
}

void MetadataSearchService::onReplyFinished() {
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (!reply) {
        return;
    }
    reply->deleteLater();

    const PendingKind kind = m_pendingKind;
    m_pendingKind = PendingKind::None;
    const bool netOk = reply->error() == QNetworkReply::NoError;
    const QByteArray body = netOk ? reply->readAll() : QByteArray();

    switch (kind) {
    case PendingKind::SearchMusicBrainz: {
        if (netOk) {
            m_searchAccum.append(parseMusicBrainzSearch(body));
        }
        --m_searchesRemaining;
        const QString token = m_discogs ? m_discogs->loadToken() : QString();
        const QUrl discogsUrl =
            token.isEmpty() ? QUrl() : discogsSearchUrl(m_searchArtist, m_searchAlbum, m_searchFormats, m_searchEdition);
        if (discogsUrl.isValid() && m_searchesRemaining > 0) {
            const QList<QPair<QByteArray, QByteArray>> headers = {
                {QByteArray("Authorization"), QByteArray("Discogs token=") + token.toUtf8()}};
            startGet(discogsUrl, headers, PendingKind::SearchDiscogs);
        } else {
            m_searchesRemaining = 0;
            finishSearchIfReady();
        }
        break;
    }
    case PendingKind::SearchDiscogs: {
        if (netOk) {
            m_searchAccum.append(parseDiscogsSearch(body, m_searchArtist, m_searchFormats));
        }
        m_searchesRemaining = 0;
        finishSearchIfReady();
        break;
    }
    case PendingKind::DetailMusicBrainz: {
        const int fetchId = m_detailFetchId;
        const int index = m_detailIndex;
        const QString trackTitle = m_currentFields.value(QStringLiteral("title")).toString();
        applyReleaseDetailFields(index, fetchId,
                                 netOk ? parseMusicBrainzDetail(body, trackTitle) : QVariantMap{});
        break;
    }
    case PendingKind::DetailDiscogs: {
        const int fetchId = m_detailFetchId;
        const int index = m_detailIndex;
        const QString trackTitle = m_currentFields.value(QStringLiteral("title")).toString();
        applyReleaseDetailFields(index, fetchId,
                                 netOk ? parseDiscogsDetail(body, trackTitle) : QVariantMap{});
        break;
    }
    case PendingKind::None:
        break;
    }
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
    rebuildTitleFixProposals();
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

    const int fetchId = ++m_detailFetchId;
    m_detailIndex = index;
    Q_UNUSED(fetchId);
    setStatus(QStringLiteral("Loading release details…"));

    if (source == QStringLiteral("MusicBrainz")) {
        QUrl url(QStringLiteral("https://musicbrainz.org/ws/2/release/") + id);
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("fmt"), QStringLiteral("json"));
        query.addQueryItem(
            QStringLiteral("inc"),
            QStringLiteral("artist-credits+labels+recordings+release-groups+tags"));
        url.setQuery(query);
        startGet(url, {}, PendingKind::DetailMusicBrainz);
        return;
    }

    if (source == QStringLiteral("Discogs")) {
        const QString token = m_discogs ? m_discogs->loadToken() : QString();
        if (token.isEmpty()) {
            return;
        }
        QUrl url(QStringLiteral("https://api.discogs.com/releases/") + id);
        const QList<QPair<QByteArray, QByteArray>> headers = {
            {QByteArray("Authorization"), QByteArray("Discogs token=") + token.toUtf8()}};
        startGet(url, headers, PendingKind::DetailDiscogs);
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
    const QVariantList tracklist = extra.value(QStringLiteral("tracklist")).toList();
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
        if (it.key() == QStringLiteral("tracklist")) {
            continue;
        }
        if (proposedFieldEmpty(it.key(), it.value())) {
            continue;
        }
        fields.insert(it.key(), it.value());
    }
    row.insert(QStringLiteral("fields"), fields);
    if (!tracklist.isEmpty()) {
        row.insert(QStringLiteral("tracklist"), tracklist);
        row.insert(QStringLiteral("trackCount"), tracklist.size());
        fields.insert(QStringLiteral("trackCount"), tracklist.size());
        row.insert(QStringLiteral("fields"), fields);
        QString detail = row.value(QStringLiteral("detail")).toString();
        const QString trackSuffix = QStringLiteral("%1 tracks").arg(tracklist.size());
        if (!detail.contains(QStringLiteral("tracks"), Qt::CaseInsensitive)) {
            detail = detail.isEmpty() ? trackSuffix : detail + QStringLiteral(" · ") + trackSuffix;
            row.insert(QStringLiteral("detail"), detail);
        }
    }
    m_candidates[index] = row;
    emit candidatesChanged();
    rebuildFieldChoices();
    rebuildTitleFixProposals();

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


void MetadataSearchService::setLocalTrackCount(int count) {
    const int bounded = qMax(0, count);
    if (m_localTrackCount == bounded) {
        return;
    }
    m_localTrackCount = bounded;
    emit localTrackCountChanged();
}

void MetadataSearchService::setLocalTracksForTitleFix(const QVariantList &tracks) {
    m_localTracksForTitleFix = tracks;
    setLocalTrackCount(tracks.size());
    rebuildTitleFixProposals();
}

void MetadataSearchService::setTitleFixChecked(int index, bool checked) {
    if (index < 0 || index >= m_titleFixProposals.size()) {
        return;
    }
    if (m_titleFixChecked.value(index) == checked) {
        return;
    }
    m_titleFixChecked.insert(index, checked);
    QVariantMap row = m_titleFixProposals.at(index).toMap();
    row.insert(QStringLiteral("checked"), checked);
    m_titleFixProposals[index] = row;
    emit titleFixProposalsChanged();
}

QVariantList MetadataSearchService::checkedTitleFixProposals() const {
    QVariantList rows;
    for (int i = 0; i < m_titleFixProposals.size(); ++i) {
        if (!m_titleFixChecked.value(i, false)) {
            continue;
        }
        rows << m_titleFixProposals.at(i);
    }
    return rows;
}

void MetadataSearchService::rebuildTitleFixProposals() {
    m_titleFixProposals.clear();
    m_titleFixUnmatched.clear();
    m_titleFixChecked.clear();

    if (m_localTracksForTitleFix.isEmpty() || m_selectedIndex < 0
        || m_selectedIndex >= m_candidates.size()) {
        emit titleFixProposalsChanged();
        return;
    }

    const QVariantList official =
        m_candidates.at(m_selectedIndex).toMap().value(QStringLiteral("tracklist")).toList();
    if (official.isEmpty()) {
        for (const QVariant &localValue : m_localTracksForTitleFix) {
            m_titleFixUnmatched << localValue;
        }
        emit titleFixProposalsChanged();
        return;
    }

    QVector<bool> usedOfficial(official.size(), false);
    QVector<int> matchOfficialIndex(m_localTracksForTitleFix.size(), -1);

    auto officialNumberMatches = [&](const QVariantMap &officialTrack, int localNumber) {
        if (localNumber <= 0) {
            return false;
        }
        return officialTrack.value(QStringLiteral("trackNumber")).toInt() == localNumber
               || officialTrack.value(QStringLiteral("position")).toInt() == localNumber;
    };

    for (int localIndex = 0; localIndex < m_localTracksForTitleFix.size(); ++localIndex) {
        const QVariantMap local = m_localTracksForTitleFix.at(localIndex).toMap();
        const int localNumber = local.value(QStringLiteral("trackNumber")).toInt();
        if (localNumber <= 0) {
            continue;
        }
        QList<int> candidates;
        for (int officialIndex = 0; officialIndex < official.size(); ++officialIndex) {
            if (usedOfficial.at(officialIndex)) {
                continue;
            }
            if (officialNumberMatches(official.at(officialIndex).toMap(), localNumber)) {
                candidates << officialIndex;
            }
        }
        if (candidates.isEmpty()) {
            continue;
        }
        int chosen = candidates.first();
        if (candidates.size() > 1) {
            int bestScore = -1;
            const QString localTitle = local.value(QStringLiteral("title")).toString();
            for (int officialIndex : candidates) {
                const int score = titleMatchScore(
                    localTitle, official.at(officialIndex).toMap().value(QStringLiteral("title")).toString());
                if (score > bestScore) {
                    bestScore = score;
                    chosen = officialIndex;
                }
            }
        }
        usedOfficial[chosen] = true;
        matchOfficialIndex[localIndex] = chosen;
    }

    for (int localIndex = 0; localIndex < m_localTracksForTitleFix.size(); ++localIndex) {
        if (matchOfficialIndex.at(localIndex) >= 0) {
            continue;
        }
        const QVariantMap local = m_localTracksForTitleFix.at(localIndex).toMap();
        const QString localTitle = local.value(QStringLiteral("title")).toString();
        int bestScore = 0;
        int bestIndex = -1;
        for (int officialIndex = 0; officialIndex < official.size(); ++officialIndex) {
            if (usedOfficial.at(officialIndex)) {
                continue;
            }
            const int score = titleMatchScore(
                localTitle, official.at(officialIndex).toMap().value(QStringLiteral("title")).toString());
            if (score > bestScore) {
                bestScore = score;
                bestIndex = officialIndex;
            }
        }
        if (bestIndex >= 0 && bestScore >= 70) {
            usedOfficial[bestIndex] = true;
            matchOfficialIndex[localIndex] = bestIndex;
        }
    }

    for (int localIndex = 0; localIndex < m_localTracksForTitleFix.size(); ++localIndex) {
        const QVariantMap local = m_localTracksForTitleFix.at(localIndex).toMap();
        const int officialIndex = matchOfficialIndex.at(localIndex);
        if (officialIndex < 0) {
            m_titleFixUnmatched << local;
            continue;
        }
        const QVariantMap officialTrack = official.at(officialIndex).toMap();
        const QString currentTitle = local.value(QStringLiteral("title")).toString().trimmed();
        const QString proposedTitle = officialTrack.value(QStringLiteral("title")).toString().trimmed();
        if (proposedTitle.isEmpty()
            || currentTitle.compare(proposedTitle, Qt::CaseInsensitive) == 0) {
            continue;
        }
        QVariantMap proposal;
        proposal.insert(QStringLiteral("path"), local.value(QStringLiteral("path")));
        proposal.insert(QStringLiteral("trackNumber"), local.value(QStringLiteral("trackNumber")));
        proposal.insert(QStringLiteral("current"), currentTitle);
        proposal.insert(QStringLiteral("proposed"), proposedTitle);
        proposal.insert(QStringLiteral("matchScore"), titleMatchScore(currentTitle, proposedTitle));
        proposal.insert(QStringLiteral("checked"), true);
        const int proposalIndex = m_titleFixProposals.size();
        m_titleFixChecked.insert(proposalIndex, true);
        m_titleFixProposals << proposal;
    }

    emit titleFixProposalsChanged();
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
