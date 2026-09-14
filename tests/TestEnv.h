#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>

// Isolates XDG dirs so ConfigService / LibraryService / SecretsStore never
// touch the developer's real hyprplay config during tests.
class TestEnv {
public:
    TestEnv() {
        Q_ASSERT(m_root.isValid());
        m_config = m_root.filePath(QStringLiteral("config"));
        m_data = m_root.filePath(QStringLiteral("data"));
        m_cache = m_root.filePath(QStringLiteral("cache"));
        m_runtime = m_root.filePath(QStringLiteral("runtime"));
        QDir().mkpath(m_config);
        QDir().mkpath(m_data);
        QDir().mkpath(m_cache);
        QDir().mkpath(m_runtime);

        qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_config));
        qputenv("XDG_DATA_HOME", QFile::encodeName(m_data));
        qputenv("XDG_CACHE_HOME", QFile::encodeName(m_cache));
        qputenv("XDG_RUNTIME_DIR", QFile::encodeName(m_runtime));
        qputenv("HYPRPLAY_TEST_AO", QByteArrayLiteral("null"));

        // Unique connection-ish suffix via application name is fixed; wipe
        // leftover library connection if a previous test crashed.
        QCoreApplication::setOrganizationName(QString());
        QCoreApplication::setApplicationName(QStringLiteral("hyprplay"));
        QStandardPaths::setTestModeEnabled(true);
    }

    QString root() const { return m_root.path(); }
    QString configHome() const { return m_config; }
    QString dataHome() const { return m_data; }
    QString cacheHome() const { return m_cache; }

    QString libraryRoot() const {
        const QString path = m_root.filePath(QStringLiteral("Music"));
        QDir().mkpath(path);
        return path;
    }

    QString playlistsDir() const {
        const QString path = libraryRoot() + QStringLiteral("/Playlists");
        QDir().mkpath(path);
        return path;
    }

    QString lyricsDir() const {
        const QString path = libraryRoot() + QStringLiteral("/Lyrics");
        QDir().mkpath(path);
        return path;
    }

    QString inboxDir() const {
        const QString path = m_root.filePath(QStringLiteral("inbox"));
        QDir().mkpath(path);
        return path;
    }

private:
    QTemporaryDir m_root;
    QString m_config;
    QString m_data;
    QString m_cache;
    QString m_runtime;
};
