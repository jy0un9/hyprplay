#include "ArtistModel.h"

ArtistModel::ArtistModel(QObject *parent) : QAbstractListModel(parent) {}

int ArtistModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_artists.size();
}

QVariant ArtistModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_artists.size()) {
        return {};
    }
    if (role == NameRole || role == Qt::DisplayRole) {
        return m_artists.at(index.row());
    }
    return {};
}

QHash<int, QByteArray> ArtistModel::roleNames() const {
    return {{NameRole, "name"}};
}

void ArtistModel::setArtists(const QStringList &artists) {
    beginResetModel();
    m_artists = artists;
    endResetModel();
    emit countChanged();
}

QString ArtistModel::artistAt(int index) const {
    if (index < 0 || index >= m_artists.size()) {
        return {};
    }
    return m_artists.at(index);
}
