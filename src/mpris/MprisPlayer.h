#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QObject>

#include "core/PlaybackService.h"

class TrackMediaService;

class MprisPlayer : public QObject {
    Q_OBJECT

public:
    explicit MprisPlayer(PlaybackService *playback, TrackMediaService *media,
                         QObject *parent = nullptr);

    void publish();

signals:
    void raiseRequested();

private:
    void registerAdaptor();
    void connectPlayback();
    void updateMetadata();
    void updatePlaybackStatus();

    PlaybackService *m_playback = nullptr;
    TrackMediaService *m_media = nullptr;
    QString m_serviceName;
    bool m_registered = false;
};

class MprisRootAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit)
    Q_PROPERTY(bool CanRaise READ canRaise)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes)

public:
    explicit MprisRootAdaptor(MprisPlayer *player);

    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return true; }
    QString identity() const { return QStringLiteral("Qt Music"); }
    QString desktopEntry() const { return QStringLiteral("qt-music"); }
    QStringList supportedUriSchemes() const { return {QStringLiteral("file")}; }
    QStringList supportedMimeTypes() const {
        return {QStringLiteral("audio/flac"), QStringLiteral("audio/opus"),
                QStringLiteral("audio/ogg")};
    }

public slots:
    void Quit();
    void Raise();

signals:
    void RaiseRequested();

private:
    MprisPlayer *m_player = nullptr;
};

class MprisPlayerAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus NOTIFY playbackStatusChanged)
    Q_PROPERTY(QVariantMap Metadata READ metadata NOTIFY metadataChanged)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes)
    Q_PROPERTY(bool CanGoNext READ canGoNext NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool CanPlay READ canPlay NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool CanPause READ canPause NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool CanSeek READ canSeek NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool CanControl READ canControl NOTIFY capabilitiesChanged)
    Q_PROPERTY(double MinimumRate READ minimumRate)
    Q_PROPERTY(double MaximumRate READ maximumRate)
    Q_PROPERTY(qlonglong Position READ position)

public:
    explicit MprisPlayerAdaptor(MprisPlayer *player, PlaybackService *playback,
                                TrackMediaService *media);

    QString playbackStatus() const;
    QVariantMap metadata() const;
    double volume() const;
    void setVolume(double volume);
    QStringList supportedMimeTypes() const {
        return {QStringLiteral("audio/flac"), QStringLiteral("audio/opus")};
    }
    bool canGoNext() const { return hasTrack(); }
    bool canGoPrevious() const { return hasTrack(); }
    bool canPlay() const { return true; }
    bool canPause() const { return hasTrack(); }
    bool canSeek() const { return hasTrack(); }
    bool canControl() const { return true; }
    double minimumRate() const { return 1.0; }
    double maximumRate() const { return 1.0; }
    qlonglong position() const {
        return static_cast<qlonglong>(m_playback->position() * 1'000'000);
    }

public slots:
    void Play();
    void Pause();
    void PlayPause();
    void Stop();
    void Next();
    void Previous();
    void Seek(double offset);
    void SetPosition(const QDBusObjectPath &trackId, double position);

    void notifyMetadataChanged();
    void notifyPlaybackStatusChanged();
    void notifyCapabilitiesChanged();

signals:
    void playbackStatusChanged();
    void metadataChanged();
    void capabilitiesChanged();
    void Seeked(qlonglong position);
    void positionChangedInternally();

private:
    bool hasTrack() const { return !m_playback->currentPath().isEmpty(); }
    qlonglong currentPositionMicros() const;
    void emitSeeked();
    static void emitPropertiesChanged(const QString &interfaceName, const QVariantMap &changed);

    MprisPlayer *m_player = nullptr;
    PlaybackService *m_playback = nullptr;
    TrackMediaService *m_media = nullptr;
    int m_trackSerial = 0;
};
