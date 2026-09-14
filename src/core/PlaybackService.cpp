#include "PlaybackService.h"

#include <QDebug>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QVector>

#include <algorithm>
#include <clocale>
#include <cmath>
#include <numeric>

namespace {

int runMpvCommand(mpv_handle *mpv, const QStringList &args) {
    if (!mpv) {
        return MPV_ERROR_UNSUPPORTED;
    }
    QVector<QByteArray> encoded;
    encoded.reserve(args.size());
    QVector<const char *> cargs;
    cargs.reserve(args.size() + 1);
    for (const QString &arg : args) {
        encoded.append(arg.toUtf8());
        cargs.append(encoded.last().constData());
    }
    cargs.append(nullptr);
    const int err = mpv_command(mpv, cargs.data());
    if (err < 0) {
        qWarning("mpv command %s failed: %s", qPrintable(args.join(QLatin1Char(' '))),
                 mpv_error_string(err));
    }
    return err;
}

} // namespace

PlaybackService::PlaybackService(QObject *parent) : QObject(parent) {
    connect(&m_pollTimer, &QTimer::timeout, this, &PlaybackService::pollMpv);
    m_pollTimer.start(16);
}

bool PlaybackService::ensureMpv() {
    if (m_mpv) {
        return true;
    }
    return initMpv();
}

PlaybackService::~PlaybackService() {
    shutdownMpv();
}

bool PlaybackService::initMpv() {
    // Qt resets locale after QGuiApplication starts; mpv requires LC_NUMERIC=C.
    setlocale(LC_NUMERIC, "C");

    m_mpv = mpv_create();
    if (!m_mpv) {
        m_error = QStringLiteral("Failed to create mpv context");
        return false;
    }

    mpv_set_option_string(m_mpv, "terminal", "no");
    mpv_set_option_string(m_mpv, "force-window", "no");
    mpv_set_option_string(m_mpv, "video", "no");
    mpv_set_option_string(m_mpv, "vo", "null");

    if (m_dacPassthrough) {
        // DAC passthrough: minimize DSP so a USB DAC receives the source
        // rate/format. ALSA first for direct hw access (falls back to
        // PipeWire when PipeWire holds the device). Samplerate/format "0"/"no"
        // means follow-the-source on mpv 0.41 (verified via --list-options).
        if (m_volume != 100) {
            m_volume = 100;
            emit volumeChanged();
        }
        mpv_set_option_string(m_mpv, "ao", "alsa,pipewire,pulse");
        mpv_set_option_string(m_mpv, "audio-exclusive", "yes");
        mpv_set_option_string(m_mpv, "audio-stream-silence", "no");
        mpv_set_option_string(m_mpv, "audio-wait-open", "2");
        mpv_set_option_string(m_mpv, "audio-buffer", "0.2");
        mpv_set_option_string(m_mpv, "gapless-audio", "yes");
        mpv_set_option_string(m_mpv, "audio-samplerate", "0");
        mpv_set_option_string(m_mpv, "audio-format", "no");
        mpv_set_option_string(m_mpv, "audio-channels", "auto");
        mpv_set_option_string(m_mpv, "replaygain", "no");
        mpv_set_option_string(m_mpv, "volume-max", "100");
    } else {
        // Native backends only: if these fail we want a loud error, not a
        // silent fallback to the ALSA shim (which mixers can't attribute to
        // this app, breaking per-app volume).
        mpv_set_option_string(m_mpv, "ao", "pipewire,pulse");
        mpv_set_option_string(m_mpv, "audio-stream-silence", "yes");
        mpv_set_option_string(m_mpv, "gapless-audio", "weak");
        mpv_set_option_string(m_mpv, "replaygain", "no");
    }
    mpv_set_option_string(m_mpv, "keep-open", "yes");
    mpv_set_option_string(m_mpv, "idle", "yes");
    mpv_set_option_string(m_mpv, "pause", "no");
    mpv_set_option_string(m_mpv, "audio-client-name", "hyprplay");

    const int initErr = mpv_initialize(m_mpv);
    if (initErr < 0) {
        m_error = QString::fromUtf8(mpv_error_string(initErr));
        qWarning("mpv_initialize failed: %s", qPrintable(m_error));
        mpv_destroy(m_mpv);
        m_mpv = nullptr;
        return false;
    }

    mpv_set_wakeup_callback(m_mpv, &PlaybackService::mpvWakeup, this);
    mpv_observe_property(m_mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "idle-active", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "eof-reached", MPV_FORMAT_FLAG);

    m_error.clear();
    if (!m_audioDevice.isEmpty()) {
        runMpvCommand(m_mpv, {QStringLiteral("set"), QStringLiteral("audio-device"), m_audioDevice});
    }
    setVolume(m_volume);
    refreshAudioBackend();
    return true;
}

