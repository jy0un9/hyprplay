#include "PlaylistM3u.h"

#include <QFileInfo>
#include <QStringList>

namespace {

QString unescapeExtInfDisplay(const QString &display) {
    return display.trimmed();
}

QString fallbackDisplayForPath(const QString &path) {
    return QFileInfo(path).completeBaseName();
}

M3uEntry parseExtInfLine(const QString &raw) {
    M3uEntry entry;
    const int comma = raw.indexOf(QLatin1Char(','));
    const QString durationPart = comma >= 0 ? raw.left(comma).trimmed() : raw.trimmed();
    entry.display = comma >= 0 ? unescapeExtInfDisplay(raw.mid(comma + 1)) : QString();
    bool ok = false;
    const int duration = durationPart.toInt(&ok);
    if (ok && duration >= 0) {
        entry.durationSecs = duration;
    }
    return entry;
}

} // namespace

QVector<M3uEntry> parseM3u8(const QString &content) {
    QVector<M3uEntry> entries;
    M3uEntry pending;
    bool hasPending = false;

    const QStringList lines = content.split(QLatin1Char('\n'));
    for (QString rawLine : lines) {
        rawLine = rawLine.trimmed();
        if (rawLine.startsWith(QChar(0xFEFF))) {
            rawLine = rawLine.mid(1);
        }
        if (rawLine.isEmpty()) {
            continue;
        }

        if (rawLine.startsWith(QStringLiteral("#EXTINF:"), Qt::CaseInsensitive)) {
            pending = parseExtInfLine(rawLine.mid(8));
            hasPending = true;
            continue;
        }

        if (rawLine.startsWith(QLatin1Char('#'))) {
            continue;
        }

        M3uEntry entry;
        entry.sourcePath = rawLine;
        if (hasPending) {
            entry.display = pending.display;
            entry.durationSecs = pending.durationSecs;
            hasPending = false;
        }
        if (entry.display.isEmpty()) {
            entry.display = fallbackDisplayForPath(entry.sourcePath);
        }
        entries.append(entry);
    }

    return entries;
}

QString serializeM3u8(const QVector<M3uEntry> &entries) {
    QString out;
    out += QStringLiteral("#EXTM3U\n");
    for (const M3uEntry &entry : entries) {
        const int durationSecs = qMax(0, entry.durationSecs);
        const QString display =
            entry.display.isEmpty() ? fallbackDisplayForPath(entry.sourcePath) : entry.display;
        out += QStringLiteral("#EXTINF:%1,%2\n").arg(durationSecs).arg(display);
        QString pathLine = entry.sourcePath;
        pathLine.replace(QLatin1Char('\\'), QLatin1Char('/'));
        out += pathLine;
        out += QLatin1Char('\n');
    }
    return out;
}
