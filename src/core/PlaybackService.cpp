#include "PlaybackService.h"

#include <QDebug>
#include <QFileInfo>
#include <QRandomGenerator>

#include <clocale>
#include <cmath>

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
    mpv_set_option_string(m_mpv, "ao", "pipewire,pulse,alsa");
    mpv_set_option_string(m_mpv, "keep-open", "yes");
    mpv_set_option_string(m_mpv, "idle", "yes");
    mpv_set_option_string(m_mpv, "pause", "no");

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

    m_error.clear();
    setVolume(m_volume);
    return true;
}

void PlaybackService::shutdownMpv() {
    m_pollTimer.stop();
    if (m_mpv) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
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
        } else if (m_queueIndex + 1 < m_queue.size() || m_repeatMode == RepeatMode::Queue) {
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
    loadCurrentQueueTrack();
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

    processMpvEvents();
    syncFromMpv();
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

    if (m_shuffle && m_queue.size() > 1) {
        int nextIdx = m_queueIndex;
        while (nextIdx == m_queueIndex) {
            nextIdx = QRandomGenerator::global()->bounded(m_queue.size());
        }
        m_queueIndex = nextIdx;
    } else if (m_queueIndex + 1 < m_queue.size()) {
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
    volume = qBound(0, volume, 100);
    m_volume = volume;
    if (m_mpv) {
        runMpvCommand(m_mpv, {QStringLiteral("set"), QStringLiteral("volume"),
                              QString::number(volume)});
    }
    emit volumeChanged();
}

void PlaybackService::setMuted(bool muted) {
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
    m_shuffle = enabled;
    emit shuffleChanged();
}

void PlaybackService::handleMprisPlay() { play(); }
void PlaybackService::handleMprisPause() { pause(); }
void PlaybackService::handleMprisStop() { stop(); }
void PlaybackService::handleMprisNext() { next(); }
void PlaybackService::handleMprisPrevious() { previous(); }
void PlaybackService::handleMprisSeek(double offset) { seekRelative(offset); }
