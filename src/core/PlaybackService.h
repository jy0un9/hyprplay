#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVector>

#include <mpv/client.h>

#include "AudioDeviceFilter.h"

enum class RepeatMode { Off, Track, Queue };

class PlaybackService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool playing READ playing NOTIFY playbackChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY playbackChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY playbackChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY volumeChanged)
    Q_PROPERTY(QString title READ title NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(QString album READ album NOTIFY trackChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY trackChanged)
    Q_PROPERTY(int repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY shuffleChanged)
    Q_PROPERTY(QString error READ error NOTIFY playbackChanged)
    Q_PROPERTY(bool dacPassthrough READ dacPassthrough WRITE setDacPassthrough NOTIFY dacPassthroughChanged)
    Q_PROPERTY(QString audioBackend READ audioBackend NOTIFY audioBackendChanged)
    Q_PROPERTY(QString audioDevice READ audioDevice WRITE setAudioDevice NOTIFY audioBackendChanged)
    Q_PROPERTY(bool bitPerfectActive READ bitPerfectActive NOTIFY bitPerfectStatusChanged)
    Q_PROPERTY(QString bitPerfectStatus READ bitPerfectStatus NOTIFY bitPerfectStatusChanged)

public:
    explicit PlaybackService(QObject *parent = nullptr);
    ~PlaybackService() override;

    bool playing() const { return m_playing; }
    bool paused() const { return m_paused; }
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    int volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    QString title() const { return m_title; }
    QString artist() const { return m_artist; }
    QString album() const { return m_album; }
    QString currentPath() const { return m_currentPath; }
    int repeatMode() const { return static_cast<int>(m_repeatMode); }
    bool shuffle() const { return m_shuffle; }
    QString error() const { return m_error; }
    bool dacPassthrough() const { return m_dacPassthrough; }
    QString audioBackend() const { return m_audioBackend; }
    QString audioDevice() const { return m_audioDevice; }
    bool bitPerfectActive() const { return m_bitPerfectActive; }
    QString bitPerfectStatus() const { return m_bitPerfectStatus; }

    Q_INVOKABLE void playPath(const QString &path, const QString &title = {},
                              const QString &artist = {}, const QString &album = {});
    Q_INVOKABLE void playTracks(const QVariantList &tracks, int startIndex = 0);
    Q_INVOKABLE void togglePlayPause();
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void seekRelative(double delta);
    Q_INVOKABLE void setVolume(int volume);
    Q_INVOKABLE void setMuted(bool muted);
    Q_INVOKABLE void setRepeatMode(int mode);
    Q_INVOKABLE void setShuffle(bool enabled);
    Q_INVOKABLE void setDacPassthrough(bool enabled);
    Q_INVOKABLE void setAudioDevice(const QString &name);
    Q_INVOKABLE QVariantList audioDeviceList() const;
    Q_INVOKABLE bool retryExclusiveOutput();

    bool ensureMpv();

    void handleMprisPlay();
    void handleMprisPause();
    void handleMprisStop();
    void handleMprisNext();
    void handleMprisPrevious();
    void handleMprisSeek(double offset);

signals:
    void playbackChanged();
    void positionChanged();
    void volumeChanged();
    void trackChanged();
    void repeatModeChanged();
    void shuffleChanged();
    void trackFinished();
    void dacPassthroughChanged();
    void audioBackendChanged();
    void bitPerfectStatusChanged();

private:
    bool initMpv();
    void shutdownMpv();
    void pollMpv();
    void processMpvEvents();
    void handleMpvEvent(mpv_event *event);
    void syncFromMpv();
    void loadCurrentQueueTrack();
    void loadCurrentQueueTrackResuming(double resumePos, bool wasPlaying);
    void refreshAudioBackend();
    void refreshBitPerfectStatus();
    void ensureDacDeviceSelected();
    bool acquireExclusiveForCurrentDevice();
    void restoreExclusiveLease();
    bool reopenEnginePreservingPlayback();
    bool reopenEnginePreservingPlayback(double resumePos, bool wasPlaying);
    void holdResumePlaying(bool wasPlaying);
    void enforceResumePlaying();
    void rebuildShuffleBag(int preferFirstIndex);
    bool hasNextInQueue() const;

    static void mpvWakeup(void *ctx);

    mpv_handle *m_mpv = nullptr;
    QTimer m_pollTimer;

    bool m_playing = false;
    bool m_paused = false;
    double m_position = 0.0;
    double m_duration = 0.0;
    int m_volume = 80;
    bool m_muted = false;

    QString m_title;
    QString m_artist;
    QString m_album;
    QString m_currentPath;

    QVariantList m_queue;
    int m_queueIndex = -1;
    QVector<int> m_shuffleBag;
    int m_shufflePos = -1;
    RepeatMode m_repeatMode = RepeatMode::Off;
    bool m_shuffle = false;
    bool m_trackEndedPending = false;
    QString m_error;
    bool m_dacPassthrough = false;
    bool m_resumeHoldPlaying = false;
    QString m_audioBackend;
    QString m_audioDevice;
    bool m_bitPerfectActive = false;
    QString m_bitPerfectStatus;
    AlsaExclusiveLease m_alsaLease;
};
