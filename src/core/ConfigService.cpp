#include "ConfigService.h"
#include "SecretsStore.h"

#include <QMetaType>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

namespace {

QString unquote(const QString &s) {
    QString t = s.trimmed();
    if (t.startsWith('"') && t.endsWith('"')) {
        return t.mid(1, t.length() - 2);
    }
    return t;
}

} // namespace

ConfigService::ConfigService(QObject *parent) : QObject(parent) {
    m_layoutSaveTimer = new QTimer(this);
    m_layoutSaveTimer->setSingleShot(true);
    m_layoutSaveTimer->setInterval(400);
    connect(m_layoutSaveTimer, &QTimer::timeout, this, &ConfigService::save);
    ensureDefaults();
}

QString ConfigService::configPath() const {
    return configFilePath();
}

QString ConfigService::lyricsDir() const {
    if (m_lyricsDir.isEmpty()) {
        return {};
    }
    return expandPath(m_lyricsDir);
}

QString ConfigService::importInboxPath() const {
    if (m_importInbox.isEmpty()) {
        return {};
    }
    return expandPath(m_importInbox);
}

QString ConfigService::configFilePath() const {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/config.toml");
    if (!QFile::exists(path)) {
        const QStringList legacyCandidates = {
            QDir::homePath() + QStringLiteral("/.config/qt-music/qt-music/config.toml"),
            QDir::homePath() + QStringLiteral("/.config/qt-music/config.toml"),
            QDir::homePath() + QStringLiteral("/.config/hyprplay/hyprplay/config.toml"),
        };
        for (const QString &legacy : legacyCandidates) {
            if (QFile::exists(legacy) && QFile::copy(legacy, path)) {
                break;
            }
        }
    }
    return path;
}

QString ConfigService::secretsStorageDescription() const {
    return SecretsStore::storageDescription();
}

QString ConfigService::expandPath(const QString &path) const {
    if (path.startsWith("~/")) {
        return QDir::homePath() + path.mid(1);
    }
    return QDir(path).absolutePath();
}

void ConfigService::ensureDefaults() {
    if (m_libraryPaths.isEmpty()) {
        m_libraryPaths = {QStringLiteral("~/Music")};
    }
    if (m_uiFontFamily.isEmpty()) {
        m_uiFontFamily = QStringLiteral("JetBrainsMono Nerd Font");
    }
    if (m_playlistsDir.isEmpty()) {
        m_playlistsDir = expandPath(m_libraryPaths.first()) + QStringLiteral("/Playlists");
    }
    if (m_lyricsDir.isEmpty()) {
        m_lyricsDir = expandPath(m_libraryPaths.first()) + QStringLiteral("/Lyrics");
    }
}

