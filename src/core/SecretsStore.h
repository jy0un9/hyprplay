#pragma once

#include <QString>

// Token storage for Discogs / Genius.
// Preference: FreeDesktop Secret Service (libsecret) when available; otherwise
// ~/.config/qt-music/secrets.toml with mode 0600. Existing plaintext tokens are
// migrated into the keyring on first successful load when a keyring is present.
class SecretsStore {
public:
    enum class Key { DiscogsToken, GeniusToken };

    static QString filePath();
    static QString load(Key key);
    static bool store(Key key, const QString &value);
    static bool clear(Key key);
    static bool keyringAvailable();
    static QString storageDescription();
    static void hardenFilePermissions();

private:
    static QString sectionName(Key key);
    static QString keyringAttribute(Key key);
    static QString keyringLabel(Key key);
    static QString loadFromFile(Key key);
    static bool writeToFile(Key key, const QString &value);
    static bool clearFromFile(Key key);
    static QString loadFromKeyring(Key key);
    static bool storeInKeyring(Key key, const QString &value);
    static bool clearFromKeyring(Key key);
};
