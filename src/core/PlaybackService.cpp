#include "PlaybackService.h"

#include "AudioDeviceFilter.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QThread>
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
    // Close ALSA before restoring PipeWire, or the restore can fail while hw is busy.
    shutdownMpv();
    restoreExclusiveLease();
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
        // Exclusive ALSA only — no PipeWire/Pulse fallback (that path is not bit-perfect).
        ensureDacDeviceSelected();
        if (m_volume != 100) {
            m_volume = 100;
            emit volumeChanged();
        }
        mpv_set_option_string(m_mpv, "ao", "alsa");
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

    // Tests / CI: force a silent output so playback works without PipeWire.
    const QByteArray testAo = qgetenv("HYPRPLAY_TEST_AO");
    if (!testAo.isEmpty()) {
        mpv_set_option_string(m_mpv, "ao", testAo.constData());
        mpv_set_option_string(m_mpv, "audio-exclusive", "no");
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
    // Never feed exclusive ALSA device names into the PipeWire/Pulse backend.
    if (!m_dacPassthrough && isAlsaHwDevice(m_audioDevice)) {
        m_audioDevice = pipewireSinkForAlsaHwDevice(m_audioDevice);
    }
    if (!m_audioDevice.isEmpty()) {
        runMpvCommand(m_mpv, {QStringLiteral("set"), QStringLiteral("audio-device"), m_audioDevice});
    }
    setVolume(m_volume);
    refreshAudioBackend();
    refreshBitPerfectStatus();
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
                if (m_resumeHoldPlaying && flag != 0) {
                    // Async pause from engine reopen — keep playing.
                    mpv_set_property_string(m_mpv, "pause", "no");
                    m_paused = false;
                    m_playing = !m_currentPath.isEmpty();
                    emit playbackChanged();
                } else {
                    m_paused = flag != 0;
                    m_playing = !m_paused && !m_currentPath.isEmpty();
                    emit playbackChanged();
                }
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
        if (m_resumeHoldPlaying && paused != 0) {
            mpv_set_property_string(m_mpv, "pause", "no");
            paused = 0;
        }
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

    if (m_dacPassthrough) {
        refreshBitPerfectStatus();
        refreshAudioBackend();
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
    loadCurrentQueueTrackResuming(-1.0, true);
}

void PlaybackService::loadCurrentQueueTrackResuming(double resumePos, bool wasPlaying) {
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

    const bool seekOnLoad = resumePos > 0.05;
    m_position = seekOnLoad ? resumePos : 0.0;
    m_duration = 0.0;
    emit positionChanged();

    // start= must be on loadfile — seeking right after replace races the demuxer
    // and restarts from 0 when toggling bit-perfect.
    // mpv ≥0.38: loadfile <url> <flags> <index> <options>
    QStringList loadArgs{QStringLiteral("loadfile"), path, QStringLiteral("replace")};
    QStringList opts;
    if (seekOnLoad) {
        opts.append(QStringLiteral("start=%1").arg(resumePos, 0, 'f', 3));
    }
    // Be explicit: start= seeks can leave the core paused until we unpause.
    opts.append(wasPlaying ? QStringLiteral("pause=no") : QStringLiteral("pause=yes"));
    loadArgs.append(QStringLiteral("0")); // ignored insertion index for replace
    loadArgs.append(opts.join(QLatin1Char(',')));
    if (runMpvCommand(m_mpv, loadArgs) < 0) {
        // Older mpv without the index argument — retry classic form, then bare load.
        QStringList legacy{QStringLiteral("loadfile"), path, QStringLiteral("replace"),
                           opts.join(QLatin1Char(','))};
        if (runMpvCommand(m_mpv, legacy) < 0) {
            runMpvCommand(m_mpv, {QStringLiteral("loadfile"), path, QStringLiteral("replace")});
        }
    }
    mpv_set_property_string(m_mpv, "vid", "no");

    // Wait until demuxer + AO are up so unpause sticks (exclusive ALSA open is slow).
    for (int i = 0; i < 80; ++i) {
        processMpvEvents();
        char *ao = mpv_get_property_string(m_mpv, "current-ao");
        const bool hasAo = ao && ao[0] && qstrcmp(ao, "null") != 0;
        mpv_free(ao);
        double pos = 0.0;
        const bool hasPos = mpv_get_property(m_mpv, "time-pos", MPV_FORMAT_DOUBLE, &pos) >= 0
            && std::isfinite(pos);
        if (hasAo && (hasPos || !seekOnLoad)) {
            break;
        }
        QThread::msleep(25);
    }
    if (seekOnLoad) {
        double pos = 0.0;
        const bool havePos = mpv_get_property(m_mpv, "time-pos", MPV_FORMAT_DOUBLE, &pos) >= 0
            && std::isfinite(pos);
        if (!havePos || std::abs(pos - resumePos) > 1.0) {
            runMpvCommand(m_mpv, {QStringLiteral("seek"), QString::number(resumePos, 'f', 3),
                                  QStringLiteral("absolute")});
            processMpvEvents();
        }
        m_position = resumePos;
        emit positionChanged();
    }

    // Force play/pause after AO open — syncFromMpv alone can see a stale paused=yes
    // from the exclusive device handoff.
    processMpvEvents();
    if (wasPlaying) {
        runMpvCommand(m_mpv, {QStringLiteral("set"), QStringLiteral("pause"), QStringLiteral("no")});
        processMpvEvents();
        m_paused = false;
        m_playing = true;
    } else {
        runMpvCommand(m_mpv, {QStringLiteral("set"), QStringLiteral("pause"), QStringLiteral("yes")});
        processMpvEvents();
        m_paused = true;
        m_playing = false;
    }

    // Sync duration/position only; keep intentional pause state.
    if (m_mpv) {
        double duration = m_duration;
        if (mpv_get_property(m_mpv, "duration", MPV_FORMAT_DOUBLE, &duration) >= 0
            && std::isfinite(duration)) {
            m_duration = duration;
        }
        if (!seekOnLoad) {
            double pos = m_position;
            if (mpv_get_property(m_mpv, "time-pos", MPV_FORMAT_DOUBLE, &pos) >= 0
                && std::isfinite(pos)) {
                m_position = pos;
            }
        }
    }
    refreshAudioBackend();
    emit positionChanged();
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

namespace {

int mpvNodeMapInt(const mpv_node &node, const char *key) {
    if (node.format != MPV_FORMAT_NODE_MAP || !node.u.list) {
        return 0;
    }
    for (int i = 0; i < node.u.list->num; ++i) {
        if (!node.u.list->keys[i] || qstrcmp(node.u.list->keys[i], key) != 0) {
            continue;
        }
        const mpv_node &value = node.u.list->values[i];
        if (value.format == MPV_FORMAT_INT64) {
            return static_cast<int>(value.u.int64);
        }
        if (value.format == MPV_FORMAT_DOUBLE) {
            return static_cast<int>(value.u.double_);
        }
    }
    return 0;
}

QString mpvNodeMapString(const mpv_node &node, const char *key) {
    if (node.format != MPV_FORMAT_NODE_MAP || !node.u.list) {
        return {};
    }
    for (int i = 0; i < node.u.list->num; ++i) {
        if (!node.u.list->keys[i] || qstrcmp(node.u.list->keys[i], key) != 0) {
            continue;
        }
        const mpv_node &value = node.u.list->values[i];
        if (value.format == MPV_FORMAT_STRING && value.u.string) {
            return QString::fromUtf8(value.u.string);
        }
    }
    return {};
}

QString formatRateHz(int hz) {
    if (hz <= 0) {
        return QStringLiteral("—");
    }
    if (hz % 1000 == 0) {
        const double khz = hz / 1000.0;
        if (std::floor(khz) == khz) {
            return QString::number(static_cast<int>(khz)) + QStringLiteral(" kHz");
        }
        return QString::number(khz, 'f', 1) + QStringLiteral(" kHz");
    }
    return QString::number(hz) + QStringLiteral(" Hz");
}

} // namespace

void PlaybackService::ensureDacDeviceSelected() {
    if (!m_dacPassthrough) {
        return;
    }
    if (isAlsaHwDevice(m_audioDevice)) {
        return;
    }
    const QString usb = preferredUsbDacDevice();
    if (!usb.isEmpty()) {
        m_audioDevice = usb;
        return;
    }
    const QList<AudioDeviceEntry> cards = alsaHwDeviceEntries();
    if (!cards.isEmpty()) {
        m_audioDevice = cards.first().name;
    }
}

bool PlaybackService::acquireExclusiveForCurrentDevice() {
    ensureDacDeviceSelected();
    const QString cardId = alsaCardIdFromDevice(m_audioDevice);
    if (cardId.isEmpty()) {
        return false;
    }
    return acquireAlsaExclusiveLease(cardId, &m_alsaLease);
}

void PlaybackService::restoreExclusiveLease() {
    restoreAlsaExclusiveLease(&m_alsaLease);
}

void PlaybackService::holdResumePlaying(bool wasPlaying) {
    m_resumeHoldPlaying = wasPlaying;
    if (!wasPlaying) {
        return;
    }
    enforceResumePlaying();
    QTimer::singleShot(150, this, &PlaybackService::enforceResumePlaying);
    QTimer::singleShot(400, this, &PlaybackService::enforceResumePlaying);
    QTimer::singleShot(1000, this, [this]() {
        enforceResumePlaying();
        m_resumeHoldPlaying = false;
    });
}

void PlaybackService::enforceResumePlaying() {
    if (!m_resumeHoldPlaying || !m_mpv || m_currentPath.isEmpty()) {
        return;
    }
    runMpvCommand(m_mpv, {QStringLiteral("set"), QStringLiteral("pause"), QStringLiteral("no")});
    m_paused = false;
    m_playing = true;
    emit playbackChanged();
}

void PlaybackService::refreshBitPerfectStatus() {
    bool active = false;
    QString status;

    if (!m_dacPassthrough) {
        status.clear();
    } else if (!m_mpv) {
        status = QStringLiteral("Waiting for audio engine…");
    } else {
        char *ao = mpv_get_property_string(m_mpv, "current-ao");
        const QString aoStr = ao ? QString::fromUtf8(ao) : QString();
        mpv_free(ao);

        const bool alsaHw = aoStr == QLatin1String("alsa") && isAlsaHwDevice(m_audioDevice);
        if (aoStr.isEmpty() || aoStr == QLatin1String("null")) {
            status = m_alsaLease.held
                         ? QStringLiteral("DAC reserved from the OS — press play to open exclusive ALSA.")
                         : QStringLiteral("Could not reserve the DAC from PipeWire. Try Retry exclusive.");
        } else if (!alsaHw) {
            status = QStringLiteral("Not exclusive — backend is %1. Use Retry exclusive.")
                         .arg(aoStr.isEmpty() ? QStringLiteral("idle") : aoStr);
        } else {
            int sourceRate = 0;
            int outRate = 0;
            QString outFormat;
            mpv_node sourceNode{};
            mpv_node outNode{};
            if (mpv_get_property(m_mpv, "audio-params", MPV_FORMAT_NODE, &sourceNode) >= 0) {
                sourceRate = mpvNodeMapInt(sourceNode, "samplerate");
                mpv_free_node_contents(&sourceNode);
            }
            if (mpv_get_property(m_mpv, "audio-out-params", MPV_FORMAT_NODE, &outNode) >= 0) {
                outRate = mpvNodeMapInt(outNode, "samplerate");
                outFormat = mpvNodeMapString(outNode, "format");
                mpv_free_node_contents(&outNode);
            }

            if (outRate <= 0) {
                const QString cardId = alsaCardIdFromDevice(m_audioDevice);
                const QList<AlsaHwCard> cards = loadAlsaHwCards();
                for (const AlsaHwCard &card : cards) {
                    if (card.id != cardId) {
                        continue;
                    }
                    QFile hw(QStringLiteral("/proc/asound/card%1/pcm0p/sub0/hw_params").arg(card.index));
                    if (hw.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        const QString text = QString::fromUtf8(hw.readAll());
                        const QRegularExpression rateRe(QStringLiteral(R"(rate:\s*(\d+))"));
                        const QRegularExpressionMatch m = rateRe.match(text);
                        if (m.hasMatch()) {
                            outRate = m.captured(1).toInt();
                        }
                        if (text.contains(QLatin1String("closed"))) {
                            outRate = 0;
                        }
                    }
                    break;
                }
            }

            const QString reservedNote =
                QStringLiteral(" (hidden from OS settings until bit-perfect is off)");
            if (m_currentPath.isEmpty() || (!m_playing && sourceRate <= 0 && outRate <= 0)) {
                status = QStringLiteral("Exclusive ALSA ready") + reservedNote;
                active = true;
            } else if (sourceRate > 0 && outRate > 0 && sourceRate == outRate) {
                status = QStringLiteral("%1 → DAC %2 exclusive")
                             .arg(formatRateHz(sourceRate), formatRateHz(outRate));
                if (!outFormat.isEmpty()) {
                    status += QStringLiteral(" · ") + outFormat;
                }
                status += reservedNote;
                active = true;
            } else if (outRate > 0 && sourceRate > 0 && sourceRate != outRate) {
                status = QStringLiteral("Rate mismatch: source %1, DAC %2 — not bit-perfect.")
                             .arg(formatRateHz(sourceRate), formatRateHz(outRate));
            } else if (outRate > 0) {
                status = QStringLiteral("DAC locked at %1 exclusive").arg(formatRateHz(outRate))
                         + reservedNote;
                active = true;
            } else {
                status = QStringLiteral("Exclusive device selected — waiting for output params…");
                active = true;
            }
        }
    }

    if (m_bitPerfectActive == active && m_bitPerfectStatus == status) {
        return;
    }
    m_bitPerfectActive = active;
    m_bitPerfectStatus = status;
    emit bitPerfectStatusChanged();
}

bool PlaybackService::reopenEnginePreservingPlayback() {
    return reopenEnginePreservingPlayback(m_position, m_playing && !m_paused);
}

bool PlaybackService::reopenEnginePreservingPlayback(double resumePos, bool wasPlaying) {
    const bool hadTrack = !m_currentPath.isEmpty() && m_queueIndex >= 0
        && m_queueIndex < m_queue.size();

    shutdownMpv();
    m_pollTimer.start(16);
    if (!initMpv()) {
        m_error = QStringLiteral("Audio engine restart failed");
        emit playbackChanged();
        refreshBitPerfectStatus();
        return false;
    }
    setVolume(m_volume);

    if (hadTrack) {
        loadCurrentQueueTrackResuming(resumePos, wasPlaying);
    }
    holdResumePlaying(wasPlaying);
    refreshBitPerfectStatus();
    return true;
}

void PlaybackService::setDacPassthrough(bool enabled) {
    if (m_dacPassthrough == enabled) {
        return;
    }

    const bool hadTrack = !m_currentPath.isEmpty() && m_queueIndex >= 0
        && m_queueIndex < m_queue.size();
    const double resumePos = m_position;
    const bool wasPlaying = m_playing && !m_paused;

    m_dacPassthrough = enabled;
    if (enabled && m_muted) {
        m_muted = false;
        emit volumeChanged();
    }

    if (enabled) {
        if (!acquireExclusiveForCurrentDevice()) {
            m_error = QStringLiteral("Could not reserve the DAC from PipeWire");
            emit playbackChanged();
        }
        emit dacPassthroughChanged();
        emit audioBackendChanged();
        if (!reopenEnginePreservingPlayback(resumePos, wasPlaying)) {
            QThread::msleep(500);
            acquireExclusiveForCurrentDevice();
            reopenEnginePreservingPlayback(resumePos, wasPlaying);
        }
        return;
    }

    // Drop exclusive ALSA before giving the card back to PipeWire.
    const QString alsaHw =
        isAlsaHwDevice(m_audioDevice) ? m_audioDevice : QString();

    if (m_mpv) {
        shutdownMpv();
        m_pollTimer.start(16);
    }
    restoreExclusiveLease();

    if (!alsaHw.isEmpty()) {
        QString pw;
        for (int i = 0; i < 15; ++i) {
            pw = pipewireSinkForAlsaHwDevice(alsaHw);
            if (!pw.isEmpty()) {
                break;
            }
            QThread::msleep(200);
        }
        m_audioDevice = pw; // empty → system default
    }

    emit dacPassthroughChanged();
    emit audioBackendChanged();

    if (!initMpv()) {
        QThread::msleep(500);
        if (m_audioDevice.isEmpty() && !preferredUsbDacDevice().isEmpty()) {
            const QString pw = pipewireSinkForAlsaHwDevice(preferredUsbDacDevice());
            if (!pw.isEmpty()) {
                m_audioDevice = pw;
            }
        }
        if (!initMpv()) {
            m_error = QStringLiteral("Audio engine failed to restart after leaving bit-perfect");
            emit playbackChanged();
            refreshBitPerfectStatus();
            return;
        }
    }
    setVolume(m_volume);
    if (hadTrack) {
        loadCurrentQueueTrackResuming(resumePos, wasPlaying);
    }
    holdResumePlaying(wasPlaying);
    refreshBitPerfectStatus();
}

bool PlaybackService::retryExclusiveOutput() {
    if (!m_dacPassthrough) {
        return false;
    }
    const double resumePos = m_position;
    const bool wasPlaying = m_playing && !m_paused;
    if (m_mpv) {
        shutdownMpv();
        m_pollTimer.start(16);
    }
    restoreExclusiveLease();
    if (!acquireExclusiveForCurrentDevice()) {
        refreshBitPerfectStatus();
        return false;
    }
    QThread::msleep(300);
    if (!reopenEnginePreservingPlayback(resumePos, wasPlaying)) {
        refreshBitPerfectStatus();
        return false;
    }
    refreshBitPerfectStatus();
    return true;
}

void PlaybackService::setAudioDevice(const QString &name) {
    if (m_audioDevice == name) {
        return;
    }
    m_audioDevice = name;
    if (m_dacPassthrough) {
        const double resumePos = m_position;
        const bool wasPlaying = m_playing && !m_paused;

        if (m_mpv) {
            shutdownMpv();
            m_pollTimer.start(16);
        }
        if (!acquireExclusiveForCurrentDevice()) {
            refreshBitPerfectStatus();
            emit audioBackendChanged();
            return;
        }
        reopenEnginePreservingPlayback(resumePos, wasPlaying);
        emit audioBackendChanged();
        return;
    }
    if (m_mpv) {
        const QString oldBackend = m_audioBackend;
        runMpvCommand(m_mpv, {QStringLiteral("set"), QStringLiteral("audio-device"),
                              name.isEmpty() ? QStringLiteral("auto") : name});
        refreshAudioBackend();
        if (m_audioBackend == oldBackend) {
            emit audioBackendChanged();
        }
        refreshBitPerfectStatus();
    } else {
        emit audioBackendChanged();
        refreshBitPerfectStatus();
    }
}

QVariantList PlaybackService::audioDeviceList() const {
    QList<AudioDeviceEntry> raw;
    if (m_mpv) {
        mpv_node node;
        if (mpv_get_property(m_mpv, "audio-device-list", MPV_FORMAT_NODE, &node) >= 0) {
            if (node.format == MPV_FORMAT_NODE_ARRAY && node.u.list) {
                for (int i = 0; i < node.u.list->num; ++i) {
                    const mpv_node *entry = &node.u.list->values[i];
                    if (entry->format != MPV_FORMAT_NODE_MAP || !entry->u.list) {
                        continue;
                    }
                    AudioDeviceEntry device;
                    for (int j = 0; j < entry->u.list->num; ++j) {
                        const char *key = entry->u.list->keys[j];
                        const mpv_node *value = &entry->u.list->values[j];
                        if (!key || value->format != MPV_FORMAT_STRING || !value->u.string) {
                            continue;
                        }
                        if (qstrcmp(key, "name") == 0) {
                            device.name = QString::fromUtf8(value->u.string);
                        } else if (qstrcmp(key, "description") == 0) {
                            device.description = QString::fromUtf8(value->u.string);
                        }
                    }
                    if (device.name.isEmpty()) {
                        continue;
                    }
                    if (device.description.isEmpty()) {
                        device.description = device.name;
                    }
                    raw.append(device);
                }
            }
            mpv_free_node_contents(&node);
        }
    }
    return filterAudioDevices(raw, m_dacPassthrough, m_audioDevice, loadPulseSinkMeta());
}

void PlaybackService::handleMprisPlay() { play(); }
void PlaybackService::handleMprisPause() { pause(); }
void PlaybackService::handleMprisStop() { stop(); }
void PlaybackService::handleMprisNext() { next(); }
void PlaybackService::handleMprisPrevious() { previous(); }
void PlaybackService::handleMprisSeek(double offset) { seekRelative(offset); }