void ConfigService::load() {
    SecretsStore::hardenFilePermissions();
    // Touch load paths so any existing plaintext tokens migrate into the keyring.
    SecretsStore::load(SecretsStore::Key::DiscogsToken);
    SecretsStore::load(SecretsStore::Key::GeniusToken);

    const QString path = configFilePath();
    if (!QFile::exists(path)) {
        ensureDefaults();
        save();
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ensureDefaults();
        return;
    }

    QString section;
    QStringList pathList;
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        if (line.startsWith('[') && line.endsWith(']')) {
            section = line.mid(1, line.length() - 2);
            continue;
        }

        const int eq = line.indexOf('=');
        if (eq <= 0) {
            continue;
        }
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();

        if (section == QLatin1String("library")) {
            if (key == QLatin1String("paths")) {
                pathList.clear();
                static const QRegularExpression re(R"("[^"]*")");
                auto it = re.globalMatch(value);
                while (it.hasNext()) {
                    pathList << unquote(it.next().captured());
                }
                if (!pathList.isEmpty()) {
                    m_libraryPaths = pathList;
                }
            } else if (key == QLatin1String("scan_on_launch")) {
                m_scanOnLaunch =
                    value != QLatin1String("false") && value != QLatin1String("0");
            } else if (key == QLatin1String("library_watch")) {
                m_libraryWatchEnabled =
                    value != QLatin1String("false") && value != QLatin1String("0");
            } else if (key == QLatin1String("lyrics_dir")) {
                m_lyricsDir = unquote(value);
            } else if (key == QLatin1String("import_inbox")) {
                m_importInbox = unquote(value);
            }
        } else if (section == QLatin1String("import")) {
            if (key == QLatin1String("opus_bitrate_kbps")) {
                m_opusBitrateKbps = qBound(48, value.toInt(), 512);
            } else if (key == QLatin1String("mode")) {
                const QString mode = unquote(value).trimmed().toLower();
                if (mode == QLatin1String("convert_opus") || mode == QLatin1String("copy")) {
                    m_importMode = mode;
                }
            }
        } else if (section == QLatin1String("playlists")) {
            if (key == QLatin1String("dir")) {
                m_playlistsDir = unquote(value);
            }
        } else if (section == QLatin1String("playback")) {
            if (key == QLatin1String("volume")) {
                m_playback.volume = value.toInt();
            } else if (key == QLatin1String("seek_step_secs")) {
                m_playback.seekStepSecs = value.toInt();
            } else if (key == QLatin1String("lyrics_offset_ms")) {
                m_playback.lyricsOffsetMs = value.toInt();
            } else if (key == QLatin1String("dac_passthrough")) {
                m_playback.dacPassthrough =
                    value == QLatin1String("true") || value == QLatin1String("1");
            } else if (key == QLatin1String("audio_device")) {
                m_playback.audioDevice = unquote(value);
            }
        } else if (section == QLatin1String("beets")) {
            if (key == QLatin1String("binary")) {
                m_beetsBinary = unquote(value);
            } else if (key == QLatin1String("nomove")) {
                m_beetsNomove = value != QLatin1String("false") && value != QLatin1String("0");
            }
        } else if (section == QLatin1String("ui")) {
            if (key == QLatin1String("font_family")) {
                m_uiFontFamily = unquote(value);
            } else if (key == QLatin1String("font_size")) {
                m_uiFontSize = qBound(9, value.toInt(), 24);
            } else if (key == QLatin1String("wasd_navigation")) {
                m_wasdNavigation =
                    value == QLatin1String("true") || value == QLatin1String("1");
            } else if (key == QLatin1String("tooltips_enabled")) {
                m_tooltipsEnabled =
                    value != QLatin1String("false") && value != QLatin1String("0");
            }
        } else if (section == QLatin1String("lyrics")) {
            if (key == QLatin1String("fetch_interval_secs")) {
                m_lyrics.fetchIntervalSecs = qBound(1, value.toInt(), 60);
            } else if (key == QLatin1String("negative_cache_days")) {
                m_lyrics.negativeCacheDays = qBound(0, value.toInt(), 365);
            } else if (key == QLatin1String("netease_enabled")) {
                m_lyrics.neteaseEnabled =
                    value != QLatin1String("false") && value != QLatin1String("0");
            } else if (key == QLatin1String("plain_enabled")) {
                m_lyrics.plainEnabled =
                    value != QLatin1String("false") && value != QLatin1String("0");
            } else if (key == QLatin1String("slow_interval_secs")) {
                m_lyrics.slowIntervalSecs = qBound(2, value.toInt(), 120);
            }
        } else if (section == QLatin1String("layout")) {
            if (key.startsWith(QLatin1String("split_"))) {
                const QString splitName = key.mid(6);
                const QString stored = unquote(value);
                if (!splitName.isEmpty() && !stored.isEmpty() && stored != QLatin1String("{}")) {
                    m_layoutSplitStates.insert(splitName, stored);
                }
            } else {
                const int intValue = value.toInt();
                if (key == QLatin1String("side_panel_width")) {
                    m_layoutSidePanelWidth = intValue;
                } else if (key == QLatin1String("now_playing_height")) {
                    m_layoutNowPlayingHeight = intValue;
                } else if (key == QLatin1String("library_artists_width")) {
                    m_layoutLibraryArtistsWidth = intValue;
                } else if (key == QLatin1String("library_albums_width")) {
                    m_layoutLibraryAlbumsWidth = intValue;
                } else if (key == QLatin1String("playlists_list_width")) {
                    m_layoutPlaylistsListWidth = intValue;
                }
                // Ignore legacy sidebar_width / sidebar_collapsed keys.
            }
        }
    }

    ensureDefaults();
    m_layoutSidePanelWidth = qBound(220, m_layoutSidePanelWidth, 480);
    m_layoutNowPlayingHeight = qBound(96, m_layoutNowPlayingHeight, 240);
    m_layoutLibraryArtistsWidth = qBound(140, m_layoutLibraryArtistsWidth, 420);
    m_layoutLibraryAlbumsWidth = qBound(140, m_layoutLibraryAlbumsWidth, 420);
    m_layoutPlaylistsListWidth = qBound(180, m_layoutPlaylistsListWidth, 420);
    emit configChanged();
    emit layoutChanged();
}

