#include "OmarchyThemeService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QTextStream>

namespace {

QString expandHome(const QString &path) {
    if (path.startsWith(QStringLiteral("~/"))) {
        return QDir::homePath() + path.mid(1);
    }
    return path;
}

QColor parseColor(const QString &value) {
    QColor color(value.trimmed());
    return color.isValid() ? color : QColor(Qt::white);
}

} // namespace

OmarchyThemeService::OmarchyThemeService(QObject *parent) : QObject(parent) {
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        Q_UNUSED(path);
        reload();
    });
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path) {
        Q_UNUSED(path);
        reload();
    });
    reload();
}

QString OmarchyThemeService::rgba(const QString &hexColor, qreal alpha) const {
    QColor color(hexColor);
    if (!color.isValid()) {
        return hexColor;
    }
    color.setAlphaF(qBound(0.0, alpha, 1.0));
    return color.name(QColor::HexArgb);
}

QString OmarchyThemeService::onFill(const QString &fillHex) const {
    const QColor fill = parseColor(fillHex);
    const qreal luma = 0.299 * fill.redF() + 0.587 * fill.greenF() + 0.114 * fill.blueF();
    return luma > 0.55 ? QStringLiteral("#1a1a1a") : QStringLiteral("#f4f4f4");
}

QString OmarchyThemeService::iconUrl(const QString &name, int size, const QString &color) const {
    QColor tint(color.isEmpty() ? m_colors.value(QStringLiteral("foreground")) : color);
    if (!tint.isValid()) {
        tint = QColor(m_dark ? 235 : 32, m_dark ? 235 : 32, m_dark ? 235 : 32);
    }
    return QStringLiteral("image://themeicon/%1?%2&%3")
        .arg(name)
        .arg(qMax(1, size))
        .arg(tint.name().mid(1));
}

void OmarchyThemeService::reload() {
    loadColors();
    applyApplicationPalette();
    emit colorsChanged();
}

