#pragma once

#include <QString>

inline bool isSupportedAudioFile(const QString &path) {
    const QString lower = path.toLower();
    return lower.endsWith(QLatin1String(".flac")) || lower.endsWith(QLatin1String(".opus"))
           || lower.endsWith(QLatin1String(".ogg")) || lower.endsWith(QLatin1String(".oga"))
           || lower.endsWith(QLatin1String(".mp3")) || lower.endsWith(QLatin1String(".m4a"))
           || lower.endsWith(QLatin1String(".aac"));
}
