#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QString>

namespace LyricsParsers {

// Lowercase, strip track-number prefixes, feat markers and parentheticals,
// keep unicode letters so CJK/Cyrillic titles survive.
QString normalizeQueryText(const QString &text);
// Timestamped LRC check shared with the fetch chain.
bool syncedUsable(const QString &synced);
// Best LRCLIB /search candidate id, or 0. Skips instrumentals, untimed
// lyrics and duration outliers; prefers exact title/artist and close duration.
qint64 pickLrclibMatch(const QJsonArray &items, const QString &artist,
                       const QString &title, const QString &album, double wantSecs);
// (pick* matchers below)

qint64 pickNeteaseMatch(const QJsonDocument &doc, const QString &artist, const QString &title,
                        double wantSecs);
QString neteaseSyncedFromDoc(const QJsonDocument &doc);
QString ovPlainFromDoc(const QJsonDocument &doc);
QString geniusPageFromSearch(const QJsonDocument &doc, const QString &artist,
                             const QString &title);
QString scrapeGeniusHtml(const QString &html);
QString buildSummary(int fetched, int fetchedLrclib, int fetchedNetease, int fetchedOv,
                     int fetchedGenius, int skipped, int knownMiss, int failed);

} // namespace LyricsParsers
