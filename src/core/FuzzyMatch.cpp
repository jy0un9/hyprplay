#include "FuzzyMatch.h"

#include <QVariantMap>
#include <algorithm>

namespace {

int fuzzyScore(const QString &text, const QString &query) {
    const QString hay = text.toLower();
    const QString needle = query.trimmed().toLower();
    if (needle.isEmpty()) {
        return 0;
    }
    if (hay == needle) {
        return 1000;
    }
    if (hay.startsWith(needle)) {
        return 800 + (100 - qMin(100, hay.size()));
    }
    if (hay.contains(needle)) {
        return 500 + (100 - qMin(100, hay.indexOf(needle)));
    }

    int hayIndex = 0;
    int consecutive = 0;
    int bestConsecutive = 0;
    for (const QChar ch : needle) {
        bool matched = false;
        while (hayIndex < hay.size()) {
            if (hay.at(hayIndex) == ch) {
                ++consecutive;
                bestConsecutive = qMax(bestConsecutive, consecutive);
                ++hayIndex;
                matched = true;
                break;
            }
            consecutive = 0;
            ++hayIndex;
        }
        if (!matched) {
            return -1;
        }
    }
    return 200 + bestConsecutive * 10 - hay.size();
}

} // namespace

bool fuzzyMatch(const QString &text, const QString &query) {
    return fuzzyScore(text, query) >= 0;
}

QStringList filterFuzzy(const QStringList &items, const QString &query) {
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        return items;
    }

    struct ScoredItem {
        QString value;
        int score = -1;
    };

    QList<ScoredItem> scored;
    scored.reserve(items.size());
    for (const QString &item : items) {
        const int score = fuzzyScore(item, trimmed);
        if (score >= 0) {
            scored.append({item, score});
        }
    }

    std::sort(scored.begin(), scored.end(), [](const ScoredItem &left, const ScoredItem &right) {
        if (left.score == right.score) {
            return left.value.compare(right.value, Qt::CaseInsensitive) < 0;
        }
        return left.score > right.score;
    });

    QStringList filtered;
    filtered.reserve(scored.size());
    for (const ScoredItem &item : scored) {
        filtered << item.value;
    }
    return filtered;
}

QVariantList filterTracksFuzzy(const QVariantList &tracks, const QString &query) {
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        return tracks;
    }

    struct ScoredTrack {
        QVariantMap track;
        int score = -1;
    };

    QList<ScoredTrack> scored;
    scored.reserve(tracks.size());
    for (const QVariant &value : tracks) {
        const QVariantMap track = value.toMap();
        const QString title = track.value(QStringLiteral("title")).toString();
        const QString artist = track.value(QStringLiteral("artist")).toString();
        const QString album = track.value(QStringLiteral("album")).toString();
        const int score = qMax(qMax(fuzzyScore(title, trimmed), fuzzyScore(artist, trimmed)),
                               fuzzyScore(album, trimmed));
        if (score >= 0) {
            scored.append({track, score});
        }
    }

    std::sort(scored.begin(), scored.end(), [](const ScoredTrack &left, const ScoredTrack &right) {
        if (left.score == right.score) {
            const int leftTrack = left.track.value(QStringLiteral("trackNumber")).toInt();
            const int rightTrack = right.track.value(QStringLiteral("trackNumber")).toInt();
            if (leftTrack != rightTrack) {
                return leftTrack < rightTrack;
            }
            return left.track.value(QStringLiteral("title")).toString().compare(
                       right.track.value(QStringLiteral("title")).toString(), Qt::CaseInsensitive)
                   < 0;
        }
        return left.score > right.score;
    });

    QVariantList filtered;
    filtered.reserve(scored.size());
    for (const ScoredTrack &item : scored) {
        filtered << item.track;
    }
    return filtered;
}
