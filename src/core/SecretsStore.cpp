#include "SecretsStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

// gio headers declare a field named `signals`; Qt macros that name.
#undef signals
#include <libsecret/secret.h>

namespace {

constexpr auto kSchemaName = "ai.commandcode.qt-music.Token";

const SecretSchema *tokenSchema() {
    static SecretSchema schema = {};
    static bool ready = false;
    if (!ready) {
        schema.name = kSchemaName;
        schema.flags = SECRET_SCHEMA_NONE;
        schema.attributes[0] = {"key", SECRET_SCHEMA_ATTRIBUTE_STRING};
        schema.attributes[1] = {nullptr, static_cast<SecretSchemaAttributeType>(0)};
        ready = true;
    }
    return &schema;
}

QString readFileUtf8(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

bool writeFileUtf8(const QString &path, const QString &content) {
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    if (file.write(content.toUtf8()) < 0) {
        return false;
    }
    file.close();
    SecretsStore::hardenFilePermissions();
    return true;
}

QString sectionTokenRegex(const QString &section) {
    return QStringLiteral("\\[%1\\][^\\[]*?token\\s*=\\s*\"([^\"]*)\"").arg(section);
}

QString upsertSectionToken(QString content, const QString &section, const QString &token) {
    const QRegularExpression sectionRe(
        QStringLiteral("(\\[%1\\][^\\[]*?token\\s*=\\s*\")[^\"]*(\")").arg(section));
    if (sectionRe.match(content).hasMatch()) {
        content.replace(sectionRe, QStringLiteral("\\1%1\\2").arg(token));
        return content;
    }

    const QRegularExpression headerRe(QStringLiteral("\\[%1\\]").arg(section));
    if (headerRe.match(content).hasMatch()) {
        content.replace(headerRe, QStringLiteral("[%1]\ntoken = \"%2\"").arg(section, token));
        return content;
    }

    if (!content.isEmpty() && !content.endsWith(QLatin1Char('\n'))) {
        content.append(QLatin1Char('\n'));
    }
    content.append(QStringLiteral("[%1]\ntoken = \"%2\"\n").arg(section, token));
    return content;
}

QString clearSectionToken(QString content, const QString &section) {
    const QRegularExpression tokenLineRe(
        QStringLiteral("(\\[%1\\][^\\[]*?)\\n?token\\s*=\\s*\"[^\"]*\"\\n?").arg(section));
    if (tokenLineRe.match(content).hasMatch()) {
        content.replace(tokenLineRe, QStringLiteral("\\1\n"));
    }
    // Drop empty section headers left behind.
    const QRegularExpression emptySection(
        QStringLiteral("\\[%1\\]\\s*(?=\\[|\\z)").arg(section));
    content.remove(emptySection);
    return content.trimmed().isEmpty() ? QString() : content;
}

} // namespace

QString SecretsStore::filePath() {
    const QString appDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(appDir);
    return appDir + QStringLiteral("/secrets.toml");
}

QString SecretsStore::sectionName(Key key) {
    switch (key) {
    case Key::DiscogsToken:
        return QStringLiteral("discogs");
    case Key::GeniusToken:
        return QStringLiteral("genius");
    }
    return {};
}

QString SecretsStore::keyringAttribute(Key key) {
    switch (key) {
    case Key::DiscogsToken:
        return QStringLiteral("discogs_token");
    case Key::GeniusToken:
        return QStringLiteral("genius_token");
    }
    return {};
}

QString SecretsStore::keyringLabel(Key key) {
    switch (key) {
    case Key::DiscogsToken:
        return QStringLiteral("qt-music Discogs token");
    case Key::GeniusToken:
        return QStringLiteral("qt-music Genius token");
    }
    return QStringLiteral("qt-music token");
}

void SecretsStore::hardenFilePermissions() {
    const QString path = filePath();
    if (!QFile::exists(path)) {
        return;
    }
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

QString SecretsStore::loadFromFile(Key key) {
    const QString path = filePath();
    if (!QFile::exists(path)) {
        return {};
    }
    hardenFilePermissions();
    const QString content = readFileUtf8(path);
    if (content.isEmpty()) {
        return {};
    }
    const QRegularExpression re(sectionTokenRegex(sectionName(key)));
    const QRegularExpressionMatch match = re.match(content);
    if (!match.hasMatch()) {
        return {};
    }
    return match.captured(1).trimmed();
}

bool SecretsStore::writeToFile(Key key, const QString &value) {
    const QString path = filePath();
    QString content = readFileUtf8(path);
    content = upsertSectionToken(content, sectionName(key), value);
    return writeFileUtf8(path, content);
}

bool SecretsStore::clearFromFile(Key key) {
    const QString path = filePath();
    if (!QFile::exists(path)) {
        return true;
    }
    QString content = readFileUtf8(path);
    content = clearSectionToken(content, sectionName(key));
    if (content.trimmed().isEmpty()) {
        return QFile::remove(path) || !QFile::exists(path);
    }
    return writeFileUtf8(path, content.endsWith(QLatin1Char('\n')) ? content
                                                                  : content + QLatin1Char('\n'));
}

bool SecretsStore::keyringAvailable() {
    GError *error = nullptr;
    SecretService *service =
        secret_service_get_sync(SECRET_SERVICE_NONE, nullptr, &error);
    if (error) {
        g_error_free(error);
        return false;
    }
    if (!service) {
        return false;
    }
    g_object_unref(service);
    return true;
}

QString SecretsStore::loadFromKeyring(Key key) {
    GError *error = nullptr;
    gchar *password = secret_password_lookup_sync(
        tokenSchema(), nullptr, &error, "key",
        keyringAttribute(key).toUtf8().constData(), nullptr);
    if (error) {
        g_error_free(error);
        return {};
    }
    if (!password) {
        return {};
    }
    const QString value = QString::fromUtf8(password).trimmed();
    secret_password_free(password);
    return value;
}

bool SecretsStore::storeInKeyring(Key key, const QString &value) {
    GError *error = nullptr;
    const bool ok = secret_password_store_sync(
        tokenSchema(), SECRET_COLLECTION_DEFAULT,
        keyringLabel(key).toUtf8().constData(), value.toUtf8().constData(), nullptr, &error,
        "key", keyringAttribute(key).toUtf8().constData(), nullptr);
    if (error) {
        g_error_free(error);
        return false;
    }
    return ok;
}

bool SecretsStore::clearFromKeyring(Key key) {
    GError *error = nullptr;
    const bool ok = secret_password_clear_sync(
        tokenSchema(), nullptr, &error, "key",
        keyringAttribute(key).toUtf8().constData(), nullptr);
    if (error) {
        g_error_free(error);
        // Treat "not found" as success.
        return true;
    }
    return ok;
}

QString SecretsStore::load(Key key) {
    if (const QString fromKeyring = loadFromKeyring(key); !fromKeyring.isEmpty()) {
        return fromKeyring;
    }

    const QString fromFile = loadFromFile(key);
    if (fromFile.isEmpty()) {
        return {};
    }

    // Prefer keyring when available: migrate off plaintext and clear the file entry.
    if (storeInKeyring(key, fromFile)) {
        clearFromFile(key);
        return fromFile;
    }

    return fromFile;
}

bool SecretsStore::store(Key key, const QString &value) {
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return clear(key);
    }

    if (storeInKeyring(key, trimmed)) {
        clearFromFile(key);
        return true;
    }

    return writeToFile(key, trimmed);
}

bool SecretsStore::clear(Key key) {
    const bool keyringOk = clearFromKeyring(key);
    const bool fileOk = clearFromFile(key);
    return keyringOk || fileOk;
}

QString SecretsStore::storageDescription() {
    if (keyringAvailable()) {
        return QStringLiteral(
            "Tokens are stored in the system keyring (Secret Service). "
            "Fallback file secrets.toml uses mode 0600.");
    }
    return QStringLiteral(
        "Tokens are stored in ~/.config/qt-music/secrets.toml with mode 0600 "
        "(system keyring unavailable).");
}
