#include "ArtistMediaService.h"

#include "ConfigService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

namespace {

bool hasImageExtension(const QString &fileName) {
    const QString ext = QFileInfo(fileName).suffix().toLower();
    return ext == QStringLiteral("jpg") || ext == QStringLiteral("jpeg")
           || ext == QStringLiteral("png") || ext == QStringLiteral("webp")
           || ext == QStringLiteral("gif") || ext == QStringLiteral("bmp")
           || ext == QStringLiteral("jfif");
}

bool isPreferredArtistImageName(const QString &fileName) {
    const QString lower = fileName.toLower();
    return lower.startsWith(QStringLiteral("artist."))
           || lower.contains(QStringLiteral("cover")) || lower.contains(QStringLiteral("folder"))
           || lower.contains(QStringLiteral("front")) || lower.contains(QStringLiteral("album"))
           || lower.contains(QStringLiteral("art"));
}

QString fileUrlIfExists(const QString &path) {
    if (path.isEmpty() || !QFile::exists(path)) {
        return {};
    }
    return QUrl::fromLocalFile(path).toString();
}

bool isUnderLibraryRoots(const QString &dirPath, ConfigService *config) {
    if (!config) {
        return false;
    }
    const QString absDir = QDir(dirPath).absolutePath();
    for (const QString &root : config->libraryPaths()) {
        const QString absRoot = QDir(config->expandPath(root)).absolutePath();
        if (absDir.startsWith(absRoot + QLatin1Char('/')) || absDir == absRoot) {
            return true;
        }
    }
    return false;
}

} // namespace

ArtistMediaService::ArtistMediaService(ConfigService *config, QObject *parent)
    : QObject(parent), m_config(config) {}

void ArtistMediaService::clearMedia() {
    m_artistImageUrl.clear();
    m_profileText.clear();
    m_currentArtist.clear();
    emit mediaChanged();
}

void ArtistMediaService::loadForArtist(const QString &artistName, const QString &sampleTrackPath) {
    if (artistName.isEmpty()) {
        clearMedia();
        return;
    }

    if (artistName == m_currentArtist) {
        return;
    }

    m_currentArtist = artistName;
    m_artistImageUrl.clear();
    m_profileText.clear();

    const QString artistDir = resolveArtistFolder(sampleTrackPath, m_config);
    if (!artistDir.isEmpty()) {
        m_artistImageUrl = fileUrlIfExists(findArtistImage(artistDir));
        m_profileText = loadProfileText(artistDir);
    }

    emit mediaChanged();
}

QString ArtistMediaService::resolveArtistFolder(const QString &sampleTrackPath, ConfigService *config) {
    if (sampleTrackPath.isEmpty()) {
        return {};
    }

    const QFileInfo track(sampleTrackPath);
    const QDir albumDir = track.dir();
    const QString artistDir = QFileInfo(albumDir.absolutePath()).dir().absolutePath();
    if (!QDir(artistDir).exists()) {
        return {};
    }

    if (!isUnderLibraryRoots(artistDir, config)) {
        return {};
    }

    return artistDir;
}

QString ArtistMediaService::findArtistImage(const QString &artistDir) {
    static const QStringList names = {
        QStringLiteral("artist.jpg"),  QStringLiteral("artist.jpeg"),
        QStringLiteral("artist.png"),  QStringLiteral("artist.webp"),
        QStringLiteral("artist.gif"),  QStringLiteral("cover.jpg"),
        QStringLiteral("cover.jpeg"),  QStringLiteral("cover.png"),
        QStringLiteral("folder.jpg"),  QStringLiteral("folder.jpeg"),
        QStringLiteral("folder.png"),  QStringLiteral("front.jpg"),
        QStringLiteral("front.jpeg"),  QStringLiteral("front.png"),
    };

    for (const QString &name : names) {
        const QString candidate = artistDir + QLatin1Char('/') + name;
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }

    QString preferred;
    QString fallback;
    const QStringList entries = QDir(artistDir).entryList(QDir::Files);
    for (const QString &entry : entries) {
        if (!hasImageExtension(entry)) {
            continue;
        }
        const QString path = artistDir + QLatin1Char('/') + entry;
        if (isPreferredArtistImageName(entry)) {
            preferred = path;
            break;
        }
        if (fallback.isEmpty()) {
            fallback = path;
        }
    }

    return !preferred.isEmpty() ? preferred : fallback;
}

QString ArtistMediaService::loadProfileText(const QString &artistDir) {
    const QString path = artistDir + QStringLiteral("/profile.txt");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    const QString text = QString::fromUtf8(file.readAll()).trimmed();
    if (text.isEmpty() || isRedirectProfile(text)) {
        return {};
    }
    return text;
}

bool ArtistMediaService::isRedirectProfile(const QString &profile) {
    const QString lower = profile.toLower();
    return lower.contains(QStringLiteral("please use")) || lower.contains(QStringLiteral("please see"))
           || lower.contains(QStringLiteral("see artist"))
           || (lower.contains(QStringLiteral("for band")) && lower.contains(QStringLiteral("use ")))
           || (lower.contains(QStringLiteral("for the band")) && lower.contains(QStringLiteral("use ")));
}