void PlaybackService::shutdownMpv() {
    m_pollTimer.stop();
    if (m_mpv) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
    if (!m_audioBackend.isEmpty()) {
        m_audioBackend.clear();
        emit audioBackendChanged();
    }
}

void PlaybackService::mpvWakeup(void *ctx) {
    auto *self = static_cast<PlaybackService *>(ctx);
    QMetaObject::invokeMethod(self, &PlaybackService::pollMpv, Qt::QueuedConnection);
}

void PlaybackService::processMpvEvents() {
    if (!m_mpv) {
        return;
    }

    while (true) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        }
        handleMpvEvent(event);
    }
}

void PlaybackService::handleMpvEvent(mpv_event *event) {
    if (event->event_id == MPV_EVENT_END_FILE) {
        auto *ev = static_cast<mpv_event_end_file *>(event->data);
        if (ev->reason == MPV_END_FILE_REASON_EOF) {
            m_trackEndedPending = true;
        } else if (ev->reason == MPV_END_FILE_REASON_ERROR) {
            m_error = QString::fromUtf8(mpv_error_string(ev->error));
            qWarning("mpv playback error: %s", qPrintable(m_error));
            m_playing = false;
            emit playbackChanged();
        }
    } else if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
        auto *prop = static_cast<mpv_event_property *>(event->data);
        if (prop->format == MPV_FORMAT_DOUBLE) {
            const double value = *static_cast<double *>(prop->data);
            if (qstrcmp(prop->name, "time-pos") == 0) {
                m_position = value;
                emit positionChanged();
            } else if (qstrcmp(prop->name, "duration") == 0) {
                m_duration = value;
                emit playbackChanged();
            }
        } else if (prop->format == MPV_FORMAT_FLAG) {
            const int flag = *static_cast<int *>(prop->data);
            if (qstrcmp(prop->name, "pause") == 0) {
                m_paused = flag != 0;
                m_playing = !m_paused && !m_currentPath.isEmpty();
                emit playbackChanged();
            } else if (qstrcmp(prop->name, "eof-reached") == 0 && flag != 0) {
                m_trackEndedPending = true;
            }
        }
    } else if (event->event_id == MPV_EVENT_COMMAND_REPLY) {
        if (event->error < 0) {
            m_error = QString::fromUtf8(mpv_error_string(event->error));
            qWarning("mpv command reply error: %s", qPrintable(m_error));
            emit playbackChanged();
        }
    }
}

