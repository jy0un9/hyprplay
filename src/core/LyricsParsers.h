#pragma once

#include <QJsonDocument>
#include <QString>

namespace LyricsParsers {

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
