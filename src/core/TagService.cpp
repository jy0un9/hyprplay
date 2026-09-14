#include "TagService.h"

#include "LibraryService.h"

#include <QFile>
#include <QFileInfo>
#include <QUuid>

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/flacfile.h>
#include <taglib/xiphcomment.h>
#include <taglib/opusfile.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4file.h>
#include <taglib/tpropertymap.h>

namespace {

QString tagString(const TagLib::String &value) {
    return QString::fromUtf8(value.toCString(true));
}

TagLib::String toTagString(const QString &value) {
    return TagLib::String(value.toUtf8().constData(), TagLib::String::UTF8);
}

bool applyFields(TagLib::Tag *tag, TagLib::Ogg::XiphComment *comment, const QVariantMap &fields) {
    if (!tag) {
        return false;
    }

    if (fields.contains(QStringLiteral("title"))) {
        tag->setTitle(toTagString(fields.value(QStringLiteral("title")).toString().trimmed()));
    }
    if (fields.contains(QStringLiteral("artist"))) {
        tag->setArtist(toTagString(fields.value(QStringLiteral("artist")).toString().trimmed()));
    }
    if (fields.contains(QStringLiteral("album"))) {
        tag->setAlbum(toTagString(fields.value(QStringLiteral("album")).toString().trimmed()));
    }
    if (fields.contains(QStringLiteral("genre"))) {
        tag->setGenre(toTagString(fields.value(QStringLiteral("genre")).toString().trimmed()));
    }
    if (fields.contains(QStringLiteral("trackNumber"))) {
        tag->setTrack(fields.value(QStringLiteral("trackNumber")).toInt());
    }
    if (fields.contains(QStringLiteral("year"))) {
        tag->setYear(fields.value(QStringLiteral("year")).toInt());
    }
    if (comment && fields.contains(QStringLiteral("albumArtist"))) {
        const QString albumArtist = fields.value(QStringLiteral("albumArtist")).toString().trimmed();
        comment->addField("ALBUMARTIST", toTagString(albumArtist), true);
    }
    return true;
}

void setAlbumArtistField(QVariantMap *row, TagLib::Ogg::XiphComment *comment) {
    if (!row || !comment) {
        return;
    }
    const TagLib::PropertyMap props = comment->properties();
    const TagLib::StringList values = props[TagLib::String("ALBUMARTIST")];
    if (!values.isEmpty()) {
        row->insert(QStringLiteral("albumArtist"), tagString(values.front()));
    }
}

void setAlbumArtistFromMap(QVariantMap *row, const TagLib::PropertyMap &props) {
    if (!row) {
        return;
    }
    const TagLib::StringList values = props[TagLib::String("ALBUMARTIST")];
    if (!values.isEmpty()) {
        row->insert(QStringLiteral("albumArtist"), tagString(values.front()));
        return;
    }
    const TagLib::StringList tpe2 = props[TagLib::String("TPE2")];
    if (!tpe2.isEmpty()) {
        row->insert(QStringLiteral("albumArtist"), tagString(tpe2.front()));
    }
}

bool applyPropertyAlbumArtist(TagLib::PropertyMap *props, const QVariantMap &fields) {
    if (!props || !fields.contains(QStringLiteral("albumArtist"))) {
        return true;
    }
    const QString albumArtist = fields.value(QStringLiteral("albumArtist")).toString().trimmed();
    if (albumArtist.isEmpty()) {
        props->erase(TagLib::String("ALBUMARTIST"));
    } else {
        (*props)[TagLib::String("ALBUMARTIST")] = TagLib::StringList(toTagString(albumArtist));
    }
    return true;
}

bool saveFileTags(const QString &path, const QVariantMap &fields, QString *error) {
    const QString lower = path.toLower();
    if (lower.endsWith(QStringLiteral(".flac"))) {
        TagLib::FLAC::File file(QFile::encodeName(path).constData());
        if (!file.isValid()) {
            if (error) {
                *error = QStringLiteral("Could not open FLAC file");
            }
            return false;
        }
        TagLib::Ogg::XiphComment *comment = file.xiphComment(true);
        if (!applyFields(file.tag(), comment, fields)) {
            if (error) {
                *error = QStringLiteral("Could not read FLAC tags");
            }
            return false;
        }
        if (!file.save()) {
            if (error) {
                *error = QStringLiteral("Failed to save FLAC tags");
            }
            return false;
        }
        return true;
    }

    if (lower.endsWith(QStringLiteral(".opus"))) {
        TagLib::Ogg::Opus::File file(QFile::encodeName(path).constData());
        if (!file.isValid()) {
            if (error) {
                *error = QStringLiteral("Could not open Opus file");
            }
            return false;
        }
        TagLib::Ogg::XiphComment *comment = file.tag();
        if (!applyFields(file.tag(), comment, fields)) {
            if (error) {
                *error = QStringLiteral("Could not read Opus tags");
            }
            return false;
        }
        if (!file.save()) {
            if (error) {
                *error = QStringLiteral("Failed to save Opus tags");
            }
            return false;
        }
        return true;
    }

    if (lower.endsWith(QStringLiteral(".mp3"))) {
        TagLib::MPEG::File file(QFile::encodeName(path).constData());
        if (!file.isValid()) {
            if (error) {
                *error = QStringLiteral("Could not open MP3 file");
            }
            return false;
        }
        if (!applyFields(file.tag(), nullptr, fields)) {
            if (error) {
                *error = QStringLiteral("Could not read MP3 tags");
            }
            return false;
        }
        TagLib::PropertyMap props = file.properties();
        applyPropertyAlbumArtist(&props, fields);
        file.setProperties(props);
        if (!file.save()) {
            if (error) {
                *error = QStringLiteral("Failed to save MP3 tags");
            }
            return false;
        }
        return true;
    }

    if (lower.endsWith(QStringLiteral(".m4a")) || lower.endsWith(QStringLiteral(".aac"))) {
        TagLib::MP4::File file(QFile::encodeName(path).constData());
        if (!file.isValid()) {
            if (error) {
                *error = QStringLiteral("Could not open M4A/AAC file");
            }
            return false;
        }
        if (!applyFields(file.tag(), nullptr, fields)) {
            if (error) {
                *error = QStringLiteral("Could not read M4A/AAC tags");
            }
            return false;
        }
        TagLib::PropertyMap props = file.properties();
        applyPropertyAlbumArtist(&props, fields);
        file.setProperties(props);
        if (!file.save()) {
            if (error) {
                *error = QStringLiteral("Failed to save M4A/AAC tags");
            }
            return false;
        }
        return true;
    }

    TagLib::FileRef ref(QFile::encodeName(path).constData());
    if (ref.isNull() || !ref.tag()) {
        if (error) {
            *error = QStringLiteral("Unsupported or unreadable audio file");
        }
        return false;
    }
    if (!applyFields(ref.tag(), nullptr, fields)) {
        if (error) {
            *error = QStringLiteral("Could not apply tags");
        }
        return false;
    }
    if (!ref.save()) {
        if (error) {
            *error = QStringLiteral("Failed to save tags");
        }
        return false;
    }
    return true;
}

QVariantMap readExtendedTags(const QString &path) {
    QVariantMap row;
    row.insert(QStringLiteral("path"), path);

    const TagLib::FileRef ref(QFile::encodeName(path).constData());
    if (ref.isNull() || !ref.tag()) {
        row.insert(QStringLiteral("title"), QFileInfo(path).completeBaseName());
        return row;
    }

    const TagLib::Tag *tag = ref.tag();
    row.insert(QStringLiteral("title"), tagString(tag->title()));
    row.insert(QStringLiteral("artist"), tagString(tag->artist()));
    row.insert(QStringLiteral("album"), tagString(tag->album()));
    row.insert(QStringLiteral("genre"), tagString(tag->genre()));
    row.insert(QStringLiteral("trackNumber"), tag->track());
    row.insert(QStringLiteral("year"), tag->year());

    const QString lower = path.toLower();
    if (lower.endsWith(QStringLiteral(".flac"))) {
        TagLib::FLAC::File file(QFile::encodeName(path).constData());
        if (file.isValid() && file.xiphComment()) {
            setAlbumArtistField(&row, file.xiphComment());
        }
    } else if (lower.endsWith(QStringLiteral(".opus"))) {
        TagLib::Ogg::Opus::File file(QFile::encodeName(path).constData());
        if (file.isValid() && file.tag()) {
            setAlbumArtistField(&row, file.tag());
        }
    } else if (lower.endsWith(QStringLiteral(".mp3"))) {
        TagLib::MPEG::File file(QFile::encodeName(path).constData());
        if (file.isValid()) {
            setAlbumArtistFromMap(&row, file.properties());
        }
    } else if (lower.endsWith(QStringLiteral(".m4a")) || lower.endsWith(QStringLiteral(".aac"))) {
        TagLib::MP4::File file(QFile::encodeName(path).constData());
        if (file.isValid()) {
            setAlbumArtistFromMap(&row, file.properties());
        }
    }

    if (row.value(QStringLiteral("title")).toString().isEmpty()) {
        row.insert(QStringLiteral("title"), QFileInfo(path).completeBaseName());
    }
    return row;
}

} // namespace

