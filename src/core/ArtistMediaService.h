#pragma once

#include <QObject>

class ConfigService;

class ArtistMediaService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString artistImageUrl READ artistImageUrl NOTIFY mediaChanged)
    Q_PROPERTY(QString profileText READ profileText NOTIFY mediaChanged)
    Q_PROPERTY(bool hasProfile READ hasProfile NOTIFY mediaChanged)

public:
    explicit ArtistMediaService(ConfigService *config, QObject *parent = nullptr);

    QString artistImageUrl() const { return m_artistImageUrl; }
    QString profileText() const { return m_profileText; }
    bool hasProfile() const { return !m_profileText.isEmpty(); }

    Q_INVOKABLE void loadForArtist(const QString &artistName, const QString &sampleTrackPath = {});
    Q_INVOKABLE void clearMedia();

signals:
    void mediaChanged();

private:
    static QString resolveArtistFolder(const QString &sampleTrackPath, ConfigService *config);
    static QString findArtistImage(const QString &artistDir);
    static QString loadProfileText(const QString &artistDir);
    static bool isRedirectProfile(const QString &profile);

    ConfigService *m_config = nullptr;
    QString m_artistImageUrl;
    QString m_profileText;
    QString m_currentArtist;
};