void ConfigService::save() {
    ensureDefaults();
    const QString path = configFilePath();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }

    QTextStream out(&file);
    out << "# Hyprplay configuration\n\n";

    out << "[library]\n";
    out << "paths = [";
    for (int i = 0; i < m_libraryPaths.size(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << '"' << m_libraryPaths.at(i) << '"';
    }
    out << "]\n";
    out << "scan_on_launch = " << (m_scanOnLaunch ? "true" : "false") << "\n";
    out << "library_watch = " << (m_libraryWatchEnabled ? "true" : "false") << "\n";
    if (!m_lyricsDir.isEmpty()) {
        out << "lyrics_dir = \"" << m_lyricsDir << "\"\n";
    }
    if (!m_importInbox.isEmpty()) {
        out << "import_inbox = \"" << m_importInbox << "\"\n";
    }
    out << "\n";

    out << "[import]\n";
    out << "mode = \"" << m_importMode << "\"\n";
    out << "opus_bitrate_kbps = " << m_opusBitrateKbps << "\n\n";

    out << "[playlists]\n";
    out << "dir = \"" << m_playlistsDir << "\"\n\n";

    out << "[playback]\n";
    out << "volume = " << m_playback.volume << "\n";
    out << "seek_step_secs = " << m_playback.seekStepSecs << "\n";
    out << "lyrics_offset_ms = " << m_playback.lyricsOffsetMs << "\n";
    out << "dac_passthrough = " << (m_playback.dacPassthrough ? "true" : "false") << "\n";
    if (!m_playback.audioDevice.isEmpty()) {
        out << "audio_device = \"" << m_playback.audioDevice << "\"\n";
    }
    out << "\n";

    out << "[beets]\n";
    out << "binary = \"" << m_beetsBinary << "\"\n";
    out << "nomove = " << (m_beetsNomove ? "true" : "false") << "\n\n";

    out << "[ui]\n";
    out << "font_family = \"" << m_uiFontFamily << "\"\n";
    out << "font_size = " << m_uiFontSize << "\n";
    out << "wasd_navigation = " << (m_wasdNavigation ? "true" : "false") << "\n";
    out << "tooltips_enabled = " << (m_tooltipsEnabled ? "true" : "false") << "\n\n";

    out << "[lyrics]\n";
    out << "fetch_interval_secs = " << m_lyrics.fetchIntervalSecs << "\n";
    out << "negative_cache_days = " << m_lyrics.negativeCacheDays << "\n";
    out << "netease_enabled = " << (m_lyrics.neteaseEnabled ? "true" : "false") << "\n";
    out << "plain_enabled = " << (m_lyrics.plainEnabled ? "true" : "false") << "\n";
    out << "slow_interval_secs = " << m_lyrics.slowIntervalSecs << "\n\n";

    out << "[layout]\n";
    out << "side_panel_width = " << m_layoutSidePanelWidth << "\n";
    out << "now_playing_height = " << m_layoutNowPlayingHeight << "\n";
    out << "library_artists_width = " << m_layoutLibraryArtistsWidth << "\n";
    out << "library_albums_width = " << m_layoutLibraryAlbumsWidth << "\n";
    out << "playlists_list_width = " << m_layoutPlaylistsListWidth << "\n";
    for (auto it = m_layoutSplitStates.constBegin(); it != m_layoutSplitStates.constEnd(); ++it) {
        const QString encoded = it.value().toString();
        if (encoded.isEmpty() || encoded == QLatin1String("{}")) {
            continue;
        }
        out << "split_" << it.key() << " = \"" << encoded << "\"\n";
    }
}

