#include "LibraryService.h"

#include <QDir>
#include <QDateTime>
#include <QDirIterator>
#include <QFileInfo>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QThreadPool>
#include <QVariantMap>
#include <QtConcurrent>
#include <QThread>

#include <taglib/fileref.h>
#include <taglib/tag.h>

namespace {

QString cacheDbPath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/library.db");
}

bool isAudioFile(const QString &path) {
    const QString lower = path.toLower();
    return lower.endsWith(QLatin1String(".flac")) || lower.endsWith(QLatin1String(".opus"));
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
}

LibraryService::~LibraryService() {
    for (int i = 0; i < 600 && m_scanning; ++i) {
        QThread::msleep(50);
    }
    if (m_db.isOpen()) {
        m_db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("qt_music_library"));
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

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("qt_music_library"));
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
        m_scanStatus = QStringLiteral("No library paths configured");
        emit scanStatusChanged();
        return;
    }

    if (m_trackCount > 0 && storedRootsMatch(roots)) {
        m_scanStatus = QStringLiteral("Library loaded (%1 tracks)").arg(m_trackCount);
        emit scanStatusChanged();
        emit scanFinished(true);
        return;
    }

    if (m_trackCount > 0 && !m_scanOnLaunch) {
        m_scanStatus = QStringLiteral("Library loaded (%1 tracks) — launch scan off").arg(m_trackCount);
        emit scanStatusChanged();
        emit scanFinished(true);
        return;
    }

    rescan(roots);
}

void LibraryService::refreshTrackCount() {
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM tracks")) && q.next()) {
        m_trackCount = q.value(0).toInt();
        emit libraryChanged();
    }
}

void LibraryService::rescan(const QStringList &roots) {
    if (m_scanning) {
        return;
    }

    m_scanning = true;
    emit scanningChanged();
    m_scanStatus = QStringLiteral("Scanning library...");
    emit scanStatusChanged();

    const QStringList normalized = normalizeRoots(roots);
    m_libraryRoots = normalized;
    const QString dbPath = cacheDbPath();

    (void)QtConcurrent::run([this, normalized, dbPath]() {
        {
            QSqlDatabase db =
                QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("qt_music_scan"));
            db.setDatabaseName(dbPath);
            if (!db.open()) {
                QMetaObject::invokeMethod(this, [this]() {
                    m_scanning = false;
                    emit scanningChanged();
                    m_scanStatus = QStringLiteral("Scan failed: could not open database");
                    emit scanStatusChanged();
                }, Qt::QueuedConnection);
                QSqlDatabase::removeDatabase(QStringLiteral("qt_music_scan"));
                return;
            }

            {
                QSqlQuery pragma(db);
                pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
                pragma.exec(QStringLiteral("PRAGMA busy_timeout=5000"));
            }

            scanRoots(normalized, true, db);
            saveStoredRoots(normalized, db);

            db.close();
            QSqlDatabase::removeDatabase(QStringLiteral("qt_music_scan"));
        }

        QMetaObject::invokeMethod(this, [this]() {
            m_scanning = false;
            emit scanningChanged();
            refreshTrackCount();
            m_scanStatus = QStringLiteral("Scan complete (%1 tracks)").arg(m_trackCount);
            emit scanStatusChanged();
            emit scanFinished(true);
        }, Qt::QueuedConnection);
    });
}

void LibraryService::scanRoots(const QStringList &roots, bool fullRebuild, QSqlDatabase &db) {
    if (fullRebuild) {
        QSqlQuery deleteQ(db);
        deleteQ.exec(QStringLiteral("DELETE FROM tracks"));
    }

    int found = 0;
    for (const QString &root : roots) {
        QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = QFileInfo(it.next()).absoluteFilePath();
            if (!isAudioFile(path)) {
                continue;
            }
            if (ingestFile(db, path)) {
                ++found;
                if (found % 100 == 0) {
                    const QString status = QStringLiteral("Scanning... %1 tracks").arg(found);
                    QMetaObject::invokeMethod(this, [this, status]() {
                        m_scanStatus = status;
                        emit scanStatusChanged();
                    }, Qt::QueuedConnection);
                }
            }
        }
    }
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
