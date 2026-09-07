#include "MprisPlayer.h"

#include "../core/PlaybackService.h"
#include "../core/TrackMediaService.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QFileInfo>
#include <QUrl>

namespace {

constexpr auto kMprisPath = "/org/mpris/MediaPlayer2";
constexpr auto kPlayerInterface = "org.mpris.MediaPlayer2.Player";
constexpr auto kPropertiesInterface = "org.freedesktop.DBus.Properties";

} // namespace

MprisPlayer::MprisPlayer(PlaybackService *playback, TrackMediaService *media, QObject *parent)
    : QObject(parent), m_playback(playback), m_media(media) {
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

    auto *rootAdaptor = new MprisRootAdaptor(this, m_playback);
    auto *playerAdaptor = new MprisPlayerAdaptor(this, m_playback, m_media);
    Q_UNUSED(playerAdaptor);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(m_serviceName)) {
        qWarning("MPRIS: failed to register service %s", qPrintable(m_serviceName));
        return;
    }

    if (!bus.registerObject(QString::fromLatin1(kMprisPath), this,
                            QDBusConnection::ExportAdaptors)) {
        qWarning("MPRIS: failed to register object");
        return;
    }

    m_registered = true;
    connect(rootAdaptor, &MprisRootAdaptor::RaiseRequested, this, &MprisPlayer::raiseRequested);
}

void MprisPlayer::connectPlayback() {
    connect(m_playback, &PlaybackService::playbackChanged, this,
            &MprisPlayer::updatePlaybackStatus);
    connect(m_playback, &PlaybackService::trackChanged, this, &MprisPlayer::updateMetadata);
    connect(m_media, &TrackMediaService::mediaChanged, this, &MprisPlayer::updateMetadata);
    connect(m_playback, &PlaybackService::volumeChanged, this, &MprisPlayer::updatePlaybackStatus);
    connect(m_playback, &PlaybackService::repeatModeChanged, this, [this]() {
        if (!m_registered) {
            return;
        }
        if (auto *adaptor = findChild<MprisPlayerAdaptor *>()) {
            adaptor->notifyLoopStatusChanged();
        }
    });
    connect(m_playback, &PlaybackService::shuffleChanged, this, [this]() {
        if (!m_registered) {
            return;
        }
        if (auto *adaptor = findChild<MprisPlayerAdaptor *>()) {
            adaptor->notifyShuffleChanged();
        }
    });
}

void MprisPlayer::updateMetadata() {
    if (!m_registered) {
        return;
    }
    auto *adaptor = findChild<MprisPlayerAdaptor *>();
    if (adaptor) {
        adaptor->notifyMetadataChanged();
    }
}

void MprisPlayer::updatePlaybackStatus() {
    if (!m_registered) {
        return;
    }
    auto *adaptor = findChild<MprisPlayerAdaptor *>();
    if (adaptor) {
        adaptor->notifyPlaybackStatusChanged();
    }
}

MprisRootAdaptor::MprisRootAdaptor(MprisPlayer *player, PlaybackService *playback)
    : QDBusAbstractAdaptor(player), m_player(player), m_playback(playback) {}

void MprisRootAdaptor::Quit() {
    QCoreApplication::quit();
}

void MprisRootAdaptor::Raise() {
    emit RaiseRequested();
}

void MprisRootAdaptor::OpenUri(const QString &uri) {
    if (!m_playback) {
        return;
    }
    const QUrl url(uri);
    QString path;
    if (url.isLocalFile()) {
        path = url.toLocalFile();
    } else if (uri.startsWith(QLatin1Char('/'))) {
        path = uri;
    }
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        qWarning("MPRIS OpenUri: unsupported or missing path: %s", qPrintable(uri));
        return;
    }
    const QString title = QFileInfo(path).completeBaseName();
    m_playback->playPath(path, title);
    emit RaiseRequested();
}

MprisPlayerAdaptor::MprisPlayerAdaptor(MprisPlayer *player, PlaybackService *playback,
                                       TrackMediaService *media)
    : QDBusAbstractAdaptor(player), m_player(player), m_playback(playback), m_media(media) {
    connect(playback, &PlaybackService::playbackChanged, this, [this]() {
        notifyPlaybackStatusChanged();
        if (m_playback->duration() > 0.0) {
            notifyMetadataChanged();
        }
    });
    connect(playback, &PlaybackService::trackChanged, this, [this]() {
        ++m_trackSerial;
        notifyMetadataChanged();
        notifyPlaybackStatusChanged();
    });
    connect(playback, &PlaybackService::positionChanged, this,
            &MprisPlayerAdaptor::positionChangedInternally);
    connect(media, &TrackMediaService::mediaChanged, this,
            &MprisPlayerAdaptor::notifyMetadataChanged);
}

void MprisPlayerAdaptor::emitPropertiesChanged(const QString &interfaceName,
                                               const QVariantMap &changed) {
    QDBusMessage msg = QDBusMessage::createSignal(QString::fromLatin1(kMprisPath),
                                                  QString::fromLatin1(kPropertiesInterface),
                                                  QStringLiteral("PropertiesChanged"));
    msg.setArguments({interfaceName, changed, QStringList{}});
    QDBusConnection::sessionBus().send(msg);
}