TagService::TagService(LibraryService *library, QObject *parent)
    : QObject(parent), m_library(library) {}

QVariantMap TagService::loadTags(const QString &path) const {
    return readExtendedTags(path);
}

QStringList TagService::albumTagPaths(const QString &artist, const QString &album) const {
    QStringList paths;
    if (!m_library) {
        return paths;
    }
    const QVariantList tracks = m_library->tracksForAlbum(artist, album);
    for (const QVariant &item : tracks) {
        const QString trackPath = item.toMap().value(QStringLiteral("path")).toString();
        if (!trackPath.isEmpty()) {
            paths << trackPath;
        }
    }
    return paths;
}

QStringList TagService::artistTagPaths(const QString &artist) const {
    QStringList paths;
    if (!m_library || artist.isEmpty()) {
        return paths;
    }
    for (const QString &album : m_library->albumsForArtist(artist)) {
        paths << albumTagPaths(artist, album);
    }
    return paths;
}

bool TagService::saveArtistTags(const QString &artist, const QVariantMap &fields) {
    const QStringList paths = artistTagPaths(artist);
    if (paths.isEmpty()) {
        setStatus(QStringLiteral("No tracks for artist"));
        return false;
    }

    setSaving(true);
    int saved = 0;
    QString lastError;
    for (const QString &path : paths) {
        if (writeTagsAtomic(path, fields, &lastError)) {
            if (m_library) {
                m_library->reingestFile(path);
            }
            emit tagsSaved(path);
            ++saved;
        }
    }
    setSaving(false);

    if (saved == paths.size()) {
        setStatus(QStringLiteral("Updated %1 track(s)").arg(saved));
        return true;
    }
    setStatus(lastError.isEmpty()
                  ? QStringLiteral("Updated %1/%2 tracks").arg(saved).arg(paths.size())
                  : lastError);
    return saved > 0;
}