QString OmarchyThemeService::resolveColorsPath() const {
    const QString statePath =
        expandHome(QStringLiteral("~/.local/state/omarchy/current/theme/colors.toml"));
    if (QFile::exists(statePath)) {
        return statePath;
    }

    const QString slug = readThemeSlug();
    if (slug.isEmpty()) {
        return {};
    }

    const QStringList candidates = {
        expandHome(QStringLiteral("~/.config/omarchy/themes/") + slug + QStringLiteral("/colors.toml")),
        QStringLiteral("/usr/share/omarchy/themes/") + slug + QStringLiteral("/colors.toml"),
    };
    for (const QString &candidate : candidates) {
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

QString OmarchyThemeService::readThemeSlug() {
    QFile file(expandHome(QStringLiteral("~/.local/state/omarchy/current/theme.name")));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

QHash<QString, QString> OmarchyThemeService::defaultPalette() {
    QHash<QString, QString> colors;
    colors.insert(QStringLiteral("background"), QStringLiteral("#1a1b26"));
    colors.insert(QStringLiteral("dark_background"), QStringLiteral("#13141c"));
    colors.insert(QStringLiteral("lighter_background"), QStringLiteral("#24283b"));
    colors.insert(QStringLiteral("selection"), QStringLiteral("#292e42"));
    colors.insert(QStringLiteral("muted"), QStringLiteral("#414868"));
    colors.insert(QStringLiteral("foreground"), QStringLiteral("#a9b1d6"));
    colors.insert(QStringLiteral("accent"), QStringLiteral("#7aa2f7"));
    colors.insert(QStringLiteral("red"), QStringLiteral("#f7768e"));
    colors.insert(QStringLiteral("green"), QStringLiteral("#9ece6a"));
    colors.insert(QStringLiteral("yellow"), QStringLiteral("#e0af68"));
    colors.insert(QStringLiteral("magenta"), QStringLiteral("#ad8ee6"));
    return colors;
}

QHash<QString, QString> OmarchyThemeService::parseColorsToml(const QString &path) {
    QHash<QString, QString> colors;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return colors;
    }

    static const QRegularExpression linePattern(
        QStringLiteral("^\\s*([A-Za-z0-9_]+)\\s*=\\s*\"([^\"]*)\"\\s*$"));
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QRegularExpressionMatch match = linePattern.match(stream.readLine());
        if (!match.hasMatch()) {
            continue;
        }
        colors.insert(match.captured(1), match.captured(2));
    }
    return colors;
}

void OmarchyThemeService::loadColors() {
    m_colors = defaultPalette();
    m_dark = true;
    m_themeName = QStringLiteral("Tokyo Night");

    const QString path = resolveColorsPath();
    if (!path.isEmpty()) {
        const QHash<QString, QString> parsed = parseColorsToml(path);
        for (auto it = parsed.constBegin(); it != parsed.constEnd(); ++it) {
            m_colors.insert(it.key(), it.value());
        }
        if (parsed.contains(QStringLiteral("mode"))) {
            m_dark = parsed.value(QStringLiteral("mode")).compare(QStringLiteral("light"),
                                                                    Qt::CaseInsensitive) != 0;
        } else {
            m_dark = parseColor(m_colors.value(QStringLiteral("background"))).lightnessF() < 0.55;
        }
    }

    const QString slug = readThemeSlug();
    if (!slug.isEmpty()) {
        m_themeName = slug;
        m_themeName.replace(QLatin1Char('-'), QLatin1Char(' '));
        if (!m_themeName.isEmpty()) {
            m_themeName[0] = m_themeName.at(0).toUpper();
        }
    }

    if (!path.isEmpty() && m_colorsPath != path) {
        if (!m_colorsPath.isEmpty()) {
            m_watcher->removePath(m_colorsPath);
        }
        m_colorsPath = path;
        m_watcher->addPath(m_colorsPath);
    }
    syncWatchPaths(path);
}

void OmarchyThemeService::syncWatchPaths(const QString &colorsPath) {
    QStringList wanted;
    const QString nameFile = expandHome(QStringLiteral("~/.local/state/omarchy/current/theme.name"));
    wanted << nameFile;
    wanted << QFileInfo(nameFile).absolutePath();
    wanted << expandHome(QStringLiteral("~/.local/state/omarchy/current/theme"));
    if (!colorsPath.isEmpty()) {
        wanted << colorsPath;
        wanted << QFileInfo(colorsPath).absolutePath();
    }
    for (const QString &path : wanted) {
        if (path.isEmpty() || m_watchedPaths.contains(path)) {
            continue;
        }
        if (QFile::exists(path) || QDir(path).exists()) {
            m_watcher->addPath(path);
            m_watchedPaths.insert(path);
        }
    }
}

void OmarchyThemeService::applyApplicationPalette() {
    QPalette palette;
    const QColor window = parseColor(m_colors.value(QStringLiteral("background")));
    const QColor base = parseColor(m_colors.value(QStringLiteral("dark_background")));
    const QColor alt = parseColor(m_colors.value(QStringLiteral("lighter_background")));
    const QColor text = parseColor(m_colors.value(QStringLiteral("foreground")));
    const QColor mutedText = parseColor(m_colors.contains(QStringLiteral("dark_foreground"))
                                            ? m_colors.value(QStringLiteral("dark_foreground"))
                                            : m_colors.value(QStringLiteral("muted")));
    const QColor highlight = parseColor(m_colors.value(QStringLiteral("accent")));
    const QColor mid = parseColor(m_colors.value(QStringLiteral("muted")));

    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, alt);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, alt);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::HighlightedText, parseColor(m_colors.value(QStringLiteral("background"))));
    palette.setColor(QPalette::Mid, mid);
    palette.setColor(QPalette::Dark, mid);
    palette.setColor(QPalette::Light, alt);
    palette.setColor(QPalette::PlaceholderText, mutedText);
    palette.setColor(QPalette::Link, highlight);

    if (auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        app->setPalette(palette);
    }
}
