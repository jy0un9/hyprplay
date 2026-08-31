#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QObject>

class PlaybackService;

class MprisPlayer : public QObject {
    Q_OBJECT

public:
    explicit MprisPlayer(PlaybackService *playback, QObject *parent = nullptr);

    void publish();

private:
    void registerAdaptor();
    void connectPlayback();
    void updateMetadata();
    void updatePlaybackStatus();

    PlaybackService *m_playback = nullptr;
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

public:
    explicit MprisPlayerAdaptor(MprisPlayer *player, PlaybackService *playback);

    QString playbackStatus() const;
    QVariantMap metadata() const;
    double volume() const;
    void setVolume(double volume);
    QStringList supportedMimeTypes() const {
        return {QStringLiteral("audio/flac"), QStringLiteral("audio/opus")};
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

signals:
    void playbackStatusChanged();
    void metadataChanged();

private:
    MprisPlayer *m_player = nullptr;
    PlaybackService *m_playback = nullptr;
};