void ConfigService::saveOnExit() {
    m_layoutSaveTimer->stop();
    save();
}

void ConfigService::setScanOnLaunch(bool enabled) {
    if (m_scanOnLaunch == enabled) {
        return;
    }
    m_scanOnLaunch = enabled;
    emit configChanged();
    save();
}

void ConfigService::setLibraryWatchEnabled(bool enabled) {
    if (m_libraryWatchEnabled == enabled) {
        return;
    }
    m_libraryWatchEnabled = enabled;
    emit configChanged();
}

void ConfigService::setLibraryPaths(const QString &paths) {
    QStringList list;
    for (const QString &part : paths.split(QLatin1Char(','))) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            list << trimmed;
        }
    }
    if (list.isEmpty()) {
        return;
    }
    if (m_libraryPaths == list) {
        return;
    }
    m_libraryPaths = list;
    emit configChanged();
}

void ConfigService::setLyricsDir(const QString &path) {
    const QString trimmed = path.trimmed();
    if (m_lyricsDir == trimmed) {
        return;
    }
    m_lyricsDir = trimmed;
    emit configChanged();
}

void ConfigService::setVolume(int volume) {
    volume = qBound(0, volume, 100);
    if (m_playback.volume == volume) {
        return;
    }
    m_playback.volume = volume;
    emit configChanged();
}

void ConfigService::setSeekStepSecs(int secs) {
    secs = qBound(1, secs, 60);
    if (m_playback.seekStepSecs == secs) {
        return;
    }
    m_playback.seekStepSecs = secs;
    emit configChanged();
    save();
}

void ConfigService::setDacPassthrough(bool enabled) {
    if (m_playback.dacPassthrough == enabled) {
        return;
    }
    m_playback.dacPassthrough = enabled;
    emit configChanged();
    save();
}

void ConfigService::setAudioDevice(const QString &name) {
    if (m_playback.audioDevice == name) {
        return;
    }
    m_playback.audioDevice = name;
    emit configChanged();
    save();
}

void ConfigService::setLyricsFetchIntervalSecs(int secs) {
    secs = qBound(1, secs, 60);
    if (m_lyrics.fetchIntervalSecs == secs) {
        return;
    }
    m_lyrics.fetchIntervalSecs = secs;
    emit configChanged();
    save();
}

void ConfigService::setLyricsNegativeCacheDays(int days) {
    days = qBound(0, days, 365);
    if (m_lyrics.negativeCacheDays == days) {
        return;
    }
    m_lyrics.negativeCacheDays = days;
    emit configChanged();
    save();
}

void ConfigService::setLyricsNeteaseEnabled(bool enabled) {
    if (m_lyrics.neteaseEnabled == enabled) {
        return;
    }
    m_lyrics.neteaseEnabled = enabled;
    emit configChanged();
    save();
}

void ConfigService::setLyricsPlainEnabled(bool enabled) {
    if (m_lyrics.plainEnabled == enabled) {
        return;
    }
    m_lyrics.plainEnabled = enabled;
    emit configChanged();
    save();
}

void ConfigService::setLyricsSlowIntervalSecs(int secs) {
    secs = qBound(2, secs, 120);
    if (m_lyrics.slowIntervalSecs == secs) {
        return;
    }
    m_lyrics.slowIntervalSecs = secs;
    emit configChanged();
    save();
}

void ConfigService::setLyricsOffsetMs(int offsetMs) {
    offsetMs = qBound(-30000, offsetMs, 30000);
    if (m_playback.lyricsOffsetMs == offsetMs) {
        return;
    }
    m_playback.lyricsOffsetMs = offsetMs;
    emit configChanged();
    save();
}

void ConfigService::setImportInbox(const QString &path) {
    const QString trimmed = path.trimmed();
    if (m_importInbox == trimmed) {
        return;
    }
    m_importInbox = trimmed;
    emit configChanged();
}