void PlaybackService::syncFromMpv() {
    if (!m_mpv) {
        return;
    }

    double position = m_position;
    double duration = m_duration;
    int paused = m_paused ? 1 : 0;
    bool changed = false;

    if (mpv_get_property(m_mpv, "time-pos", MPV_FORMAT_DOUBLE, &position) >= 0) {
        if (std::isfinite(position) && std::abs(position - m_position) > 0.001) {
            m_position = position;
            changed = true;
        }
    }
    if (mpv_get_property(m_mpv, "duration", MPV_FORMAT_DOUBLE, &duration) >= 0) {
        if (std::isfinite(duration) && std::abs(duration - m_duration) > 0.001) {
            m_duration = duration;
            changed = true;
        }
    }
    if (mpv_get_property(m_mpv, "pause", MPV_FORMAT_FLAG, &paused) >= 0) {
        const bool newPaused = paused != 0;
        const bool newPlaying = !newPaused && !m_currentPath.isEmpty();
        if (newPaused != m_paused || newPlaying != m_playing) {
            m_paused = newPaused;
            m_playing = newPlaying;
            changed = true;
        }
    }

    if (changed) {
        emit positionChanged();
        emit playbackChanged();
    }
}

void PlaybackService::pollMpv() {
    if (!m_mpv) {
        return;
    }

    processMpvEvents();

    if (m_playing && !m_paused && !m_currentPath.isEmpty()) {
        double position = m_position;
        if (mpv_get_property(m_mpv, "time-pos", MPV_FORMAT_DOUBLE, &position) >= 0
            && std::isfinite(position) && std::abs(position - m_position) > 0.001) {
            m_position = position;
            emit positionChanged();
        }
    }

    if (m_trackEndedPending) {
        m_trackEndedPending = false;
        emit trackFinished();
        if (m_repeatMode == RepeatMode::Track) {
            seek(0);
            play();
        } else if (hasNextInQueue()) {
            next();
        } else {
            m_playing = false;
            emit playbackChanged();
        }
    }
}

void PlaybackService::playPath(const QString &path, const QString &title, const QString &artist,
                               const QString &album) {
    m_queue.clear();
    m_shuffleBag.clear();
    m_shufflePos = -1;
    m_queueIndex = 0;

    QVariantMap track;
    track.insert(QStringLiteral("path"), path);
    track.insert(QStringLiteral("title"), title);
    track.insert(QStringLiteral("artist"), artist);
    track.insert(QStringLiteral("album"), album);
    m_queue << track;

    loadCurrentQueueTrack();
}

void PlaybackService::playTracks(const QVariantList &tracks, int startIndex) {
    if (tracks.isEmpty()) {
        return;
    }
    m_queue = tracks;
    m_queueIndex = qBound(0, startIndex, tracks.size() - 1);
    if (m_shuffle && m_queue.size() > 1) {
        rebuildShuffleBag(m_queueIndex);
        m_queueIndex = m_shuffleBag.at(m_shufflePos);
    } else {
        m_shuffleBag.clear();
        m_shufflePos = -1;
    }
    loadCurrentQueueTrack();
}

void PlaybackService::rebuildShuffleBag(int preferFirstIndex) {
    const int n = m_queue.size();
    m_shuffleBag.resize(n);
    std::iota(m_shuffleBag.begin(), m_shuffleBag.end(), 0);
    for (int i = n - 1; i > 0; --i) {
        const int j = QRandomGenerator::global()->bounded(i + 1);
        std::swap(m_shuffleBag[i], m_shuffleBag[j]);
    }
    if (preferFirstIndex >= 0 && preferFirstIndex < n) {
        const int at = m_shuffleBag.indexOf(preferFirstIndex);
        if (at > 0) {
            std::swap(m_shuffleBag[0], m_shuffleBag[at]);
        }
    }
    m_shufflePos = n > 0 ? 0 : -1;
}

bool PlaybackService::hasNextInQueue() const {
    if (m_queue.isEmpty()) {
        return false;
    }
    if (m_repeatMode == RepeatMode::Queue) {
        return true;
    }
    if (m_shuffle && m_shuffleBag.size() > 1) {
        return m_shufflePos + 1 < m_shuffleBag.size();
    }
    return m_queueIndex + 1 < m_queue.size();
}

