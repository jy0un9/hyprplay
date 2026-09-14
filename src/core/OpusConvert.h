#pragma once

#include <QString>

// Encode FLAC to opusPath via a .partial file, then rename into place.
// Never leaves a truncated final .opus. Prefers opusenc, falls back to ffmpeg.
bool convertFlacToOpus(const QString &flacPath, const QString &opusPath, int bitrateKbps,
                       QString *errorOut = nullptr);
