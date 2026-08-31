#include "ThemeIconProvider.h"

#include <QDir>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QProcess>

namespace {

QString readGsettingsIconTheme() {
    QProcess process;
    process.start(QStringLiteral("gsettings"),
                  {QStringLiteral("get"), QStringLiteral("org.gnome.desktop.interface"),
                   QStringLiteral("icon-theme")});
    if (!process.waitForFinished(1500) || process.exitCode() != 0) {
        return {};
    }

    QString value = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    if (value.startsWith('\'')) {
        value = value.mid(1);
    }
    if (value.endsWith('\'')) {
        value.chop(1);
    }
    return value.trimmed();
}

QString firstExistingTheme(const QStringList &candidates) {
    for (const QString &theme : candidates) {
        if (theme.isEmpty()) {
            continue;
        }
        if (QFile::exists(QStringLiteral("/usr/share/icons/") + theme)
            || QFile::exists(QDir::homePath() + QStringLiteral("/.icons/") + theme)
            || QFile::exists(QDir::homePath() + QStringLiteral("/.local/share/icons/") + theme)) {
            return theme;
        }
    }
    return {};
}

QPixmap tintPixmap(const QPixmap &source, const QColor &color) {
    if (source.isNull()) {
        return source;
    }

    QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < image.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = line[x];
            const int alpha = qAlpha(pixel);
            if (alpha == 0) {
                continue;
            }
            line[x] = qRgba(color.red(), color.green(), color.blue(), alpha);
        }
    }
    return QPixmap::fromImage(image);
}

} // namespace

ThemeIconProvider::ThemeIconProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

QPixmap ThemeIconProvider::requestPixmap(const QString &id, QSize *size,
                                         const QSize &requestedSize) {
    QString iconName = id;
    int pixelSize = 24;
    const int queryIndex = iconName.indexOf(QLatin1Char('?'));
    if (queryIndex >= 0) {
        bool ok = false;
        const int parsed = iconName.mid(queryIndex + 1).toInt(&ok);
        if (ok && parsed > 0) {
            pixelSize = parsed;
        }
        iconName = iconName.left(queryIndex);
    }
    if (requestedSize.width() > 0) {
        pixelSize = qMax(pixelSize, requestedSize.width());
    }
    if (requestedSize.height() > 0) {
        pixelSize = qMax(pixelSize, requestedSize.height());
    }

    QIcon icon = QIcon::fromTheme(iconName);
    if (icon.isNull() && iconName.endsWith(QStringLiteral("-symbolic"))) {
        icon = QIcon::fromTheme(iconName.chopped(9));
    }

    QPixmap pixmap;
    if (!icon.isNull()) {
        pixmap = icon.pixmap(QSize(pixelSize, pixelSize));
        pixmap = tintPixmap(pixmap, QColor(235, 235, 235));
    }

    if (pixmap.isNull()) {
        pixmap = QPixmap(pixelSize, pixelSize);
        pixmap.fill(Qt::transparent);
    }

    if (size) {
        *size = pixmap.size();
    }
    return pixmap;
}

void setupSystemIconTheme() {
    QString theme = qEnvironmentVariable("GTK_ICON_THEME");
    if (theme.isEmpty()) {
        theme = qEnvironmentVariable("XDG_ICON_THEME");
    }
    if (theme.isEmpty()) {
        theme = readGsettingsIconTheme();
    }
    if (theme.isEmpty()) {
        theme = firstExistingTheme({QStringLiteral("Yaru-magenta"), QStringLiteral("Yaru"),
                                    QStringLiteral("Papirus-Dark"), QStringLiteral("Papirus"),
                                    QStringLiteral("Adwaita")});
    }
    if (theme.isEmpty()) {
        theme = QStringLiteral("Adwaita");
    }

    QIcon::setThemeName(theme);
    QIcon::setFallbackThemeName(QStringLiteral("Adwaita"));
    if (qEnvironmentVariableIsEmpty("GTK_ICON_THEME")) {
        qputenv("GTK_ICON_THEME", theme.toLocal8Bit());
    }
}