void PlaybackService::loadCurrentQueueTrack() {
    if (!ensureMpv()) {
        m_error = QStringLiteral("Audio engine unavailable");
        emit playbackChanged();
        return;
    }

    if (m_queueIndex < 0 || m_queueIndex >= m_queue.size()) {
        return;
    }

    const QVariantMap track = m_queue.at(m_queueIndex).toMap();
    const QString path = QFileInfo(track.value(QStringLiteral("path")).toString()).absoluteFilePath();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        m_error = QStringLiteral("Track file not found");
        qWarning("Track file not found: %s", qPrintable(path));
        emit playbackChanged();
        return;
    }

    m_currentPath = path;
    m_title = track.value(QStringLiteral("title")).toString();
    m_artist = track.value(QStringLiteral("artist")).toString();
    m_album = track.value(QStringLiteral("album")).toString();
    m_error.clear();

    emit trackChanged();

    m_position = 0.0;
    m_duration = 0.0;
    emit positionChanged();

    runMpvCommand(m_mpv, {QStringLiteral("loadfile"), path, QStringLiteral("replace")});
    mpv_set_property_string(m_mpv, "pause", "no");
    mpv_set_property_string(m_mpv, "vid", "no");

    m_paused = false;
    m_playing = true;

    processMpvEvents();
    syncFromMpv();
    refreshAudioBackend();
    emit playbackChanged();
}

void PlaybackService::togglePlayPause() {
    if (m_currentPath.isEmpty()) {
        return;
    }
    runMpvCommand(m_mpv, {QStringLiteral("cycle"), QStringLiteral("pause")});
    processMpvEvents();
    syncFromMpv();
}

void PlaybackService::play() {
    if (m_currentPath.isEmpty()) {
        return;
    }
    runMpvCommand(m_mpv, {QStringLiteral("set"), QStringLiteral("pause"), QStringLiteral("no")});
    processMpvEvents();
    syncFromMpv();
}

void PlaybackService::pause() {
    if (m_currentPath.isEmpty()) {
        return;
    }
    if (!ensureMpv()) {
        m_error = QStringLiteral("Audio engine unavailable");
        emit playbackChanged();
        return;
    }
    runMpvCommand(m_mpv, {QStringLiteral("set"), QStringLiteral("pause"), QStringLiteral("yes")});
    processMpvEvents();
    syncFromMpv();
}

void PlaybackService::stop() {
    runMpvCommand(m_mpv, {QStringLiteral("stop")});
    m_playing = false;
    m_paused = false;
    m_position = 0.0;
    processMpvEvents();
    emit playbackChanged();
    emit positionChanged();
}

void PlaybackService::next() {
    if (m_queue.isEmpty()) {
        return;
    }

    if (m_shuffle && m_shuffleBag.size() > 1) {
        if (m_shufflePos + 1 < m_shuffleBag.size()) {
            ++m_shufflePos;
        } else if (m_repeatMode == RepeatMode::Queue) {
            const int justPlayed = m_queueIndex;
            rebuildShuffleBag(-1);
            if (m_shuffleBag.size() > 1 && m_shuffleBag.first() == justPlayed) {
                const int swapWith = 1 + QRandomGenerator::global()->bounded(m_shuffleBag.size() - 1);
                std::swap(m_shuffleBag[0], m_shuffleBag[swapWith]);
            }
        } else {
            return;
        }
        m_queueIndex = m_shuffleBag.at(m_shufflePos);
        loadCurrentQueueTrack();
        return;
    }

    if (m_queueIndex + 1 < m_queue.size()) {
        ++m_queueIndex;
    } else if (m_repeatMode == RepeatMode::Queue) {
        m_queueIndex = 0;
    } else {
        return;
    }
    loadCurrentQueueTrack();
}

