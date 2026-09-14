#pragma once

#include <QObject>
#include <QHash>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class ConfigService;
class DiscogsService;
class QNetworkAccessManager;
class QNetworkReply;

class MetadataSearchService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QVariantList candidates READ candidates NOTIFY candidatesChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(QVariantMap currentFields READ currentFields NOTIFY fieldChoicesChanged)
    Q_PROPERTY(QVariantList fieldChoices READ fieldChoices NOTIFY fieldChoicesChanged)
    Q_PROPERTY(QVariantList titleFixProposals READ titleFixProposals NOTIFY titleFixProposalsChanged)
    Q_PROPERTY(QVariantList titleFixUnmatched READ titleFixUnmatched NOTIFY titleFixProposalsChanged)
    Q_PROPERTY(int localTrackCount READ localTrackCount NOTIFY localTrackCountChanged)

public:
    explicit MetadataSearchService(ConfigService *config, DiscogsService *discogs,
                                   QObject *parent = nullptr);
    ~MetadataSearchService() override;

    bool searching() const { return m_searching; }
    QString status() const { return m_status; }
    QVariantList candidates() const { return m_candidates; }
    int selectedIndex() const { return m_selectedIndex; }
    QVariantMap currentFields() const { return m_currentFields; }
    QVariantList fieldChoices() const;
    QVariantList titleFixProposals() const { return m_titleFixProposals; }
    QVariantList titleFixUnmatched() const { return m_titleFixUnmatched; }
    int localTrackCount() const { return m_localTrackCount; }

    Q_INVOKABLE void setSelectedIndex(int index);
    Q_INVOKABLE void setFieldChecked(const QString &key, bool checked);
    Q_INVOKABLE QVariantMap checkedFields() const;
    Q_INVOKABLE void setLocalTracksForTitleFix(const QVariantList &tracks);
    Q_INVOKABLE void setLocalTrackCount(int count);
    Q_INVOKABLE void setTitleFixChecked(int index, bool checked);
    Q_INVOKABLE QVariantList checkedTitleFixProposals() const;
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setStatus(const QString &status);

    Q_INVOKABLE void setCurrentFields(const QVariantMap &fields);
    Q_INVOKABLE void searchRelease(const QString &artist, const QString &album,
                                   const QString &albumArtist = {},
                                   const QVariantList &formats = {},
                                   const QString &edition = {});

signals:
    void searchingChanged();
    void statusChanged();
    void candidatesChanged();
    void selectedIndexChanged();
    void fieldChoicesChanged();
    void titleFixProposalsChanged();
    void localTrackCountChanged();
    void searchFinished(bool success);

private:
    enum class PendingKind { None, SearchMusicBrainz, SearchDiscogs, DetailMusicBrainz, DetailDiscogs };

    void setSearching(bool searching);
    void rebuildFieldChoices();
    void beginReleaseDetailFetch(int index);
    void applyReleaseDetailFields(int index, int fetchId, const QVariantMap &fields);
    void rebuildTitleFixProposals();
    void abortNetwork();
    void startGet(const QUrl &url, const QList<QPair<QByteArray, QByteArray>> &headers,
                  PendingKind kind);
    void onReplyFinished();
    void finishSearchIfReady();
    static QVariantMap normalizeCandidateFields(const QVariantMap &fields);

    ConfigService *m_config = nullptr;
    DiscogsService *m_discogs = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    bool m_searching = false;
    QString m_status;
    QVariantList m_candidates;
    int m_selectedIndex = -1;
    QVariantMap m_currentFields;
    QHash<QString, bool> m_fieldChecked;
    int m_detailFetchId = 0;
    int m_searchId = 0;

    PendingKind m_pendingKind = PendingKind::None;
    QString m_searchArtist;
    QString m_searchAlbum;
    QString m_searchEdition;
    QStringList m_searchFormats;
    QVariantList m_searchAccum;
    int m_searchesRemaining = 0;
    int m_detailIndex = -1;
    QVariantList m_localTracksForTitleFix;
    int m_localTrackCount = 0;
    QVariantList m_titleFixProposals;
    QVariantList m_titleFixUnmatched;
    QHash<int, bool> m_titleFixChecked;
};
