#pragma once

#include <QMutex>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QWaitCondition>

class ConfigService;
class LibraryService;

class ConvertService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool converting READ converting NOTIFY convertingChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QVariantList albums READ albums NOTIFY albumsChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY albumsChanged)
    Q_PROPERTY(bool awaitingDecision READ awaitingDecision NOTIFY decisionChanged)
    Q_PROPERTY(QString decisionTitle READ decisionTitle NOTIFY decisionChanged)
    Q_PROPERTY(QString decisionMessage READ decisionMessage NOTIFY decisionChanged)
    Q_PROPERTY(QString decisionDetail READ decisionDetail NOTIFY decisionChanged)
    Q_PROPERTY(int tracksDone READ tracksDone NOTIFY progressChanged)
    Q_PROPERTY(int tracksTotal READ tracksTotal NOTIFY progressChanged)

public:
    explicit ConvertService(ConfigService *config, LibraryService *library,
                            QObject *parent = nullptr);

    bool converting() const { return m_converting; }
    QString status() const { return m_status; }
    int progress() const { return m_progress; }
    QVariantList albums() const { return m_albums; }
    int selectedCount() const;
    bool awaitingDecision() const { return m_awaitingDecision; }
    QString decisionTitle() const { return m_decisionTitle; }
    QString decisionMessage() const { return m_decisionMessage; }
    QString decisionDetail() const { return m_decisionDetail; }
    int tracksDone() const { return m_tracksDone; }
    int tracksTotal() const { return m_tracksTotal; }

    Q_INVOKABLE void scanLibrary();
    Q_INVOKABLE void setAlbumSelected(int index, bool selected);
    Q_INVOKABLE void setAllAlbumsSelected(bool selected);
    Q_INVOKABLE void startConvert();
    Q_INVOKABLE void cancelConvert();
    Q_INVOKABLE void resolveConvertDecision(const QString &action);

signals:
    void convertingChanged();
    void statusChanged();
    void progressChanged();
    void albumsChanged();
    void convertFinished(bool success);
    void decisionChanged();
    void decisionRequired(const QString &title, const QString &message, const QString &detailPath);

private:
    enum class Decision { None, Retry, SkipTrack, SkipAlbum, Abort };

    void setConverting(bool converting);
    void setStatus(const QString &status);
    void setProgress(int progress);
    void setTrackProgress(int done, int total);
    void clearDecisionState();
    Decision waitForDecision(const QString &title, const QString &message, const QString &detail);

    ConfigService *m_config = nullptr;
    LibraryService *m_library = nullptr;
    bool m_converting = false;
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
