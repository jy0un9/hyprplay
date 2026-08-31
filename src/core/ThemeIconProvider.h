#pragma once

#include <QQuickImageProvider>

class ThemeIconProvider : public QQuickImageProvider {
public:
    ThemeIconProvider();

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
};

void setupSystemIconTheme();
