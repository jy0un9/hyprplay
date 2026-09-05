#include "ThemeIconProvider.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QProcess>
#include <QRegularExpression>

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
    QColor tint(235, 235, 235);
    const QStringList segments =
        id.split(QRegularExpression(QStringLiteral("[?&#;]")), Qt::SkipEmptyParts);
    if (!segments.isEmpty()) {
        iconName = segments.first();
        static const QRegularExpression hexRe(
            QStringLiteral("^(?:color=)?(%23)?([0-9a-fA-F]{3,8})$"));
        for (int i = 1; i < segments.size(); ++i) {
            const QString segment = segments.at(i);
            bool ok = false;
            const int parsed = segment.toInt(&ok);
            if (ok && parsed > 0) {
                pixelSize = parsed;
                continue;
            }
            const auto match = hexRe.match(segment);
            if (match.hasMatch()) {
                const QColor candidate(QStringLiteral("#") + match.captured(2));
                if (candidate.isValid()) {
                    tint = candidate;
                }
            }
        }
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
        pixmap = tintPixmap(pixmap, tint);
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
