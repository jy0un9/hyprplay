#include "LibraryService.h"

#include "AudioFormats.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QThread>
#include <QVariantMap>
#include <QtConcurrent>

#include <taglib/fileref.h>
#include <taglib/tag.h>

namespace {

constexpr int kWatchDebounceMs = 1500;
constexpr int kMaxWatchedDirs = 8000;

QString cacheDbPath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/library.db");
    if (!QFile::exists(path)) {
        // Prefer the nested legacy path (full library) over the incomplete flat copy.
        const QStringList legacyCandidates = {
            QDir::homePath() + QStringLiteral("/.local/share/qt-music/qt-music/library.db"),
            QDir::homePath() + QStringLiteral("/.local/share/qt-music/library.db"),
        };
        for (const QString &legacy : legacyCandidates) {
            if (QFile::exists(legacy) && QFile::copy(legacy, path)) {
                break;
            }
        }
    }
    return path;
}

bool isAudioFile(const QString &path) {
    return isSupportedAudioFile(path);
}

bool pathUnderRoots(const QString &path, const QStringList &roots) {
    for (const QString &root : roots) {
        if (path == root || path.startsWith(root + QLatin1Char('/'))) {
            return true;
        }
    }
    return false;
}

TrackInfo readTags(const QString &path) {
    TrackInfo info;
    info.path = path;

    const TagLib::FileRef ref(QFile::encodeName(path).constData());
    if (!ref.isNull() && ref.tag()) {
        const TagLib::Tag *tag = ref.tag();
        info.title = QString::fromUtf8(tag->title().toCString(true));
        info.artist = QString::fromUtf8(tag->artist().toCString(true));
        info.album = QString::fromUtf8(tag->album().toCString(true));
        info.trackNumber = tag->track();
    }

    if (info.title.isEmpty()) {
        info.title = QFileInfo(path).completeBaseName();
    }
    if (info.artist.isEmpty()) {
        info.artist = QStringLiteral("Unknown Artist");
    }
    if (info.album.isEmpty()) {
        info.album = QStringLiteral("Unknown Album");
    }

    if (!ref.isNull() && ref.audioProperties()) {
        info.durationMs = ref.audioProperties()->lengthInMilliseconds();
    }

    return info;
}

} // namespace

LibraryService::LibraryService(QObject *parent) : QObject(parent) {
    openDatabase();
    refreshTrackCount();

    m_watchDebounce.setSingleShot(true);
    m_watchDebounce.setInterval(kWatchDebounceMs);
    connect(&m_watchDebounce, &QTimer::timeout, this, [this]() {
        if (!m_watchEnabled || m_libraryRoots.isEmpty()) {
            return;
        }
        if (m_scanning) {
            m_watchRescanPending = true;
            return;
        }
        startScan(m_libraryRoots, false);
    });
}

LibraryService::~LibraryService() {
    m_watchDebounce.stop();
    clearWatchPaths();
    for (int i = 0; i < 600 && m_scanning; ++i) {
        QThread::msleep(50);
    }
    if (m_db.isOpen()) {
        m_db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("hyprplay_library"));
}

bool LibraryService::openDatabase() {
    const QString dbPath = cacheDbPath();
    const QString oldPath =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/library.db");
    if (!QFile::exists(dbPath) && QFile::exists(oldPath)) {
        QDir().mkpath(QFileInfo(dbPath).absolutePath());
        QFile::copy(oldPath, dbPath);
    }

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("hyprplay_library"));
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        m_scanStatus = QStringLiteral("Failed to open library database");
        emit scanStatusChanged();
        return false;
    }

    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS tracks ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "path TEXT UNIQUE NOT NULL,"
        "title TEXT,"
        "artist TEXT,"
        "album TEXT,"
        "track_number INTEGER,"
        "duration_ms INTEGER,"
        "modified_time INTEGER"
        ")"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_tracks_artist ON tracks(artist)"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_tracks_album ON tracks(album)"));
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS library_meta ("
        "key TEXT PRIMARY KEY,"
        "value TEXT NOT NULL"
        ")"));
    q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    q.exec(QStringLiteral("PRAGMA busy_timeout=5000"));
    return true;
}

