#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

struct PlaybackConfig {
    int volume = 80;
    int seekStepSecs = 5;
    int lyricsOffsetMs = 0;
    bool dacPassthrough = false;
};

struct LyricsConfig {
    int fetchIntervalSecs = 2;
    int negativeCacheDays = 30;
    bool neteaseEnabled = true;
    bool plainEnabled = false;
    int slowIntervalSecs = 4;
};

struct EqualizerConfig {
    bool enabled = true;
    QString preset = QStringLiteral("Flat");
    QList<double> gains = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
};

class ConfigService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList libraryPaths READ libraryPaths NOTIFY configChanged)
    Q_PROPERTY(bool scanOnLaunch READ scanOnLaunch WRITE setScanOnLaunch NOTIFY configChanged)
    Q_PROPERTY(QString playlistsDir READ playlistsDir NOTIFY configChanged)
    Q_PROPERTY(QString importInbox READ importInbox WRITE setImportInbox NOTIFY configChanged)
    Q_PROPERTY(QString beetsBinary READ beetsBinary WRITE setBeetsBinary NOTIFY configChanged)
    Q_PROPERTY(bool beetsNomove READ beetsNomove WRITE setBeetsNomove NOTIFY configChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY configChanged)
    Q_PROPERTY(int seekStepSecs READ seekStepSecs WRITE setSeekStepSecs NOTIFY configChanged)
    Q_PROPERTY(bool dacPassthrough READ dacPassthrough WRITE setDacPassthrough NOTIFY configChanged)
    Q_PROPERTY(int lyricsOffsetMs READ lyricsOffsetMs WRITE setLyricsOffsetMs NOTIFY configChanged)
    Q_PROPERTY(int lyricsFetchIntervalSecs READ lyricsFetchIntervalSecs WRITE setLyricsFetchIntervalSecs NOTIFY configChanged)
    Q_PROPERTY(int lyricsNegativeCacheDays READ lyricsNegativeCacheDays WRITE setLyricsNegativeCacheDays NOTIFY configChanged)
    Q_PROPERTY(bool lyricsNeteaseEnabled READ lyricsNeteaseEnabled WRITE setLyricsNeteaseEnabled NOTIFY configChanged)
    Q_PROPERTY(bool lyricsPlainEnabled READ lyricsPlainEnabled WRITE setLyricsPlainEnabled NOTIFY configChanged)
    Q_PROPERTY(int lyricsSlowIntervalSecs READ lyricsSlowIntervalSecs WRITE setLyricsSlowIntervalSecs NOTIFY configChanged)
    Q_PROPERTY(QString lyricsDir READ lyricsDir NOTIFY configChanged)
    Q_PROPERTY(QString configPath READ configPath CONSTANT)
    Q_PROPERTY(QString uiFontFamily READ uiFontFamily WRITE setUiFontFamily NOTIFY configChanged)
    Q_PROPERTY(int uiFontSize READ uiFontSize WRITE setUiFontSize NOTIFY configChanged)
    Q_PROPERTY(bool wasdNavigation READ wasdNavigation WRITE setWasdNavigation NOTIFY configChanged)
    Q_PROPERTY(bool tooltipsEnabled READ tooltipsEnabled WRITE setTooltipsEnabled NOTIFY configChanged)

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
    bool scanOnLaunch() const { return m_scanOnLaunch; }
    QString playlistsDir() const { return m_playlistsDir; }
    QString importInbox() const { return m_importInbox; }
    QString importInboxPath() const;
    QString beetsBinary() const { return m_beetsBinary; }
    bool beetsNomove() const { return m_beetsNomove; }
    int volume() const { return m_playback.volume; }
    bool dacPassthrough() const { return m_playback.dacPassthrough; }
    int seekStepSecs() const { return m_playback.seekStepSecs; }
    int lyricsOffsetMs() const { return m_playback.lyricsOffsetMs; }
    int lyricsFetchIntervalSecs() const { return m_lyrics.fetchIntervalSecs; }
    int lyricsNegativeCacheDays() const { return m_lyrics.negativeCacheDays; }
    bool lyricsNeteaseEnabled() const { return m_lyrics.neteaseEnabled; }
    bool lyricsPlainEnabled() const { return m_lyrics.plainEnabled; }
    int lyricsSlowIntervalSecs() const { return m_lyrics.slowIntervalSecs; }
    QString lyricsDir() const;
    QString configPath() const;
    QString uiFontFamily() const { return m_uiFontFamily; }
    int uiFontSize() const { return m_uiFontSize; }
    bool wasdNavigation() const { return m_wasdNavigation; }
    bool tooltipsEnabled() const { return m_tooltipsEnabled; }

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
    Q_INVOKABLE void setScanOnLaunch(bool enabled);
    Q_INVOKABLE void setLibraryPaths(const QString &paths);
    Q_INVOKABLE void setLyricsDir(const QString &path);
    Q_INVOKABLE void setVolume(int volume);
    Q_INVOKABLE void setSeekStepSecs(int secs);
    Q_INVOKABLE void setDacPassthrough(bool enabled);
    Q_INVOKABLE void setLyricsOffsetMs(int offsetMs);
    Q_INVOKABLE void setLyricsFetchIntervalSecs(int secs);
    Q_INVOKABLE void setLyricsNegativeCacheDays(int days);
    Q_INVOKABLE void setLyricsNeteaseEnabled(bool enabled);
    Q_INVOKABLE void setLyricsPlainEnabled(bool enabled);
    Q_INVOKABLE void setLyricsSlowIntervalSecs(int secs);
    Q_INVOKABLE void setImportInbox(const QString &path);
    Q_INVOKABLE void setBeetsBinary(const QString &binary);
    Q_INVOKABLE void setBeetsNomove(bool nomove);
    Q_INVOKABLE void setUiFontFamily(const QString &family);
    Q_INVOKABLE void setUiFontSize(int size);
    Q_INVOKABLE void setWasdNavigation(bool enabled);
    Q_INVOKABLE void setTooltipsEnabled(bool enabled);
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
    bool m_scanOnLaunch = true;
    QString m_playlistsDir;
    QString m_lyricsDir;
    QString m_importInbox;
    PlaybackConfig m_playback;
    LyricsConfig m_lyrics;
    EqualizerConfig m_equalizer;
    QString m_beetsBinary = QStringLiteral("beet");
    bool m_beetsNomove = true;
    QString m_uiFontFamily = QStringLiteral("JetBrainsMono Nerd Font");
    int m_uiFontSize = 13;
    bool m_wasdNavigation = false;
    bool m_tooltipsEnabled = true;

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
