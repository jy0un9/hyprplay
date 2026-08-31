#pragma once

#include <QAbstractListModel>

class TrackListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        PathRole,
        TitleRole,
        ArtistRole,
        AlbumRole,
        TrackNumberRole,
        DurationMsRole,
        ResolvedRole,
    };

    explicit TrackListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void setTracks(const QVariantList &tracks);
    Q_INVOKABLE QVariantMap trackAt(int index) const;

signals:
    void countChanged();

private:
    QVariantList m_tracks;
};