void PlaybackService::previous() {
    if (m_queue.isEmpty()) {
        return;
    }
    if (m_position > 3.0) {
        seek(0);
        return;
    }

    if (m_shuffle && m_shuffleBag.size() > 1) {
        if (m_shufflePos > 0) {
            --m_shufflePos;
            m_queueIndex = m_shuffleBag.at(m_shufflePos);
            loadCurrentQueueTrack();
        } else if (m_repeatMode == RepeatMode::Queue) {
            m_shufflePos = m_shuffleBag.size() - 1;
            m_queueIndex = m_shuffleBag.at(m_shufflePos);
            loadCurrentQueueTrack();
        }
        return;
    }

    if (m_queueIndex > 0) {
        --m_queueIndex;
        loadCurrentQueueTrack();
    } else if (m_repeatMode == RepeatMode::Queue) {
        m_queueIndex = m_queue.size() - 1;
        loadCurrentQueueTrack();
    }
}

void PlaybackService::seek(double seconds) {
    if (!ensureMpv()) {
        return;
    }

    if (m_duration > 0.0) {
        seconds = qBound(0.0, seconds, m_duration);
    } else {
        seconds = qMax(0.0, seconds);
    }

    m_position = seconds;
    emit positionChanged();

    runMpvCommand(m_mpv, {QStringLiteral("seek"), QString::number(seconds, 'f', 3),
                          QStringLiteral("absolute")});
    processMpvEvents();

    double actual = seconds;
    if (mpv_get_property(m_mpv, "time-pos", MPV_FORMAT_DOUBLE, &actual) >= 0 && std::isfinite(actual)
        && std::abs(actual - m_position) > 0.05) {
        m_position = actual;
        emit positionChanged();
    }
}

void PlaybackService::seekRelative(double delta) {
    seek(m_position + delta);
}

void PlaybackService::setVolume(int volume) {
    if (m_dacPassthrough) {
        // Bit-perfect: no digital attenuation, DAC knob controls level.
        volume = 100;
    }
    volume = qBound(0, volume, 100);
    m_volume = volume;
    if (m_mpv) {
        runMpvCommand(m_mpv, {QStringLiteral("set"), QStringLiteral("volume"),
                              QString::number(volume)});
    }
    emit volumeChanged();
}

void PlaybackService::setMuted(bool muted) {
    if (m_dacPassthrough) {
        // Bit-perfect: mpv mute is DSP on this path; pause or use the DAC knob.
        return;
    }
    m_muted = muted;
    if (m_mpv) {
        runMpvCommand(m_mpv, {QStringLiteral("set"), QStringLiteral("mute"),
                              muted ? QStringLiteral("yes") : QStringLiteral("no")});
    }
    emit volumeChanged();
}

void PlaybackService::setRepeatMode(int mode) {
    m_repeatMode = static_cast<RepeatMode>(qBound(0, mode, 2));
    emit repeatModeChanged();
}

void PlaybackService::setShuffle(bool enabled) {
    if (m_shuffle == enabled) {
        return;
    }
    m_shuffle = enabled;
    if (m_shuffle && m_queue.size() > 1 && m_queueIndex >= 0) {
        rebuildShuffleBag(m_queueIndex);
        m_queueIndex = m_shuffleBag.at(m_shufflePos);
    } else {
        m_shuffleBag.clear();
        m_shufflePos = -1;
    }
    emit shuffleChanged();
}

void PlaybackService::refreshAudioBackend() {
    QString backend;
    if (m_mpv) {
        char *ao = mpv_get_property_string(m_mpv, "current-ao");
        char *device = mpv_get_property_string(m_mpv, "audio-device");
        const QString aoStr = ao ? QString::fromUtf8(ao) : QString();
        const QString deviceStr = device ? QString::fromUtf8(device) : QString();
        mpv_free(ao);
        mpv_free(device);
        if (!aoStr.isEmpty() && aoStr != QStringLiteral("null")) {
            backend = aoStr;
            if (!deviceStr.isEmpty() && deviceStr != QStringLiteral("auto")) {
                backend += QStringLiteral(" · ") + deviceStr;
            }
        }
    }
    if (m_audioBackend == backend) {
        return;
    }
    m_audioBackend = backend;
    emit audioBackendChanged();
}

