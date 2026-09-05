#pragma once

#include <QObject>
#include <QVariantList>
#include <QVector>

class ConfigService;

class TrackMediaService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString albumArtUrl READ albumArtUrl NOTIFY mediaChanged)
    Q_PROPERTY(QString lyricsText READ lyricsText NOTIFY mediaChanged)
    Q_PROPERTY(QVariantList lyricLines READ lyricLines NOTIFY mediaChanged)
    Q_PROPERTY(QVariantList lyricTimes READ lyricTimes NOTIFY mediaChanged)
    Q_PROPERTY(int currentLyricIndex READ currentLyricIndex NOTIFY currentLyricChanged)
    Q_PROPERTY(QVariantList waveformPeaks READ waveformPeaks NOTIFY mediaChanged)
    Q_PROPERTY(bool waveformReady READ waveformReady NOTIFY mediaChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString qualityLabel READ qualityLabel NOTIFY mediaChanged)

public:
    explicit TrackMediaService(ConfigService *config, QObject *parent = nullptr);

    QString albumArtUrl() const { return m_albumArtUrl; }
    QString lyricsText() const { return m_lyricsText; }
    QVariantList lyricLines() const { return m_lyricLines; }
    QVariantList lyricTimes() const { return m_lyricTimes; }
    int currentLyricIndex() const { return m_currentLyricIndex; }
    QVariantList waveformPeaks() const { return m_waveformPeaks; }
    bool waveformReady() const { return m_waveformReady; }
    bool loading() const { return m_loading; }
    QString qualityLabel() const { return m_qualityLabel; }

    Q_INVOKABLE void loadForTrack(const QString &path, const QString &artist = {},
                                  const QString &album = {});
    Q_INVOKABLE QString albumArtForTrack(const QString &path);
    Q_INVOKABLE void updateLyricPosition(double positionSecs);
    Q_INVOKABLE int lyricIndexForPosition(double positionSecs) const;

signals:
    void mediaChanged();
    void currentLyricChanged();
    void loadingChanged();

private:
    struct TimedLine {
        double time = -1.0;
        QString text;
    };

    void clearMedia();
    void loadAlbumArt(const QString &path, const QString &album);
    void loadLyrics(const QString &path, const QString &artist, const QString &album);
    QString findLyricsFile(const QString &path, const QString &artist,
                           const QString &album) const;
    static QVector<TimedLine> parseLyricsFile(const QString &filePath);
    int computeLyricIndex(double positionSecs) const;
    static QString findFolderArt(const QString &trackPath);
    static QVector<quint8> generatePeaksWithFfmpeg(const QString &path);
    static QVector<quint8> loadPeakCache(const QString &cacheFile);
    static bool savePeakCache(const QString &cacheFile, const QVector<quint8> &peaks);
    static QString peakCachePath(const QString &trackPath);
    static QString qualityLabelForPath(const QString &path);

    ConfigService *m_config = nullptr;
    QString m_albumArtUrl;
    QString m_lyricsText;
    QVariantList m_lyricLines;
    QVariantList m_lyricTimes;
    QVector<TimedLine> m_timedLines;
    int m_currentLyricIndex = -1;
    double m_lastLyricSyncPos = -1.0;
    QVariantList m_waveformPeaks;
    bool m_waveformReady = false;
    bool m_loading = false;
    QString m_qualityLabel;
    QString m_currentPath;
    quint64 m_loadGeneration = 0;
};
