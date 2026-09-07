#include "LrcParser.h"

#include <QStringConverter>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace {

bool parseSecondsComponent(const QString &secPart, double *outSeconds) {
    if (!outSeconds || secPart.isEmpty()) {
        return false;
    }

    const int dot = secPart.indexOf(QLatin1Char('.'));
    const int colon = secPart.indexOf(QLatin1Char(':'));
    const int sep = dot >= 0 ? dot : colon;

    bool ok = false;
    double seconds = 0.0;
    if (sep >= 0) {
        seconds = secPart.left(sep).trimmed().toDouble(&ok);
        if (!ok) {
            return false;
        }
        const QString frac = secPart.mid(sep + 1).trimmed();
        if (frac.isEmpty()) {
            return false;
        }
        if (dot >= 0) {
            bool okFrac = false;
            const int centiseconds = frac.toInt(&okFrac);
            if (!okFrac) {
                return false;
            }
            if (frac.length() == 1) {
                seconds += centiseconds / 10.0;
            } else if (frac.length() == 2) {
                seconds += centiseconds / 100.0;
            } else {
                seconds += centiseconds / std::pow(10.0, frac.length());
            }
        } else {
            bool okFrac = false;
            const double fracVal = frac.toDouble(&okFrac);
            if (!okFrac) {
                return false;
            }
            const int digits = frac.length();
            seconds += fracVal / std::pow(10.0, digits);
        }
    } else {
        seconds = secPart.trimmed().toDouble(&ok);
        if (!ok) {
            return false;
        }
    }

    *outSeconds = seconds;
    return true;
}

QString stripBom(QString text) {
    if (text.startsWith(QChar(0xFEFF))) {
        text.remove(0, 1);
    }
    return text;
}

} // namespace

bool parseLrcTimestamp(const QString &stamp, double *outSecs) {
    if (!outSecs || stamp.isEmpty()) {
        return false;
    }

    const QStringList parts = stamp.split(QLatin1Char(':'));
    double hours = 0.0;
    double minutes = 0.0;
    QString secPart;

    if (parts.size() == 3) {
        bool okH = false;
        bool okM = false;
        hours = parts.at(0).trimmed().toDouble(&okH);
        minutes = parts.at(1).trimmed().toDouble(&okM);
        if (!okH || !okM) {
            return false;
        }
        secPart = parts.at(2).trimmed();
    } else if (parts.size() == 2) {
        bool okM = false;
        minutes = parts.at(0).trimmed().toDouble(&okM);
        if (!okM) {
            return false;
        }
        secPart = parts.at(1).trimmed();
    } else {
        return false;
    }

    double seconds = 0.0;
    if (!parseSecondsComponent(secPart, &seconds)) {
        return false;
    }

    *outSecs = hours * 3600.0 + minutes * 60.0 + seconds;
    return true;
}

bool parseLrcOffsetMs(const QString &line, double *outOffsetSecs) {
    if (!line.startsWith(QLatin1Char('[')) || !line.endsWith(QLatin1Char(']'))) {
        return false;
    }
    const QString inner = line.mid(1, line.size() - 2).trimmed();
    const int colon = inner.indexOf(QLatin1Char(':'));
    if (colon <= 0) {
        return false;
    }
    if (inner.left(colon).compare(QStringLiteral("offset"), Qt::CaseInsensitive) != 0) {
        return false;
    }
    bool ok = false;
    const double ms = inner.mid(colon + 1).trimmed().toDouble(&ok);
    if (!ok || !outOffsetSecs) {
        return false;
    }
    *outOffsetSecs = ms / 1000.0;
    return true;
}

QString stripEnhancedLrcTags(const QString &text) {
    QString out;
    out.reserve(text.size());
    for (int i = 0; i < text.size(); ++i) {
        if (text.at(i) != QLatin1Char('<')) {
            out.append(text.at(i));
            continue;
        }
        const int close = text.indexOf(QLatin1Char('>'), i + 1);
        if (close <= i) {
            out.append(text.at(i));
            continue;
        }
        const QString inner = text.mid(i + 1, close - i - 1);
        double ts = 0.0;
        if (parseLrcTimestamp(inner, &ts)) {
            i = close;
            continue;
        }
        out.append(text.at(i));
    }
    return out.trimmed();
}

QVector<LrcLine> parseLrcContent(const QString &content) {
    QVector<LrcLine> lines;
    double offsetSecs = 0.0;

    const QStringList rawLines = content.split(QLatin1Char('\n'));
    for (QString raw : rawLines) {
        raw = stripBom(raw.trimmed());
        if (raw.isEmpty()) {
            continue;
        }

        if (parseLrcOffsetMs(raw, &offsetSecs)) {
            continue;
        }

        QString remaining = raw;
        QVector<double> timestamps;
        while (remaining.startsWith(QLatin1Char('['))) {
            const int close = remaining.indexOf(QLatin1Char(']'));
            if (close <= 1) {
                break;
            }
            const QString stamp = remaining.mid(1, close - 1);
            double ts = 0.0;
            if (parseLrcTimestamp(stamp, &ts)) {
                timestamps.append(ts + offsetSecs);
            }
            remaining = remaining.mid(close + 1);
        }

        const QString text = stripEnhancedLrcTags(remaining.trimmed());
        if (!timestamps.isEmpty()) {
            for (double ts : timestamps) {
                if (text.isEmpty()) {
                    continue;
                }
                LrcLine line;
                line.time = qMax(0.0, ts);
                line.text = text;
                lines.append(line);
            }
            continue;
        }

        if (raw.startsWith(QLatin1Char('['))) {
            continue;
        }

        LrcLine plain;
        plain.time = -1.0;
        plain.text = raw;
        lines.append(plain);
    }

    std::stable_sort(lines.begin(), lines.end(),
                     [](const LrcLine &a, const LrcLine &b) { return a.time < b.time; });
    return lines;
}
