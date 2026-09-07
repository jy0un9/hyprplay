#pragma once

#include <QString>
#include <QVector>

struct M3uEntry {
    QString sourcePath;
    QString display;
    int durationSecs = -1;
};

QVector<M3uEntry> parseM3u8(const QString &content);
QString serializeM3u8(const QVector<M3uEntry> &entries);