QStringList LibraryService::normalizeRoots(const QStringList &roots) const {
    QStringList normalized;
    for (QString path : roots) {
        if (path.startsWith(QStringLiteral("~/"))) {
            path = QDir::homePath() + path.mid(1);
        }
        normalized << QDir(path).absolutePath();
    }
    normalized.sort();
    return normalized;
}

QString LibraryService::storedRootsKey() const {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT value FROM library_meta WHERE key = 'library_roots'"));
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    return {};
}

bool LibraryService::storedRootsMatch(const QStringList &roots) const {
    const QStringList normalized = normalizeRoots(roots);
    return !normalized.isEmpty() && storedRootsKey() == normalized.join(QLatin1Char('\n'));
}

void LibraryService::saveStoredRoots(const QStringList &roots, QSqlDatabase &db) {
    const QStringList normalized = normalizeRoots(roots);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO library_meta (key, value) VALUES ('library_roots', ?)"));
    q.addBindValue(normalized.join(QLatin1Char('\n')));
    q.exec();

    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO library_meta (key, value) VALUES ('last_scan_unix', ?)"));
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.exec();
}

void LibraryService::ensureLibrary(const QStringList &roots) {
    refreshTrackCount();
    const QStringList normalized = normalizeRoots(roots);
    m_libraryRoots = normalized;

    if (normalized.isEmpty()) {
        clearWatchPaths();
        m_scanStatus = QStringLiteral("No library paths configured");
        emit scanStatusChanged();
        return;
    }

    if (m_trackCount > 0 && storedRootsMatch(roots)) {
        m_scanStatus.clear();
        emit scanStatusChanged();
        emit scanFinished(true);
        refreshWatchPaths();
        return;
    }

    if (m_trackCount > 0 && !m_scanOnLaunch) {
        m_scanStatus.clear();
        emit scanStatusChanged();
        emit scanFinished(true);
        refreshWatchPaths();
        return;
    }

    rescan(roots);
}

void LibraryService::setWatchEnabled(bool enabled) {
    if (m_watchEnabled == enabled) {
        if (enabled) {
            refreshWatchPaths();
        }
        return;
    }
    m_watchEnabled = enabled;
    emit watchEnabledChanged();
    if (!m_watchEnabled) {
        m_watchDebounce.stop();
        m_watchRescanPending = false;
        clearWatchPaths();
        return;
    }
    refreshWatchPaths();
}

void LibraryService::clearWatchPaths() {
    if (!m_watcher) {
        return;
    }
    const QStringList dirs = m_watcher->directories();
    if (!dirs.isEmpty()) {
        m_watcher->removePaths(dirs);
    }
    const QStringList files = m_watcher->files();
    if (!files.isEmpty()) {
        m_watcher->removePaths(files);
    }
}

void LibraryService::refreshWatchPaths() {
    if (!m_watchEnabled || m_libraryRoots.isEmpty()) {
        clearWatchPaths();
        return;
    }

    if (!m_watcher) {
        m_watcher = new QFileSystemWatcher(this);
        connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
                [this](const QString &) { scheduleWatchRescan(); });
        connect(m_watcher, &QFileSystemWatcher::fileChanged, this,
                [this](const QString &) { scheduleWatchRescan(); });
    }

    clearWatchPaths();

    QStringList dirs;
    dirs.reserve(256);
    for (const QString &root : m_libraryRoots) {
        if (!QDir(root).exists()) {
            continue;
        }
        dirs << root;
        QDirIterator it(root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            dirs << it.next();
            if (dirs.size() >= kMaxWatchedDirs) {
                break;
            }
        }
        if (dirs.size() >= kMaxWatchedDirs) {
            qWarning("library watch: capped at %d directories", kMaxWatchedDirs);
            break;
        }
    }

    if (dirs.isEmpty()) {
        return;
    }

    const QStringList failed = m_watcher->addPaths(dirs);
    if (!failed.isEmpty()) {
        qWarning("library watch: failed to watch %lld/%lld dirs (inotify limit?)",
                 static_cast<long long>(failed.size()), static_cast<long long>(dirs.size()));
    }
}

