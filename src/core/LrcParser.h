#pragma once

#include <QString>
#include <QVector>

struct LrcLine {
    double time = -1.0;
    QString text;
};

bool parseLrcTimestamp(const QString &stamp, double *outSecs);
bool parseLrcOffsetMs(const QString &line, double *outOffsetSecs);
QString stripEnhancedLrcTags(const QString &text);
QVector<LrcLine> parseLrcContent(const QString &content);