void MprisPlayerAdaptor::notifyMetadataChanged() {
    const QVariantMap meta = metadata();
    emitPropertiesChanged(QString::fromLatin1(kPlayerInterface),
                          {{QStringLiteral("Metadata"), meta}});
    emit metadataChanged();
    notifyCapabilitiesChanged();
}

void MprisPlayerAdaptor::notifyPlaybackStatusChanged() {
    const QString status = playbackStatus();
    emitPropertiesChanged(QString::fromLatin1(kPlayerInterface),
                          {{QStringLiteral("PlaybackStatus"), status}});
    emit playbackStatusChanged();
    notifyCapabilitiesChanged();
}

void MprisPlayerAdaptor::notifyCapabilitiesChanged() {
    emitPropertiesChanged(QString::fromLatin1(kPlayerInterface),
                          {{QStringLiteral("CanPlay"), canPlay()},
                           {QStringLiteral("CanPause"), canPause()},
                           {QStringLiteral("CanSeek"), canSeek()},
                           {QStringLiteral("CanGoNext"), canGoNext()},
                           {QStringLiteral("CanGoPrevious"), canGoPrevious()},
                           {QStringLiteral("CanControl"), canControl()}});
    emit capabilitiesChanged();
}

void MprisPlayerAdaptor::notifyLoopStatusChanged() {
    emitPropertiesChanged(QString::fromLatin1(kPlayerInterface),
                          {{QStringLiteral("LoopStatus"), loopStatus()}});
    emit loopStatusChanged();
}

void MprisPlayerAdaptor::notifyShuffleChanged() {
    emitPropertiesChanged(QString::fromLatin1(kPlayerInterface),
                          {{QStringLiteral("Shuffle"), shuffle()}});
    emit shuffleChanged();
}

qlonglong MprisPlayerAdaptor::currentPositionMicros() const {
    return static_cast<qlonglong>(m_playback->position() * 1'000'000);
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

QString MprisPlayerAdaptor::loopStatus() const {
    switch (m_playback->repeatMode()) {
    case 1:
        return QStringLiteral("Track");
    case 2:
        return QStringLiteral("Playlist");
    default:
        return QStringLiteral("None");
    }
}

void MprisPlayerAdaptor::setLoopStatus(const QString &status) {
    if (status == QLatin1String("Track")) {
        m_playback->setRepeatMode(1);
    } else if (status == QLatin1String("Playlist")) {
        m_playback->setRepeatMode(2);
    } else {
        m_playback->setRepeatMode(0);
    }
}

bool MprisPlayerAdaptor::shuffle() const {
    return m_playback->shuffle();
}

void MprisPlayerAdaptor::setShuffle(bool enabled) {
    m_playback->setShuffle(enabled);
}

QVariantMap MprisPlayerAdaptor::metadata() const {
    QVariantMap meta;
    if (m_playback->currentPath().isEmpty()) {
        return meta;
    }

    const QString trackId =
        QStringLiteral("/org/qt/music/track/%1").arg(qMax(1, m_trackSerial));
    meta.insert(QStringLiteral("mpris:trackid"),
                QVariant::fromValue(QDBusObjectPath(trackId)));

    const QString title = m_playback->title();
    const QString artist = m_playback->artist();
    const QString album = m_playback->album();

    if (!title.isEmpty()) {
        meta.insert(QStringLiteral("xesam:title"), title);
    }
    if (!artist.isEmpty()) {
        meta.insert(QStringLiteral("xesam:artist"), QStringList{artist});
    }
    if (!album.isEmpty()) {
        meta.insert(QStringLiteral("xesam:album"), album);
    }

    const qlonglong lengthMicros =
        static_cast<qlonglong>(m_playback->duration() * 1'000'000);
    if (lengthMicros > 0) {
        meta.insert(QStringLiteral("mpris:length"), lengthMicros);
    }

    meta.insert(QStringLiteral("xesam:url"),
                QUrl::fromLocalFile(m_playback->currentPath()).toString());
    if (m_media && !m_media->albumArtUrl().isEmpty()) {
        meta.insert(QStringLiteral("mpris:artUrl"), m_media->albumArtUrl());
    }
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

void MprisPlayerAdaptor::Seek(qlonglong Offset) {
    // MPRIS Seek offset is microseconds.
    m_playback->handleMprisSeek(static_cast<double>(Offset) / 1'000'000.0);
    emitSeeked();
}

void MprisPlayerAdaptor::SetPosition(const QDBusObjectPath &TrackId, qlonglong Position) {
    Q_UNUSED(TrackId);
    m_playback->seek(static_cast<double>(Position) / 1'000'000.0);
    emitSeeked();
}

void MprisPlayerAdaptor::emitSeeked() {
    QDBusMessage msg = QDBusMessage::createSignal(QString::fromLatin1(kMprisPath),
                                                  QString::fromLatin1(kPlayerInterface),
                                                  QStringLiteral("Seeked"));
    msg.setArguments({QVariant::fromValue(currentPositionMicros())});
    QDBusConnection::sessionBus().send(msg);
}