void LibraryService::scheduleWatchRescan() {
    if (!m_watchEnabled) {
        return;
    }
    m_watchDebounce.start();
}

void LibraryService::refreshTrackCount() {
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM tracks")) && q.next()) {
        m_trackCount = q.value(0).toInt();
        emit libraryChanged();
    }
}

void LibraryService::rescan(const QStringList &roots) {
    startScan(normalizeRoots(roots), true);
}

void LibraryService::startScan(const QStringList &roots, bool fullRebuild) {
    if (m_scanning) {
        if (!fullRebuild) {
            m_watchRescanPending = true;
        }
        return;
    }
    if (roots.isEmpty()) {
        return;
    }

    m_scanning = true;
    emit scanningChanged();
    m_scanStatus = fullRebuild ? QStringLiteral("Scanning library...")
                               : QStringLiteral("Updating library…");
    emit scanStatusChanged();

    m_libraryRoots = roots;
    const QString dbPath = cacheDbPath();

    (void)QtConcurrent::run([this, roots, dbPath, fullRebuild]() {
        {
            QSqlDatabase db =
                QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("hyprplay_scan"));
            db.setDatabaseName(dbPath);
            if (!db.open()) {
                QMetaObject::invokeMethod(this, [this]() {
                    m_scanning = false;
                    emit scanningChanged();
                    m_scanStatus = QStringLiteral("Scan failed: could not open database");
                    emit scanStatusChanged();
                }, Qt::QueuedConnection);
                QSqlDatabase::removeDatabase(QStringLiteral("hyprplay_scan"));
                return;
            }

            {
                QSqlQuery pragma(db);
                pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
                pragma.exec(QStringLiteral("PRAGMA busy_timeout=5000"));
            }

            scanRoots(roots, fullRebuild, db);
            saveStoredRoots(roots, db);

            db.close();
            QSqlDatabase::removeDatabase(QStringLiteral("hyprplay_scan"));
        }

        QMetaObject::invokeMethod(this, [this, fullRebuild]() {
            m_scanning = false;
            emit scanningChanged();
            refreshTrackCount();
            m_scanStatus.clear();
            emit scanStatusChanged();
            emit scanFinished(true);
            refreshWatchPaths();
            if (m_watchRescanPending && m_watchEnabled) {
                m_watchRescanPending = false;
                scheduleWatchRescan();
            }
        }, Qt::QueuedConnection);
    });
}

void LibraryService::scanRoots(const QStringList &roots, bool fullRebuild, QSqlDatabase &db) {
    if (fullRebuild) {
        QSqlQuery deleteQ(db);
        deleteQ.exec(QStringLiteral("DELETE FROM tracks"));
    }

    QSet<QString> seenPaths;
    int found = 0;
    int updated = 0;
    for (const QString &root : roots) {
        QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = QFileInfo(it.next()).absoluteFilePath();
            if (!isAudioFile(path)) {
                continue;
            }
            seenPaths.insert(path);
            ++found;
            if (!fullRebuild && !fileNeedsIngest(db, path)) {
                continue;
            }
            if (ingestFile(db, path)) {
                ++updated;
                if (updated % 100 == 0) {
                    const QString status = fullRebuild
                                              ? QStringLiteral("Scanning... %1 tracks").arg(found)
                                              : QStringLiteral("Updating... %1 changed").arg(updated);
                    QMetaObject::invokeMethod(this, [this, status]() {
                        m_scanStatus = status;
                        emit scanStatusChanged();
                    }, Qt::QueuedConnection);
                }
            }
        }
    }

    if (!fullRebuild) {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral("SELECT path FROM tracks"))) {
            QStringList stale;
            while (q.next()) {
                const QString path = q.value(0).toString();
                if (pathUnderRoots(path, roots) && !seenPaths.contains(path)) {
                    stale << path;
                }
            }
            QSqlQuery del(db);
            del.prepare(QStringLiteral("DELETE FROM tracks WHERE path = ?"));
            for (const QString &path : stale) {
                del.addBindValue(path);
                del.exec();
            }
        }
    }
}

