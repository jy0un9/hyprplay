#pragma once

#include <QObject>
#include <QHash>
#include <QVariantList>
#include <QVariantMap>

class ConfigService;
class DiscogsService;

class MetadataSearchService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QVariantList candidates READ candidates NOTIFY candidatesChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(QVariantMap currentFields READ currentFields NOTIFY fieldChoicesChanged)
    Q_PROPERTY(QVariantList fieldChoices READ fieldChoices NOTIFY fieldChoicesChanged)

public:
    explicit MetadataSearchService(ConfigService *config, DiscogsService *discogs,
                                   QObject *parent = nullptr);

    bool searching() const { return m_searching; }
    QString status() const { return m_status; }
    QVariantList candidates() const { return m_candidates; }
    int selectedIndex() const { return m_selectedIndex; }
    QVariantMap currentFields() const { return m_currentFields; }
    QVariantList fieldChoices() const;

    Q_INVOKABLE void setSelectedIndex(int index);
    Q_INVOKABLE void setFieldChecked(const QString &key, bool checked);
    Q_INVOKABLE QVariantMap checkedFields() const;
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setStatus(const QString &status);
    Q_INVOKABLE void setCurrentFields(const QVariantMap &fields);
    Q_INVOKABLE void searchRelease(const QString &artist, const QString &album,
                                   const QString &albumArtist = {});

signals:
    void searchingChanged();
    void statusChanged();
    void candidatesChanged();
    void selectedIndexChanged();
    void fieldChoicesChanged();
    void searchFinished(bool success);

private:
    void setSearching(bool searching);
    void rebuildFieldChoices();
    void beginReleaseDetailFetch(int index);
    void applyReleaseDetailFields(int index, int fetchId, const QVariantMap &fields);
    static QVariantMap normalizeCandidateFields(const QVariantMap &fields);

    ConfigService *m_config = nullptr;
    DiscogsService *m_discogs = nullptr;
    bool m_searching = false;
    QString m_status;
    QVariantList m_candidates;
    int m_selectedIndex = -1;
    QVariantMap m_currentFields;
    QHash<QString, bool> m_fieldChecked;
    int m_detailFetchId = 0;
};
