#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

class ConfigService;

class BeetsService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)

public:
    explicit BeetsService(ConfigService *config, QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    bool available() const;

    Q_INVOKABLE bool importAlbum(const QString &albumDir, bool autotag = true);
    Q_INVOKABLE bool modifyByPath(const QString &path, const QVariantMap &fields);
    Q_INVOKABLE bool modifyByQuery(const QString &query, const QVariantMap &fields);
    Q_INVOKABLE bool modifyPaths(const QStringList &paths, const QVariantMap &fields);
    Q_INVOKABLE bool syncAlbumTags(const QString &libraryArtist, const QString &libraryAlbum,
                                   const QVariantMap &fields, const QStringList &fallbackPaths);
    Q_INVOKABLE bool syncArtistTags(const QString &libraryArtist, const QVariantMap &fields,
                                    const QStringList &fallbackPaths);
    Q_INVOKABLE QString version();

signals:
    void busyChanged();
    void statusChanged();
    void availabilityChanged();

private:
    QStringList globalArgs() const;
    QStringList modifyFieldArgs(const QVariantMap &fields) const;
    bool runBeet(const QStringList &args, QString *output = nullptr);
    void setBusy(bool busy);
    void setStatus(const QString &status);

    ConfigService *m_config = nullptr;
    bool m_busy = false;
    QString m_status;
    mutable bool m_availableChecked = false;
    mutable bool m_available = false;
};