bool LibraryService::fileNeedsIngest(QSqlDatabase &db, const QString &path) {
    const qint64 mtime = QFileInfo(path).lastModified().toSecsSinceEpoch();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT modified_time FROM tracks WHERE path = ?"));
    q.addBindValue(path);
    if (q.exec() && q.next()) {
        return q.value(0).toLongLong() != mtime;
    }
    return true;
}

bool LibraryService::ingestFile(QSqlDatabase &db, const QString &path) {
    const TrackInfo info = readTags(path);
    const QFileInfo fi(path);

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO tracks "
        "(path, title, artist, album, track_number, duration_ms, modified_time) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(QFileInfo(info.path).absoluteFilePath());
    q.addBindValue(info.title);
    q.addBindValue(info.artist);
    q.addBindValue(info.album);
    q.addBindValue(info.trackNumber);
    q.addBindValue(info.durationMs);
    q.addBindValue(fi.lastModified().toSecsSinceEpoch());
    return q.exec();
}

QStringList LibraryService::artists() const {
    QStringList result;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT DISTINCT artist FROM tracks ORDER BY artist COLLATE NOCASE"));
    while (q.next()) {
        result << q.value(0).toString();
    }
    return result;
}

QStringList LibraryService::albumsForArtist(const QString &artist) const {
    QStringList result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT DISTINCT album FROM tracks WHERE artist = ? ORDER BY album COLLATE NOCASE"));
    q.addBindValue(artist);
    q.exec();
    while (q.next()) {
        result << q.value(0).toString();
    }
    return result;
}

QVariantList LibraryService::tracksForAlbum(const QString &artist, const QString &album) const {
    QVariantList result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, path, title, artist, album, track_number, duration_ms "
        "FROM tracks WHERE artist = ? AND album = ? "
        "ORDER BY track_number, title COLLATE NOCASE"));
    q.addBindValue(artist);
    q.addBindValue(album);
    q.exec();
    while (q.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), q.value(0));
        row.insert(QStringLiteral("path"), q.value(1));
        row.insert(QStringLiteral("title"), q.value(2));
        row.insert(QStringLiteral("artist"), q.value(3));
        row.insert(QStringLiteral("album"), q.value(4));
        row.insert(QStringLiteral("trackNumber"), q.value(5));
        row.insert(QStringLiteral("durationMs"), q.value(6));
        result << row;
    }
    return result;
}

QVariantList LibraryService::tracksForArtist(const QString &artist) const {
    QVariantList result;
    if (artist.isEmpty()) {
        return result;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, path, title, artist, album, track_number, duration_ms "
        "FROM tracks WHERE artist = ? "
        "ORDER BY album COLLATE NOCASE, track_number, title COLLATE NOCASE"));
    q.addBindValue(artist);
    q.exec();
    while (q.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), q.value(0));
        row.insert(QStringLiteral("path"), q.value(1));
        row.insert(QStringLiteral("title"), q.value(2));
        row.insert(QStringLiteral("artist"), q.value(3));
        row.insert(QStringLiteral("album"), q.value(4));
        row.insert(QStringLiteral("trackNumber"), q.value(5));
        row.insert(QStringLiteral("durationMs"), q.value(6));
        result << row;
    }
    return result;
}

