#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

struct PlaybackConfig {
    int volume = 80;
    int seekStepSecs = 5;
    int lyricsOffsetMs = 0;
};

struct EqualizerConfig {
    bool enabled = true;
    QString preset = QStringLiteral("Flat");
    QList<double> gains = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
};

class ConfigService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList libraryPaths READ libraryPaths NOTIFY configChanged)
    Q_PROPERTY(QString playlistsDir READ playlistsDir NOTIFY configChanged)
    Q_PROPERTY(QString importInbox READ importInbox WRITE setImportInbox NOTIFY configChanged)
    Q_PROPERTY(QString beetsBinary READ beetsBinary WRITE setBeetsBinary NOTIFY configChanged)
    Q_PROPERTY(bool beetsNomove READ beetsNomove WRITE setBeetsNomove NOTIFY configChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY configChanged)
    Q_PROPERTY(int lyricsOffsetMs READ lyricsOffsetMs WRITE setLyricsOffsetMs NOTIFY configChanged)
    Q_PROPERTY(QString lyricsDir READ lyricsDir NOTIFY configChanged)
    Q_PROPERTY(QString configPath READ configPath CONSTANT)
    Q_PROPERTY(QString uiFontFamily READ uiFontFamily WRITE setUiFontFamily NOTIFY configChanged)

    Q_PROPERTY(int layoutSidebarWidth READ layoutSidebarWidth WRITE setLayoutSidebarWidth NOTIFY layoutChanged)
    Q_PROPERTY(int layoutSidePanelWidth READ layoutSidePanelWidth WRITE setLayoutSidePanelWidth NOTIFY layoutChanged)
    Q_PROPERTY(int layoutNowPlayingHeight READ layoutNowPlayingHeight WRITE setLayoutNowPlayingHeight NOTIFY layoutChanged)
    Q_PROPERTY(int layoutLibraryArtistsWidth READ layoutLibraryArtistsWidth WRITE setLayoutLibraryArtistsWidth NOTIFY layoutChanged)
    Q_PROPERTY(int layoutLibraryAlbumsWidth READ layoutLibraryAlbumsWidth WRITE setLayoutLibraryAlbumsWidth NOTIFY layoutChanged)
    Q_PROPERTY(int layoutPlaylistsListWidth READ layoutPlaylistsListWidth WRITE setLayoutPlaylistsListWidth NOTIFY layoutChanged)
    Q_PROPERTY(bool layoutSidebarCollapsed READ layoutSidebarCollapsed WRITE setLayoutSidebarCollapsed NOTIFY layoutChanged)

public:
    explicit ConfigService(QObject *parent = nullptr);

    QStringList libraryPaths() const { return m_libraryPaths; }
    QString playlistsDir() const { return m_playlistsDir; }
    QString importInbox() const { return m_importInbox; }
    QString importInboxPath() const;
    QString beetsBinary() const { return m_beetsBinary; }
    bool beetsNomove() const { return m_beetsNomove; }
    int volume() const { return m_playback.volume; }
    int seekStepSecs() const { return m_playback.seekStepSecs; }
    int lyricsOffsetMs() const { return m_playback.lyricsOffsetMs; }
    QString lyricsDir() const;
    QString configPath() const;
    QString uiFontFamily() const { return m_uiFontFamily; }

    int layoutSidebarWidth() const { return m_layoutSidebarWidth; }
    int layoutSidePanelWidth() const { return m_layoutSidePanelWidth; }
    int layoutNowPlayingHeight() const { return m_layoutNowPlayingHeight; }
    int layoutLibraryArtistsWidth() const { return m_layoutLibraryArtistsWidth; }
    int layoutLibraryAlbumsWidth() const { return m_layoutLibraryAlbumsWidth; }
    int layoutPlaylistsListWidth() const { return m_layoutPlaylistsListWidth; }
    bool layoutSidebarCollapsed() const { return m_layoutSidebarCollapsed; }

    Q_INVOKABLE void load();
    Q_INVOKABLE void save();
    Q_INVOKABLE void saveOnExit();
    Q_INVOKABLE void setVolume(int volume);
    Q_INVOKABLE void setLyricsOffsetMs(int offsetMs);
    Q_INVOKABLE void setImportInbox(const QString &path);
    Q_INVOKABLE void setBeetsBinary(const QString &binary);
    Q_INVOKABLE void setBeetsNomove(bool nomove);
    Q_INVOKABLE void setUiFontFamily(const QString &family);
    Q_INVOKABLE void setLayoutSidebarWidth(int width);
    Q_INVOKABLE void setLayoutSidePanelWidth(int width);
    Q_INVOKABLE void setLayoutNowPlayingHeight(int height);
    Q_INVOKABLE void setLayoutLibraryArtistsWidth(int width);
    Q_INVOKABLE void setLayoutLibraryAlbumsWidth(int width);
    Q_INVOKABLE void setLayoutPlaylistsListWidth(int width);
    Q_INVOKABLE void setLayoutSidebarCollapsed(bool collapsed);
    Q_INVOKABLE QVariant layoutSplitState(const QString &name) const;
    Q_INVOKABLE void setLayoutSplitState(const QString &name, const QVariant &state);
    Q_INVOKABLE QString expandPath(const QString &path) const;

signals:
    void configChanged();
    void layoutChanged();

private:
    void ensureDefaults();
    QString configFilePath() const;
    void scheduleLayoutSave();
    void setLayoutValue(int &field, int value, int min, int max);

    QStringList m_libraryPaths;
    QString m_playlistsDir;
    QString m_lyricsDir;
    QString m_importInbox;
    PlaybackConfig m_playback;
    EqualizerConfig m_equalizer;
    QString m_beetsBinary = QStringLiteral("beet");
    bool m_beetsNomove = true;
    QString m_uiFontFamily = QStringLiteral("JetBrainsMono Nerd Font");

    int m_layoutSidebarWidth = 208;
    int m_layoutSidePanelWidth = 300;
    int m_layoutNowPlayingHeight = 96;
    int m_layoutLibraryArtistsWidth = 220;
    int m_layoutLibraryAlbumsWidth = 240;
    int m_layoutPlaylistsListWidth = 260;
    bool m_layoutSidebarCollapsed = false;

    QVariantMap m_layoutSplitStates;

    class QTimer *m_layoutSaveTimer = nullptr;
};
