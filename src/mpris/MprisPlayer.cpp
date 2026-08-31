#include "MprisPlayer.h"

#include "../core/PlaybackService.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QUrl>

MprisPlayer::MprisPlayer(PlaybackService *playback, QObject *parent)
    : QObject(parent), m_playback(playback) {
    m_serviceName = QStringLiteral("org.mpris.MediaPlayer2.qt-music");
}

void MprisPlayer::publish() {
    registerAdaptor();
    connectPlayback();
    updateMetadata();
    updatePlaybackStatus();
}

void MprisPlayer::registerAdaptor() {
    if (m_registered) {
        return;
    }

    auto *rootAdaptor = new MprisRootAdaptor(this);
    new MprisPlayerAdaptor(this, m_playback);
    Q_UNUSED(rootAdaptor);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(m_serviceName)) {
        qWarning("MPRIS: failed to register service %s", qPrintable(m_serviceName));
        return;
    }

    if (!bus.registerObject(QStringLiteral("/org/mpris/MediaPlayer2"), this)) {
        qWarning("MPRIS: failed to register object");
        return;
    }

    m_registered = true;
    connect(rootAdaptor, &MprisRootAdaptor::RaiseRequested, this, []() {
        // Window raise handled by shell; no-op for MVP
    });
}

void MprisPlayer::connectPlayback() {
    connect(m_playback, &PlaybackService::playbackChanged, this,
            &MprisPlayer::updatePlaybackStatus);
    connect(m_playback, &PlaybackService::trackChanged, this, &MprisPlayer::updateMetadata);
    connect(m_playback, &PlaybackService::volumeChanged, this, &MprisPlayer::updatePlaybackStatus);
}

void MprisPlayer::updateMetadata() {
    if (!m_registered) {
        return;
    }
    auto *adaptor = findChild<MprisPlayerAdaptor *>();
    if (adaptor) {
        emit adaptor->metadataChanged();
    }
}

void MprisPlayer::updatePlaybackStatus() {
    if (!m_registered) {
        return;
    }
    auto *adaptor = findChild<MprisPlayerAdaptor *>();
    if (adaptor) {
        emit adaptor->playbackStatusChanged();
    }
}

MprisRootAdaptor::MprisRootAdaptor(MprisPlayer *player)
    : QDBusAbstractAdaptor(player), m_player(player) {}

void MprisRootAdaptor::Quit() {
    QCoreApplication::quit();
}

void MprisRootAdaptor::Raise() {
    emit RaiseRequested();
}

MprisPlayerAdaptor::MprisPlayerAdaptor(MprisPlayer *player, PlaybackService *playback)
    : QDBusAbstractAdaptor(player), m_player(player), m_playback(playback) {
    connect(playback, &PlaybackService::playbackChanged, this, &MprisPlayerAdaptor::playbackStatusChanged);
    connect(playback, &PlaybackService::trackChanged, this, &MprisPlayerAdaptor::metadataChanged);
}

QString MprisPlayerAdaptor::playbackStatus() const {
    if (m_playback->playing() && !m_playback->paused()) {
        return QStringLiteral("Playing");
    }
    if (m_playback->paused()) {
        return QStringLiteral("Paused");
    }
    return QStringLiteral("Stopped");
}

QVariantMap MprisPlayerAdaptor::metadata() const {
    QVariantMap meta;
    if (m_playback->currentPath().isEmpty()) {
        return meta;
    }

    meta.insert(QStringLiteral("mpris:trackid"),
                QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/qt/music/current"))));
    meta.insert(QStringLiteral("xesam:title"), m_playback->title());
    meta.insert(QStringLiteral("xesam:artist"), QStringList{m_playback->artist()});
    meta.insert(QStringLiteral("xesam:album"), m_playback->album());
    meta.insert(QStringLiteral("mpris:length"),
                static_cast<qlonglong>(m_playback->duration() * 1'000'000));
    meta.insert(QStringLiteral("xesam:url"), QUrl::fromLocalFile(m_playback->currentPath()));
    return meta;
}

double MprisPlayerAdaptor::volume() const {
    return m_playback->volume() / 100.0;
}

void MprisPlayerAdaptor::setVolume(double volume) {
    m_playback->setVolume(static_cast<int>(volume * 100.0));
}

void MprisPlayerAdaptor::Play() { m_playback->handleMprisPlay(); }
void MprisPlayerAdaptor::Pause() { m_playback->handleMprisPause(); }

void MprisPlayerAdaptor::PlayPause() {
    if (m_playback->playing() && !m_playback->paused()) {
        m_playback->handleMprisPause();
    } else {
        m_playback->handleMprisPlay();
    }
}

void MprisPlayerAdaptor::Stop() { m_playback->handleMprisStop(); }
void MprisPlayerAdaptor::Next() { m_playback->handleMprisNext(); }
void MprisPlayerAdaptor::Previous() { m_playback->handleMprisPrevious(); }
void MprisPlayerAdaptor::Seek(double offset) { m_playback->handleMprisSeek(offset); }

void MprisPlayerAdaptor::SetPosition(const QDBusObjectPath &trackId, double position) {
    Q_UNUSED(trackId);
    m_playback->seek(position / 1'000'000.0);
}
