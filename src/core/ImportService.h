#pragma once

#include <QObject>
#include <QVariantList>

class BeetsService;
class ConfigService;
class LibraryService;
class TagService;

class ImportService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool importing READ importing NOTIFY importingChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QVariantList albums READ albums NOTIFY albumsChanged)

public:
    explicit ImportService(ConfigService *config, LibraryService *library, TagService *tags,
                           BeetsService *beets, QObject *parent = nullptr);

    bool importing() const { return m_importing; }
    QString status() const { return m_status; }
    int progress() const { return m_progress; }
    QVariantList albums() const { return m_albums; }

    Q_INVOKABLE void scanInbox();
    Q_INVOKABLE void startImport(bool runBeets = true);
    Q_INVOKABLE void cancelImport();

signals:
    void importingChanged();
    void statusChanged();
    void progressChanged();
    void albumsChanged();
    void importFinished(bool success);

private:
    void setImporting(bool importing);
    void setStatus(const QString &status);
    void setProgress(int progress);

    ConfigService *m_config = nullptr;
    LibraryService *m_library = nullptr;
    TagService *m_tags = nullptr;
    BeetsService *m_beets = nullptr;
    bool m_importing = false;
    bool m_cancelRequested = false;
    int m_progress = 0;
    QString m_status;
    QVariantList m_albums;
};
