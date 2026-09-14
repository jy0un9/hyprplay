#pragma once

#include <QMutex>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QWaitCondition>

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
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY albumsChanged)
    Q_PROPERTY(QString destinationRoot READ destinationRoot NOTIFY albumsChanged)
    Q_PROPERTY(bool awaitingDecision READ awaitingDecision NOTIFY decisionChanged)
    Q_PROPERTY(QString decisionTitle READ decisionTitle NOTIFY decisionChanged)
    Q_PROPERTY(QString decisionMessage READ decisionMessage NOTIFY decisionChanged)
    Q_PROPERTY(QString decisionDetail READ decisionDetail NOTIFY decisionChanged)
    Q_PROPERTY(int tracksDone READ tracksDone NOTIFY progressChanged)
    Q_PROPERTY(int tracksTotal READ tracksTotal NOTIFY progressChanged)

public:
    explicit ImportService(ConfigService *config, LibraryService *library, TagService *tags,
                           BeetsService *beets, QObject *parent = nullptr);

    bool importing() const { return m_importing; }
    QString status() const { return m_status; }
    int progress() const { return m_progress; }
    QVariantList albums() const { return m_albums; }
    int selectedCount() const;
    QString destinationRoot() const;
    bool awaitingDecision() const { return m_awaitingDecision; }
    QString decisionTitle() const { return m_decisionTitle; }
    QString decisionMessage() const { return m_decisionMessage; }
    QString decisionDetail() const { return m_decisionDetail; }
    int tracksDone() const { return m_tracksDone; }
    int tracksTotal() const { return m_tracksTotal; }

    Q_INVOKABLE void scanInbox();
    Q_INVOKABLE void setAlbumSelected(int index, bool selected);
    Q_INVOKABLE void setAllAlbumsSelected(bool selected);
    Q_INVOKABLE void startImport(bool runBeets = true);
    Q_INVOKABLE void cancelImport();
    Q_INVOKABLE void resolveImportDecision(const QString &action);

signals:
    void importingChanged();
    void statusChanged();
    void progressChanged();
    void albumsChanged();
    void importFinished(bool success);
    void decisionChanged();
    void decisionRequired(const QString &title, const QString &message, const QString &detailPath);

private:
    enum class Decision { None, Retry, SkipTrack, SkipAlbum, Abort };

    void setImporting(bool importing);
    void setStatus(const QString &status);
    void setProgress(int progress);
    void setTrackProgress(int done, int total);
    void clearDecisionState();
    Decision waitForDecision(const QString &title, const QString &message, const QString &detail);
    QString musicLibraryRoot() const;

    ConfigService *m_config = nullptr;
    LibraryService *m_library = nullptr;
    TagService *m_tags = nullptr;
    BeetsService *m_beets = nullptr;
    bool m_importing = false;
    bool m_cancelRequested = false;
    int m_progress = 0;
    int m_tracksDone = 0;
    int m_tracksTotal = 0;
    QString m_status;
    QVariantList m_albums;

    QMutex m_decisionMutex;
    QWaitCondition m_decisionCond;
    Decision m_decision = Decision::None;
    bool m_awaitingDecision = false;
    QString m_decisionTitle;
    QString m_decisionMessage;
    QString m_decisionDetail;
};
