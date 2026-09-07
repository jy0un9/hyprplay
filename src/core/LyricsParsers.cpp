#include "LyricsParsers.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

namespace {

constexpr double kNeteaseToleranceSecs = 8.0;

QString normalizeTitle(const QString &title) {
    QString normalized = title.toLower();
    // Drop parentheticals such as "(Remastered)" or "[Explicit]".
    normalized.remove(QRegularExpression(QStringLiteral("\\s*[\\(\\[].*?[\\)\\]]")));
    normalized.remove(QRegularExpression(QStringLiteral("[^a-z0-9 ]")));
    return normalized.simplified();
}

bool durationMatchesMs(qint64 haveMs, double wantSecs) {
    if (haveMs <= 0 || wantSecs < 0) {
        return true;
    }
    return qAbs(haveMs / 1000.0 - wantSecs) <= kNeteaseToleranceSecs;
}

} // namespace

namespace LyricsParsers {

qint64 pickNeteaseMatch(const QJsonDocument &doc, const QString &artist,
                                       const QString &title, double wantSecs) {
    const QJsonObject result = doc.object().value(QStringLiteral("result")).toObject();
    const QJsonArray songs = result.value(QStringLiteral("songs")).toArray();
    const QString wantArtist = artist.trimmed();
    const QString wantTitle = title.trimmed();
    for (const QJsonValue &value : songs) {
        const QJsonObject song = value.toObject();
        QStringList artists;
        for (const QJsonValue &artistValue :
             song.value(QStringLiteral("artists")).toArray()) {
            artists << artistValue.toObject().value(QStringLiteral("name")).toString();
        }
        const QString joined = artists.join(QStringLiteral(", "));
        if (!joined.contains(wantArtist, Qt::CaseInsensitive)
            && !wantArtist.contains(artists.value(0), Qt::CaseInsensitive)) {
            continue;
        }
        const QString name = song.value(QStringLiteral("name")).toString();
        if (!name.contains(wantTitle, Qt::CaseInsensitive)
            && !wantTitle.contains(name, Qt::CaseInsensitive)) {
            continue;
        }
        if (!durationMatchesMs(song.value(QStringLiteral("duration")).toVariant().toLongLong(),
                               wantSecs)) {
            continue;
        }
        const qint64 id = song.value(QStringLiteral("id")).toVariant().toLongLong();
        if (id > 0) {
            return id;
        }
    }
    return 0;
}

QString neteaseSyncedFromDoc(const QJsonDocument &doc) {
    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("code")).toInt(-1) != 200) {
        return {};
    }
    if (obj.value(QStringLiteral("nolyric")).toBool()
        || obj.value(QStringLiteral("uncollected")).toBool()) {
        return {};
    }
    return obj.value(QStringLiteral("lrc")).toObject().value(QStringLiteral("lyric")).toString();
}

QString ovPlainFromDoc(const QJsonDocument &doc) {
    return doc.object().value(QStringLiteral("lyrics")).toString();
}

QString geniusPageFromSearch(const QJsonDocument &doc, const QString &artist,
                                            const QString &title) {
    const QJsonArray hits =
        doc.object().value(QStringLiteral("response")).toObject().value(QStringLiteral("hits")).toArray();
    const QString wantArtist = artist.trimmed();
    const QString wantTitle = normalizeTitle(title);
    for (const QJsonValue &value : hits) {
        const QJsonObject result = value.toObject().value(QStringLiteral("result")).toObject();
        const QString hitArtist =
            result.value(QStringLiteral("primary_artist")).toObject().value(QStringLiteral("name")).toString();
        if (!hitArtist.contains(wantArtist, Qt::CaseInsensitive)
            && !wantArtist.contains(hitArtist, Qt::CaseInsensitive)) {
            continue;
        }
        const QString hitTitle = normalizeTitle(result.value(QStringLiteral("title")).toString());
        if (!hitTitle.contains(wantTitle, Qt::CaseInsensitive)
            && !wantTitle.contains(hitTitle, Qt::CaseInsensitive)) {
            continue;
        }
        const QString url = result.value(QStringLiteral("url")).toString().trimmed();
        if (!url.isEmpty()) {
            return url;
        }
    }
    return {};
}

QString scrapeGeniusHtml(const QString &html) {
    if (html.isEmpty()) {
        return {};
    }
    // Genius renders lyrics in <div data-lyrics-container="true"> blocks.
    // Non-greedy match per block; nested divs are not used inside containers.
    static const QRegularExpression containerRe(
        QStringLiteral("<div[^>]*data-lyrics-container=\"true\"[^>]*>(.*?)</div>"),
        QRegularExpression::DotMatchesEverythingOption
            | QRegularExpression::CaseInsensitiveOption);
    QStringList blocks;
    auto it = containerRe.globalMatch(html);
    while (it.hasNext()) {
        QString block = it.next().captured(1);
        block.replace(QRegularExpression(QStringLiteral("<br\\s*/?>"),
                                         QRegularExpression::CaseInsensitiveOption),
                      QStringLiteral("\n"));
        block.replace(QRegularExpression(QStringLiteral("<[^>]+>")), QString());
        block.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
        block.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
        block.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
        block.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
        block.replace(QStringLiteral("&#x27;"), QStringLiteral("'"));
        block.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
        block.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
        blocks << block.trimmed();
    }
    const QString text = blocks.join(QStringLiteral("\n")).trimmed();
    // Drop annotation-only or truncated captures.
    int lines = 0;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        if (!line.trimmed().isEmpty()) {
            ++lines;
        }
    }
    if (text.length() < 100 || lines < 4) {
        return {};
    }
    return text + QLatin1Char('\n');
}

QString buildSummary(int fetched, int fetchedLrclib, int fetchedNetease,
                                    int fetchedOv, int fetchedGenius, int skipped,
                                    int knownMiss, int failed) {
    QString summary;
    if (fetched == 0 && failed == 0) {
        if (skipped > 0 && knownMiss == 0) {
            return QStringLiteral("Lyrics already present");
        }
        if (knownMiss > 0 && skipped == 0) {
            return QStringLiteral("No lyrics available (checked before)");
        }
        if (skipped > 0) {
            return QStringLiteral("%1 already present, %2 with no lyrics available")
                .arg(skipped)
                .arg(knownMiss);
        }
        return QStringLiteral("No lyrics found");
    }
    summary = QStringLiteral("Lyrics: %1 fetched").arg(fetched);
    QStringList parts;
    if (fetchedLrclib > 0) {
        parts << QStringLiteral("LRCLIB %1").arg(fetchedLrclib);
    }
    if (fetchedNetease > 0) {
        parts << QStringLiteral("NetEase %1").arg(fetchedNetease);
    }
    if (fetchedOv > 0) {
        parts << QStringLiteral("lyrics.ovh %1").arg(fetchedOv);
    }
    if (fetchedGenius > 0) {
        parts << QStringLiteral("Genius %1").arg(fetchedGenius);
    }
    if (!parts.isEmpty()) {
        summary += QStringLiteral(" (%1)").arg(parts.join(QStringLiteral(", ")));
    }
    if (skipped > 0) {
        summary += QStringLiteral(", %1 already present").arg(skipped);
    }
    if (knownMiss > 0) {
        summary += QStringLiteral(", %1 with no lyrics available").arg(knownMiss);
    }
    if (failed > 0) {
        summary += QStringLiteral(", %1 failed").arg(failed);
    }
    return summary;
}

} // namespace LyricsParsers
