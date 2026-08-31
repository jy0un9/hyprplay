#include "TrackListModel.h"

TrackListModel::TrackListModel(QObject *parent) : QAbstractListModel(parent) {}

int TrackListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_tracks.size();
}

QVariant TrackListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_tracks.size()) {
        return {};
    }
    const QVariantMap track = m_tracks.at(index.row()).toMap();
    switch (role) {
    case IdRole:
        return track.value(QStringLiteral("id"));
    case PathRole:
        return track.value(QStringLiteral("path"));
    case TitleRole:
        return track.value(QStringLiteral("title"));
    case ArtistRole:
        return track.value(QStringLiteral("artist"));
    case AlbumRole:
        return track.value(QStringLiteral("album"));
    case TrackNumberRole:
        return track.value(QStringLiteral("trackNumber"));
    case DurationMsRole:
        return track.value(QStringLiteral("durationMs"));
    case ResolvedRole:
        return track.value(QStringLiteral("resolved"), true);
    case Qt::DisplayRole:
        return track.value(QStringLiteral("title"));
    default:
        return {};
    }
}

QHash<int, QByteArray> TrackListModel::roleNames() const {
    return {
        {IdRole, "trackId"},
        {PathRole, "path"},
        {TitleRole, "title"},
        {ArtistRole, "artist"},
        {AlbumRole, "album"},
        {TrackNumberRole, "trackNumber"},
        {DurationMsRole, "durationMs"},
        {ResolvedRole, "resolved"},
    };
}

void TrackListModel::setTracks(const QVariantList &tracks) {
    beginResetModel();
    m_tracks = tracks;
    endResetModel();
    emit countChanged();
}

QVariantMap TrackListModel::trackAt(int index) const {
    if (index < 0 || index >= m_tracks.size()) {
        return {};
    }
    return m_tracks.at(index).toMap();
}