QVariantList LibraryService::allTracks() const {
    QVariantList result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, path, title, artist, album, track_number, duration_ms "
        "FROM tracks ORDER BY artist COLLATE NOCASE, album COLLATE NOCASE, track_number LIMIT 5000"));
    q.exec();
    while (q.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), q.value(0));
        row.insert(QStringLiteral("path"), q.value(1));
        row.insert(QStringLiteral("title"), q.value(2));
        row.insert(QStringLiteral("artist"), q.value(3));
        row.insert(QStringLiteral("album"), q.value(4));
        row.insert(QStringLiteral("trackNumber"), q.value(5));
        row.insert(QStringLiteral("durationMs"), q.value(6));
        result << row;
    }
    return result;
}

QString LibraryService::sampleTrackPathForArtist(const QString &artist) const {
    if (artist.isEmpty()) {
        return {};
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT path FROM tracks WHERE artist = ? LIMIT 1"));
    q.addBindValue(artist);
    if (!q.exec() || !q.next()) {
        return {};
    }
    return q.value(0).toString();
}

QVariantList LibraryService::searchTracks(const QString &query) const {
    QVariantList result;
    if (query.trimmed().isEmpty()) {
        return result;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, path, title, artist, album, track_number, duration_ms "
        "FROM tracks WHERE title LIKE ? OR artist LIKE ? OR album LIKE ? "
        "ORDER BY artist, album, track_number LIMIT 200"));
    const QString pattern = QStringLiteral("%%1%").arg(query.trimmed());
    q.addBindValue(pattern);
    q.addBindValue(pattern);
    q.addBindValue(pattern);
    q.exec();
    while (q.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), q.value(0));
        row.insert(QStringLiteral("path"), q.value(1));
        row.insert(QStringLiteral("title"), q.value(2));
        row.insert(QStringLiteral("artist"), q.value(3));
        row.insert(QStringLiteral("album"), q.value(4));
        row.insert(QStringLiteral("trackNumber"), q.value(5));
        row.insert(QStringLiteral("durationMs"), q.value(6));
        result << row;
    }
    return result;
}

TrackInfo LibraryService::trackById(qint64 id) const {
    TrackInfo info;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, path, title, artist, album, track_number, duration_ms "
        "FROM tracks WHERE id = ?"));
    q.addBindValue(id);
    q.exec();
    if (q.next()) {
        info.id = q.value(0).toLongLong();
        info.path = q.value(1).toString();
        info.title = q.value(2).toString();
        info.artist = q.value(3).toString();
        info.album = q.value(4).toString();
        info.trackNumber = q.value(5).toInt();
        info.durationMs = q.value(6).toLongLong();
    }
    return info;
}

namespace {

QVariantMap trackRowFromQuery(const QSqlQuery &q) {
    QVariantMap row;
    row.insert(QStringLiteral("id"), q.value(0));
    row.insert(QStringLiteral("path"), q.value(1));
    row.insert(QStringLiteral("title"), q.value(2));
    row.insert(QStringLiteral("artist"), q.value(3));
    row.insert(QStringLiteral("album"), q.value(4));
    row.insert(QStringLiteral("trackNumber"), q.value(5));
    row.insert(QStringLiteral("durationMs"), q.value(6));
    row.insert(QStringLiteral("resolved"), true);
    return row;
}

QString normalizePlaylistPath(const QString &path) {
    return QString(path).replace(QLatin1Char('\\'), QLatin1Char('/')).trimmed().toLower();
}

} // namespace

QVariantMap LibraryService::trackByPath(const QString &path) const {
    if (!m_db.isOpen() || m_scanning) {
        return {};
    }
    const QString absPath = QFileInfo(path).absoluteFilePath();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, path, title, artist, album, track_number, duration_ms "
        "FROM tracks WHERE path = ?"));
    q.addBindValue(absPath);
    if (q.exec() && q.next()) {
        return trackRowFromQuery(q);
    }
    return {};
}

