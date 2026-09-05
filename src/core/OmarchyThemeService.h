#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QString>

class QFileSystemWatcher;

class OmarchyThemeService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString themeName READ themeName NOTIFY colorsChanged)
    Q_PROPERTY(bool dark READ dark NOTIFY colorsChanged)
    Q_PROPERTY(QString background READ background NOTIFY colorsChanged)
    Q_PROPERTY(QString darkBackground READ darkBackground NOTIFY colorsChanged)
    Q_PROPERTY(QString surface READ surface NOTIFY colorsChanged)
    Q_PROPERTY(QString surfaceHover READ surfaceHover NOTIFY colorsChanged)
    Q_PROPERTY(QString selection READ selection NOTIFY colorsChanged)
    Q_PROPERTY(QString foreground READ foreground NOTIFY colorsChanged)
    Q_PROPERTY(QString muted READ muted NOTIFY colorsChanged)
    Q_PROPERTY(QString accent READ accent NOTIFY colorsChanged)
    Q_PROPERTY(QString border READ border NOTIFY colorsChanged)
    Q_PROPERTY(QString error READ error NOTIFY colorsChanged)
    Q_PROPERTY(QString success READ success NOTIFY colorsChanged)
    Q_PROPERTY(QString warning READ warning NOTIFY colorsChanged)
    Q_PROPERTY(QString magenta READ magenta NOTIFY colorsChanged)
    Q_PROPERTY(int radiusSm READ radiusSm CONSTANT)
    Q_PROPERTY(int radiusMd READ radiusMd CONSTANT)
    Q_PROPERTY(int radiusLg READ radiusLg CONSTANT)
    Q_PROPERTY(int spaceXs READ spaceXs CONSTANT)
    Q_PROPERTY(int spaceSm READ spaceSm CONSTANT)
    Q_PROPERTY(int spaceMd READ spaceMd CONSTANT)
    Q_PROPERTY(int spaceLg READ spaceLg CONSTANT)
    Q_PROPERTY(int spaceXl READ spaceXl CONSTANT)
    Q_PROPERTY(int fontCaption READ fontCaption CONSTANT)
    Q_PROPERTY(int fontSmall READ fontSmall CONSTANT)
    Q_PROPERTY(int fontBody READ fontBody CONSTANT)
    Q_PROPERTY(int fontSubtitle READ fontSubtitle CONSTANT)
    Q_PROPERTY(int fontHeading READ fontHeading CONSTANT)
    Q_PROPERTY(int fontTitle READ fontTitle CONSTANT)
    Q_PROPERTY(int fontDisplay READ fontDisplay CONSTANT)

public:
    explicit OmarchyThemeService(QObject *parent = nullptr);

    QString themeName() const { return m_themeName; }
    bool dark() const { return m_dark; }
    QString background() const { return m_colors.value(QStringLiteral("background")); }
    QString darkBackground() const { return m_colors.value(QStringLiteral("dark_background")); }
    QString surface() const { return m_colors.value(QStringLiteral("lighter_background")); }
    QString surfaceHover() const { return m_colors.value(QStringLiteral("selection")); }
    QString selection() const { return m_colors.value(QStringLiteral("selection")); }
    QString foreground() const { return m_colors.value(QStringLiteral("foreground")); }
    QString muted() const { return m_colors.value(QStringLiteral("muted")); }
    QString accent() const { return m_colors.value(QStringLiteral("accent")); }
    QString border() const { return m_colors.value(QStringLiteral("muted")); }
    QString error() const { return m_colors.value(QStringLiteral("red")); }
    QString success() const { return m_colors.value(QStringLiteral("green")); }
    QString warning() const { return m_colors.value(QStringLiteral("yellow")); }
    QString magenta() const { return m_colors.value(QStringLiteral("magenta")); }
    int radiusSm() const { return 6; }
    int radiusMd() const { return 10; }
    int radiusLg() const { return 14; }
    int spaceXs() const { return 4; }
    int spaceSm() const { return 8; }
    int spaceMd() const { return 12; }
    int spaceLg() const { return 16; }
    int spaceXl() const { return 24; }
    int fontCaption() const { return 11; }
    int fontSmall() const { return 12; }
    int fontBody() const { return 13; }
    int fontSubtitle() const { return 15; }
    int fontHeading() const { return 17; }
    int fontTitle() const { return 20; }
    int fontDisplay() const { return 22; }

    Q_INVOKABLE QString rgba(const QString &hexColor, qreal alpha) const;
    Q_INVOKABLE QString iconUrl(const QString &name, int size, const QString &color = {}) const;
    Q_INVOKABLE void reload();

signals:
    void colorsChanged();

private:
    QString resolveColorsPath() const;
    void loadColors();
    void applyApplicationPalette();
    static QHash<QString, QString> parseColorsToml(const QString &path);
    static QHash<QString, QString> defaultPalette();
    static QString readThemeSlug();

    QFileSystemWatcher *m_watcher = nullptr;
    QString m_themeName;
    bool m_dark = true;
    QHash<QString, QString> m_colors;
    QString m_colorsPath;
};