void ConfigService::setImportMode(const QString &mode) {
    const QString normalized = mode.trimmed().toLower();
    const QString resolved = (normalized == QLatin1String("convert_opus"))
                                 ? QStringLiteral("convert_opus")
                                 : QStringLiteral("copy");
    if (m_importMode == resolved) {
        return;
    }
    m_importMode = resolved;
    emit configChanged();
}

void ConfigService::setOpusBitrateKbps(int kbps) {
    kbps = qBound(48, kbps, 512);
    if (m_opusBitrateKbps == kbps) {
        return;
    }
    m_opusBitrateKbps = kbps;
    emit configChanged();
}

void ConfigService::setBeetsBinary(const QString &binary) {
    const QString trimmed = binary.trimmed().isEmpty() ? QStringLiteral("beet") : binary.trimmed();
    if (m_beetsBinary == trimmed) {
        return;
    }
    m_beetsBinary = trimmed;
    emit configChanged();
}

void ConfigService::setBeetsNomove(bool nomove) {
    if (m_beetsNomove == nomove) {
        return;
    }
    m_beetsNomove = nomove;
    emit configChanged();
}

void ConfigService::setUiFontFamily(const QString &family) {
    const QString trimmed = family.trimmed().isEmpty()
                                ? QStringLiteral("JetBrainsMono Nerd Font")
                                : family.trimmed();
    if (m_uiFontFamily == trimmed) {
        return;
    }
    m_uiFontFamily = trimmed;
    emit configChanged();
}

void ConfigService::setUiFontSize(int size) {
    size = qBound(9, size, 24);
    if (m_uiFontSize == size) {
        return;
    }
    m_uiFontSize = size;
    emit configChanged();
}

void ConfigService::setWasdNavigation(bool enabled) {
    if (m_wasdNavigation == enabled) {
        return;
    }
    m_wasdNavigation = enabled;
    emit configChanged();
    save();
}

void ConfigService::setTooltipsEnabled(bool enabled) {
    if (m_tooltipsEnabled == enabled) {
        return;
    }
    m_tooltipsEnabled = enabled;
    emit configChanged();
    save();
}

void ConfigService::scheduleLayoutSave() {
    m_layoutSaveTimer->start();
}

void ConfigService::setLayoutValue(int &field, int value, int min, int max) {
    value = qBound(min, value, max);
    if (field == value) {
        return;
    }
    field = value;
    emit layoutChanged();
    scheduleLayoutSave();
}

void ConfigService::setLayoutSidePanelWidth(int width) {
    setLayoutValue(m_layoutSidePanelWidth, width, 220, 480);
}

void ConfigService::setLayoutNowPlayingHeight(int height) {
    setLayoutValue(m_layoutNowPlayingHeight, height, 96, 240);
}

void ConfigService::setLayoutLibraryArtistsWidth(int width) {
    setLayoutValue(m_layoutLibraryArtistsWidth, width, 140, 420);
}

void ConfigService::setLayoutLibraryAlbumsWidth(int width) {
    setLayoutValue(m_layoutLibraryAlbumsWidth, width, 140, 420);
}

void ConfigService::setLayoutPlaylistsListWidth(int width) {
    setLayoutValue(m_layoutPlaylistsListWidth, width, 180, 420);
}

QVariant ConfigService::layoutSplitState(const QString &name) const {
    const QString encoded = m_layoutSplitStates.value(name).toString();
    if (encoded.isEmpty() || encoded == QLatin1String("{}")) {
        return {};
    }
    const QByteArray bytes = QByteArray::fromBase64(encoded.toLatin1());
    if (bytes.isEmpty()) {
        return {};
    }
    return bytes;
}

void ConfigService::setLayoutSplitState(const QString &name, const QVariant &state) {
    if (name.isEmpty()) {
        return;
    }

    QByteArray bytes;
    if (state.metaType().id() == QMetaType::QByteArray) {
        bytes = state.toByteArray();
    } else if (state.canConvert<QByteArray>()) {
        bytes = state.toByteArray();
    } else {
        return;
    }
    if (bytes.isEmpty()) {
        return;
    }

    const QString encoded = QString::fromLatin1(bytes.toBase64());
    if (m_layoutSplitStates.value(name).toString() == encoded) {
        return;
    }
    m_layoutSplitStates.insert(name, encoded);
    scheduleLayoutSave();
}
