#include "PlaylistListModel.h"

PlaylistListModel::PlaylistListModel(QObject *parent) : QAbstractListModel(parent) {}

int PlaylistListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_names.size();
}

QVariant PlaylistListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_names.size()) {
        return {};
    }

    const QString name = m_names.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return name;
    case TrackCountRole:
        return m_trackCounts.value(name, 0);
    default:
        return {};
    }
}

QHash<int, QByteArray> PlaylistListModel::roleNames() const {
    return {
        {NameRole, "name"},
        {TrackCountRole, "trackCount"},
    };
}

void PlaylistListModel::setPlaylists(const QStringList &names,
                                     const QHash<QString, int> &trackCounts) {
    beginResetModel();
    m_names = names;
    m_trackCounts = trackCounts;
    endResetModel();
    emit countChanged();
}
