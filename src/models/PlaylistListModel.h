#pragma once

#include <QAbstractListModel>
#include <QStringList>

class PlaylistListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        TrackCountRole,
    };

    explicit PlaylistListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void setPlaylists(const QStringList &names, const QHash<QString, int> &trackCounts);

signals:
    void countChanged();

private:
    QStringList m_names;
    QHash<QString, int> m_trackCounts;
};
