#pragma once

#include <QObject>
#include <QVariantList>

class ConfigService;
class QNetworkAccessManager;
class QNetworkReply;

class DiscogsService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool hasToken READ hasToken NOTIFY tokenChanged)

public:
    explicit DiscogsService(ConfigService *config, QObject *parent = nullptr);
    ~DiscogsService() override;

    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    bool hasToken() const;

    Q_INVOKABLE QString loadToken() const;
    Q_INVOKABLE void setToken(const QString &token);
    Q_INVOKABLE void searchArtists(const QString &query);
    Q_INVOKABLE void searchReleases(const QString &artist, const QString &album);
    Q_INVOKABLE void fetchArtist(const QString &artistName, const QString &artistFolder,
                                 quint64 discogsId = 0);
    Q_INVOKABLE void fetchRelease(const QString &artist, const QString &album,
                                  const QString &albumFolder, quint64 releaseId);

signals:
    void busyChanged();
    void statusChanged();
    void tokenChanged();
    void artistsSearchFinished(const QVariantList &results);
    void releasesSearchFinished(const QVariantList &results);
    void artistFetched(const QString &artistName, bool success);
    void releaseFetched(const QString &artist, const QString &album, bool success);

private:
    enum class Op {
        None,
        SearchArtists,
        SearchReleases,
        FetchArtistSearch,
        FetchArtistDetail,
        FetchArtistImage,
        FetchReleaseDetail,
        FetchReleaseImage,
    };

    void setBusy(bool busy);
    void setStatus(const QString &status);
    void abortActiveReply();
    void finishOp();
    void failArtist(const QString &message);
    void failRelease(const QString &message);
    void startGet(const QUrl &url, bool authenticated);
    void startArtistDetail(quint64 id);
    void startReleaseDetail(quint64 id);
    void onReplyFinished();

    ConfigService *m_config = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    bool m_busy = false;
    QString m_status;
    Op m_op = Op::None;

    QString m_artistName;
    QString m_artistFolder;
    QString m_albumArtist;
    QString m_albumName;
    QString m_albumFolder;
    QString m_pendingImageUrl;
    bool m_artistProfileOk = false;
    bool m_releaseInfoOk = false;
};
