#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantList>

class ConfigService;
class LibraryService;

class PlaylistService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList playlistNames READ playlistNames NOTIFY playlistsChanged)
    Q_PROPERTY(QString selectedPlaylist READ selectedPlaylist NOTIFY selectionChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int playlistCount READ playlistCount NOTIFY playlistsChanged)

public:
    explicit PlaylistService(ConfigService *config, LibraryService *library, QObject *parent = nullptr);

    QStringList playlistNames() const { return m_playlistNames; }
    QString selectedPlaylist() const { return m_selectedPlaylist; }
    QString status() const { return m_status; }
    int playlistCount() const { return m_playlistNames.size(); }

    Q_INVOKABLE void reload();
    Q_INVOKABLE void selectPlaylist(const QString &name);
    Q_INVOKABLE QVariantList tracksForSelectedPlaylist() const;
    Q_INVOKABLE int trackCountForPlaylist(const QString &name) const;
    QHash<QString, int> trackCounts() const { return m_trackCounts; }
    Q_INVOKABLE bool createPlaylist(const QString &name);
    Q_INVOKABLE bool deletePlaylist(const QString &name);
    Q_INVOKABLE bool renamePlaylist(const QString &oldName, const QString &newName);
    Q_INVOKABLE bool removeTrackFromPlaylist(const QString &playlistName, int index);
    Q_INVOKABLE bool addTrackToPlaylist(const QString &playlistName, const QVariantMap &track);

public slots:
    void setStatus(const QString &status);

signals:
    void playlistsChanged();
    void selectionChanged();
    void statusChanged();
    void playlistTracksChanged();

private:
    struct LoadedPlaylist {
        QString name;
        QString filePath;
        QVariantList entries;
    };

    QString playlistsDirectory() const;
    LoadedPlaylist loadPlaylistFile(const QString &filePath) const;
    static QString sanitizePlaylistName(const QString &name);
    static QString relativeTrackPath(const QString &absolutePath, const QStringList &roots);
    bool savePlaylist(const LoadedPlaylist &playlist) const;
    LoadedPlaylist playlistByName(const QString &name) const;

    ConfigService *m_config = nullptr;
    LibraryService *m_library = nullptr;
    QStringList m_playlistNames;
    QHash<QString, int> m_trackCounts;
    QString m_selectedPlaylist;
    QString m_status;
    mutable QVariantList m_selectedTracks;
    mutable bool m_selectedTracksDirty = true;
};
