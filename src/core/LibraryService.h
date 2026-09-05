#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QStringList>

struct TrackInfo {
    qint64 id = 0;
    QString path;
    QString title;
    QString artist;
    QString album;
    int trackNumber = 0;
    qint64 durationMs = 0;
};

class LibraryService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int trackCount READ trackCount NOTIFY libraryChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanStatusChanged)

public:
    explicit LibraryService(QObject *parent = nullptr);
    ~LibraryService() override;

    bool scanning() const { return m_scanning; }
    int trackCount() const { return m_trackCount; }
    QString scanStatus() const { return m_scanStatus; }

    void setScanOnLaunch(bool enabled) { m_scanOnLaunch = enabled; }

    Q_INVOKABLE void ensureLibrary(const QStringList &roots);
    Q_INVOKABLE void rescan(const QStringList &roots);
    Q_INVOKABLE QStringList artists() const;
    Q_INVOKABLE QStringList albumsForArtist(const QString &artist) const;
    Q_INVOKABLE QVariantList tracksForAlbum(const QString &artist, const QString &album) const;
    Q_INVOKABLE QVariantList tracksForArtist(const QString &artist) const;
    Q_INVOKABLE QVariantList allTracks() const;
    Q_INVOKABLE QVariantList searchTracks(const QString &query) const;
    Q_INVOKABLE QString sampleTrackPathForArtist(const QString &artist) const;
    Q_INVOKABLE QVariantMap trackByPath(const QString &path) const;
    Q_INVOKABLE QVariantMap resolvePlaylistEntry(const QString &sourcePath,
                                                 const QString &display = {}) const;
    Q_INVOKABLE TrackInfo trackById(qint64 id) const;
    Q_INVOKABLE bool reingestFile(const QString &path);
    Q_INVOKABLE QString artistFolderForTrack(const QString &path) const;

signals:
    void scanningChanged();
    void scanStatusChanged();
    void libraryChanged();
    void scanFinished(bool success);

private:
    bool openDatabase();
    QStringList normalizeRoots(const QStringList &roots) const;
    QString storedRootsKey() const;
    bool storedRootsMatch(const QStringList &roots) const;
    void saveStoredRoots(const QStringList &roots, QSqlDatabase &db);
    void scanRoots(const QStringList &roots, bool fullRebuild, QSqlDatabase &db);
    static bool ingestFile(QSqlDatabase &db, const QString &path);
    void refreshTrackCount();

    QSqlDatabase m_db;
    QStringList m_libraryRoots;
    bool m_scanning = false;
    bool m_scanOnLaunch = true;
    int m_trackCount = 0;
    QString m_scanStatus;
};
