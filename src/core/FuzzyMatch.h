#pragma once

#include <QString>
#include <QStringList>

bool fuzzyMatch(const QString &text, const QString &query);
QStringList filterFuzzy(const QStringList &items, const QString &query);
QVariantList filterTracksFuzzy(const QVariantList &tracks, const QString &query);