bool TagService::writeTagsAtomic(const QString &path, const QVariantMap &fields,
                                 QString *error) const {
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        if (error) {
            *error = QStringLiteral("File not found");
        }
        return false;
    }

    const QString backup = path + QStringLiteral(".bak");
    const QString temp =
        info.absolutePath() + QLatin1Char('/')
        + info.completeBaseName() + QStringLiteral(".tmp-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces) + QLatin1Char('.')
        + info.suffix();

    if (QFile::exists(backup)) {
        QFile::remove(backup);
    }
    if (!QFile::copy(path, backup)) {
        if (error) {
            *error = QStringLiteral("Could not create backup");
        }
        return false;
    }
    if (!QFile::copy(path, temp)) {
        QFile::remove(backup);
        if (error) {
            *error = QStringLiteral("Could not create temp file");
        }
        return false;
    }

    QString writeError;
    if (!saveFileTags(temp, fields, &writeError)) {
        QFile::remove(temp);
        QFile::remove(backup);
        if (error) {
            *error = writeError;
        }
        return false;
    }

    QFile::remove(path);
    if (!QFile::rename(temp, path)) {
        QFile::copy(backup, path);
        QFile::remove(temp);
        QFile::remove(backup);
        if (error) {
            *error = QStringLiteral("Could not replace original file");
        }
        return false;
    }

    QFile::remove(backup);
    return true;
}

bool TagService::writeTagsToFile(const QString &path, const QVariantMap &fields) {
    QString error;
    return writeTagsAtomic(path, fields, &error);
}

bool TagService::saveTrackTags(const QString &path, const QVariantMap &fields) {
    setSaving(true);
    QString error;
    const bool ok = writeTagsAtomic(path, fields, &error);
    if (ok && m_library) {
        m_library->reingestFile(path);
        emit tagsSaved(path);
        setStatus(QStringLiteral("Saved tags"));
    } else {
        setStatus(error.isEmpty() ? QStringLiteral("Failed to save tags") : error);
    }
    setSaving(false);
    return ok;
}

bool TagService::saveAlbumTags(const QString &artist, const QString &album,
                               const QVariantMap &fields) {
    const QStringList paths = albumTagPaths(artist, album);
    if (paths.isEmpty()) {
        setStatus(QStringLiteral("No tracks in album"));
        return false;
    }

    setSaving(true);
    int saved = 0;
    QString lastError;
    for (const QString &path : paths) {
        if (writeTagsAtomic(path, fields, &lastError)) {
            if (m_library) {
                m_library->reingestFile(path);
            }
            emit tagsSaved(path);
            ++saved;
        }
    }
    setSaving(false);

    if (saved == paths.size()) {
        setStatus(QStringLiteral("Updated %1 track(s)").arg(saved));
        return true;
    }
    setStatus(lastError.isEmpty()
                  ? QStringLiteral("Updated %1/%2 tracks").arg(saved).arg(paths.size())
                  : lastError);
    return saved > 0;
}

void TagService::setStatus(const QString &status) {
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}

void TagService::setSaving(bool saving) {
    if (m_saving == saving) {
        return;
    }
    m_saving = saving;
    emit savingChanged();
}