QVariantMap LibraryService::resolvePlaylistEntry(const QString &sourcePath,
                                                 const QString &display) const {
    if (!m_db.isOpen() || m_scanning) {
        return {};
    }

    QString portable = sourcePath.trimmed();
    if (portable.startsWith(QChar(0xFEFF))) {
        portable = portable.mid(1);
    }
    portable.replace(QLatin1Char('\\'), QLatin1Char('/'));

    if (portable.isEmpty()) {
        return {};
    }

    const QFileInfo directInfo(portable);
    if (directInfo.isAbsolute() && QFile::exists(portable)) {
        if (QVariantMap track = trackByPath(portable); !track.isEmpty()) {
            return track;
        }
    }

    for (const QString &root : m_libraryRoots) {
        const QString candidate = QDir(root).absoluteFilePath(portable);
        if (QFile::exists(candidate)) {
            if (QVariantMap track = trackByPath(candidate); !track.isEmpty()) {
                return track;
            }
        }
    }

    const QString normalized = normalizePlaylistPath(portable);
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT id, path, title, artist, album, track_number, duration_ms "
            "FROM tracks WHERE lower(path) LIKE ?"));
        q.addBindValue(QStringLiteral("%") + normalized);
        if (q.exec()) {
            QVariantMap best;
            int bestLen = 0;
            while (q.next()) {
                const QString path = q.value(1).toString();
                const QString pathNorm = normalizePlaylistPath(path);
                if (pathNorm.endsWith(normalized) || pathNorm == normalized) {
                    const int len = normalized.length();
                    if (len >= bestLen) {
                        bestLen = len;
                        best = trackRowFromQuery(q);
                    }
                }
            }
            if (!best.isEmpty()) {
                return best;
            }
        }
    }

    const QString fileName = QFileInfo(portable).fileName();
    if (!fileName.isEmpty()) {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT id, path, title, artist, album, track_number, duration_ms "
            "FROM tracks WHERE lower(path) LIKE ?"));
        q.addBindValue(QStringLiteral("%/") + fileName.toLower());
        QVariantList matches;
        if (q.exec()) {
            while (q.next()) {
                matches << trackRowFromQuery(q);
            }
        }
        if (matches.size() == 1) {
            return matches.first().toMap();
        }
        if (matches.size() > 1) {
            const QStringList segments = portable.split(QLatin1Char('/'), Qt::SkipEmptyParts);
            const QString parent =
                segments.size() >= 2 ? segments.at(segments.size() - 2).toLower() : QString();
            for (const QVariant &item : matches) {
                const QVariantMap track = item.toMap();
                const QString path = track.value(QStringLiteral("path")).toString().toLower();
                if (!parent.isEmpty() && path.contains(parent)) {
                    return track;
                }
            }
            return matches.first().toMap();
        }
    }

    if (!display.trimmed().isEmpty()) {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT id, path, title, artist, album, track_number, duration_ms "
            "FROM tracks WHERE (artist || ' - ' || title) LIKE ? OR title LIKE ? "
            "LIMIT 1"));
        const QString pattern = QStringLiteral("%") + display.trimmed() + QStringLiteral("%");
        q.addBindValue(pattern);
        q.addBindValue(pattern);
        if (q.exec() && q.next()) {
            return trackRowFromQuery(q);
        }
    }

    return {};
}

bool LibraryService::reingestFile(const QString &path) {
    if (!m_db.isOpen() || m_scanning || path.isEmpty()) {
        return false;
    }
    if (!ingestFile(m_db, path)) {
        return false;
    }
    refreshTrackCount();
    emit libraryChanged();
    return true;
}

QString LibraryService::artistFolderForTrack(const QString &path) const {
    if (path.isEmpty()) {
        return {};
    }
    const QFileInfo track(path);
    const QDir albumDir = track.dir();
    const QString artistDir = QFileInfo(albumDir.absolutePath()).dir().absolutePath();
    if (!QDir(artistDir).exists()) {
        return {};
    }
    for (const QString &root : m_libraryRoots) {
        const QString absRoot = QDir(root).absolutePath();
        if (artistDir.startsWith(absRoot + QLatin1Char('/')) || artistDir == absRoot) {
            return artistDir;
        }
    }
    return {};
}