void PlaybackService::setDacPassthrough(bool enabled) {
    if (m_dacPassthrough == enabled) {
        return;
    }
    m_dacPassthrough = enabled;
    if (enabled && m_muted) {
        // Mute never engages on the passthrough path; drop stale flag so the icon stays honest.
        m_muted = false;
        emit volumeChanged();
    }
    emit dacPassthroughChanged();

    if (!m_mpv) {
        // Engine not started yet; initMpv() will apply the mode.
        return;
    }

    // ao/exclusive require an engine restart. Preserve queue + position.
    const bool hadTrack = !m_currentPath.isEmpty() && m_queueIndex >= 0
        && m_queueIndex < m_queue.size();
    const double resumePos = m_position;
    const bool wasPlaying = m_playing && !m_paused;

    shutdownMpv();
    m_pollTimer.start(16);
    if (!initMpv()) {
        m_error = QStringLiteral("Audio engine restart failed after DAC toggle");
        emit playbackChanged();
        return;
    }
    setVolume(m_volume);

    if (hadTrack) {
        loadCurrentQueueTrack();
        if (resumePos > 1.0) {
            seek(resumePos);
        }
        if (!wasPlaying) {
            pause();
        }
    }
}

void PlaybackService::setAudioDevice(const QString &name) {
    if (m_audioDevice == name) {
        return;
    }
    m_audioDevice = name;
    if (m_mpv) {
        const QString oldBackend = m_audioBackend;
        runMpvCommand(m_mpv, {QStringLiteral("set"), QStringLiteral("audio-device"),
                              name.isEmpty() ? QStringLiteral("auto") : name});
        refreshAudioBackend();
        if (m_audioBackend == oldBackend) {
            // Backend string can stay identical (e.g. pre-init); still notify audioDevice bindings.
            emit audioBackendChanged();
        }
    } else {
        emit audioBackendChanged();
    }
}

QVariantList PlaybackService::audioDeviceList() const {
    QVariantList out;
    if (!m_mpv) {
        return out;
    }
    mpv_node node;
    if (mpv_get_property(m_mpv, "audio-device-list", MPV_FORMAT_NODE, &node) < 0) {
        return out;
    }
    if (node.format == MPV_FORMAT_NODE_ARRAY && node.u.list) {
        for (int i = 0; i < node.u.list->num; ++i) {
            const mpv_node *entry = &node.u.list->values[i];
            if (entry->format != MPV_FORMAT_NODE_MAP || !entry->u.list) {
                continue;
            }
            QString name;
            QString description;
            for (int j = 0; j < entry->u.list->num; ++j) {
                const char *key = entry->u.list->keys[j];
                const mpv_node *value = &entry->u.list->values[j];
                if (!key || value->format != MPV_FORMAT_STRING || !value->u.string) {
                    continue;
                }
                if (qstrcmp(key, "name") == 0) {
                    name = QString::fromUtf8(value->u.string);
                } else if (qstrcmp(key, "description") == 0) {
                    description = QString::fromUtf8(value->u.string);
                }
            }
            if (name.isEmpty()) {
                continue;
            }
            QVariantMap item;
            item.insert(QStringLiteral("name"), name);
            item.insert(QStringLiteral("description"),
                        description.isEmpty() ? name : description);
            out.append(item);
        }
    }
    mpv_free_node_contents(&node);
    return out;
}

void PlaybackService::handleMprisPlay() { play(); }
void PlaybackService::handleMprisPause() { pause(); }
void PlaybackService::handleMprisStop() { stop(); }
void PlaybackService::handleMprisNext() { next(); }
void PlaybackService::handleMprisPrevious() { previous(); }
void PlaybackService::handleMprisSeek(double offset) { seekRelative(offset); }
