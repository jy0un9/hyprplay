#pragma once

#include <QObject>
#include <QVariantMap>

class ConfigService;
class LibraryService;

class TagService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool saving READ saving NOTIFY savingChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit TagService(LibraryService *library, QObject *parent = nullptr);

    bool saving() const { return m_saving; }
    QString status() const { return m_status; }

    Q_INVOKABLE QVariantMap loadTags(const QString &path) const;
    Q_INVOKABLE bool saveTrackTags(const QString &path, const QVariantMap &fields);
    Q_INVOKABLE bool saveAlbumTags(const QString &artist, const QString &album,
                                   const QVariantMap &fields);
    Q_INVOKABLE bool saveArtistTags(const QString &artist, const QVariantMap &fields);
    Q_INVOKABLE bool writeTagsToFile(const QString &path, const QVariantMap &fields);
    Q_INVOKABLE QStringList albumTagPaths(const QString &artist, const QString &album) const;
    Q_INVOKABLE QStringList artistTagPaths(const QString &artist) const;

signals:
    void savingChanged();
    void statusChanged();
    void tagsSaved(const QString &path);

private:
    bool writeTagsAtomic(const QString &path, const QVariantMap &fields, QString *error) const;
    void setStatus(const QString &status);
    void setSaving(bool saving);

    LibraryService *m_library = nullptr;
    bool m_saving = false;
    QString m_status;
};
