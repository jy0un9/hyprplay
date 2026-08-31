#pragma once

#include <QObject>
#include <QVariantList>

class ConfigService;

class DiscogsService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool hasToken READ hasToken NOTIFY tokenChanged)

public:
    explicit DiscogsService(ConfigService *config, QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    bool hasToken() const;

    Q_INVOKABLE QString loadToken() const;
    Q_INVOKABLE void setToken(const QString &token);
    Q_INVOKABLE QVariantList searchArtists(const QString &query) const;
    Q_INVOKABLE void fetchArtist(const QString &artistName, const QString &artistFolder,
                                 quint64 discogsId = 0);

signals:
    void busyChanged();
    void statusChanged();
    void tokenChanged();
    void artistFetched(const QString &artistName, bool success);

private:
    void setBusy(bool busy);
    void setStatus(const QString &status);
    bool fetchArtistById(quint64 id, const QString &artistName, const QString &artistFolder);

    ConfigService *m_config = nullptr;
    bool m_busy = false;
    QString m_status;
};
