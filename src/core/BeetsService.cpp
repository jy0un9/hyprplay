#include "BeetsService.h"

#include "ConfigService.h"

#include <QProcess>

namespace {

QString quoteBeetValue(const QString &value) {
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"") + escaped + QLatin1Char('"');
}

QString beetsFieldQuery(const QString &field, const QString &value) {
    return field + QLatin1Char(':') + quoteBeetValue(value);
}

} // namespace

BeetsService::BeetsService(ConfigService *config, QObject *parent)
    : QObject(parent), m_config(config) {}

bool BeetsService::available() const {
    if (m_availableChecked) {
        return m_available;
    }
    m_availableChecked = true;
    if (!m_config) {
        m_available = false;
        return false;
    }
    QProcess process;
    process.start(m_config->beetsBinary(), {QStringLiteral("version")});
    if (!process.waitForFinished(5000)) {
        process.kill();
        m_available = false;
        return false;
    }
    m_available = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    return m_available;
}

QString BeetsService::version() {
    QString output;
    if (!runBeet({QStringLiteral("version")}, &output)) {
        return {};
    }
    return output.trimmed();
}

QStringList BeetsService::globalArgs() const {
    return {QStringLiteral("--disable-plugins"), QStringLiteral("fetchart,embedart")};
}

bool BeetsService::runBeet(const QStringList &args, QString *output) {
    if (!m_config) {
        return false;
    }

    QProcess process;
    process.setProgram(m_config->beetsBinary());
    process.setArguments(globalArgs() + args);
    process.start();
    if (!process.waitForFinished(300000)) {
        process.kill();
        setStatus(QStringLiteral("beet command timed out"));
        return false;
    }

    const QByteArray stdoutBytes = process.readAllStandardOutput();
    const QByteArray stderrBytes = process.readAllStandardError();
    if (output) {
        *output = QString::fromUtf8(stdoutBytes);
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString err = QString::fromUtf8(stderrBytes.trimmed());
        setStatus(err.isEmpty() ? QStringLiteral("beet command failed") : err);
        return false;
    }
    return true;
}

bool BeetsService::importAlbum(const QString &albumDir, bool autotag) {
    if (albumDir.isEmpty()) {
        return false;
    }
    if (!available()) {
        setStatus(QStringLiteral("beet not found on PATH"));
        return false;
    }

    setBusy(true);
    QStringList args;
    args << QStringLiteral("import") << QStringLiteral("-q");
    if (m_config->beetsNomove()) {
        args << QStringLiteral("--nocopy") << QStringLiteral("--nomove");
    }
    if (!autotag) {
        args << QStringLiteral("-A");
    }
    args << albumDir;

    const bool ok = runBeet(args);
    setBusy(false);
    setStatus(ok ? QStringLiteral("Imported into beets: %1").arg(albumDir)
                 : m_status);
    return ok;
}

bool BeetsService::modifyByPath(const QString &path, const QVariantMap &fields) {
    if (path.isEmpty() || fields.isEmpty()) {
        return false;
    }
    if (!available()) {
        setStatus(QStringLiteral("beet not found on PATH"));
        return false;
    }

    QStringList args;
    args << QStringLiteral("modify") << QStringLiteral("-y");
    if (m_config->beetsNomove()) {
        args << QStringLiteral("-M");
    }
    args << QStringLiteral("path:") + path;
    args << modifyFieldArgs(fields);

    setBusy(true);
    const bool ok = runBeet(args);
    setBusy(false);
    setStatus(ok ? QStringLiteral("Updated beets metadata")
                 : m_status);
    return ok;
}

QStringList BeetsService::modifyFieldArgs(const QVariantMap &fields) const {
    QStringList args;
    static const QHash<QString, QString> fieldMap = {
        {QStringLiteral("title"), QStringLiteral("title")},
        {QStringLiteral("artist"), QStringLiteral("artist")},
        {QStringLiteral("album"), QStringLiteral("album")},
        {QStringLiteral("trackNumber"), QStringLiteral("track")},
        {QStringLiteral("year"), QStringLiteral("year")},
        {QStringLiteral("genre"), QStringLiteral("genre")},
        {QStringLiteral("albumArtist"), QStringLiteral("albumartist")},
        {QStringLiteral("mb_albumid"), QStringLiteral("mb_albumid")},
    };

    for (auto it = fieldMap.constBegin(); it != fieldMap.constEnd(); ++it) {
        if (!fields.contains(it.key())) {
            continue;
        }
        const QString value = fields.value(it.key()).toString();
        args << it.value() + QLatin1Char('=') + quoteBeetValue(value);
    }
    return args;
}

bool BeetsService::modifyByQuery(const QString &query, const QVariantMap &fields) {
    if (query.trimmed().isEmpty() || fields.isEmpty()) {
        return false;
    }
    if (!available()) {
        setStatus(QStringLiteral("beet not found on PATH"));
        return false;
    }

    QStringList args;
    args << QStringLiteral("modify") << QStringLiteral("-y");
    if (m_config->beetsNomove()) {
        args << QStringLiteral("-M");
    }
    args << query.trimmed();
    args << modifyFieldArgs(fields);

    setBusy(true);
    const bool ok = runBeet(args);
    setBusy(false);
    setStatus(ok ? QStringLiteral("Updated beets: %1").arg(query.trimmed())
                 : m_status);
    return ok;
}

bool BeetsService::modifyPaths(const QStringList &paths, const QVariantMap &fields) {
    if (paths.isEmpty() || fields.isEmpty()) {
        return false;
    }
    if (!available()) {
        setStatus(QStringLiteral("beet not found on PATH"));
        return false;
    }

    setBusy(true);
    int updated = 0;
    QString lastError = m_status;
    for (const QString &path : paths) {
        QStringList args;
        args << QStringLiteral("modify") << QStringLiteral("-y");
        if (m_config->beetsNomove()) {
            args << QStringLiteral("-M");
        }
        args << QStringLiteral("path:") + path;
        args << modifyFieldArgs(fields);
        if (runBeet(args)) {
            ++updated;
        } else {
            lastError = m_status;
        }
    }
    setBusy(false);

    if (updated == paths.size()) {
        setStatus(QStringLiteral("Updated beets for %1 track(s)").arg(updated));
        return true;
    }
    setStatus(lastError.isEmpty()
                  ? QStringLiteral("Updated beets for %1/%2 track(s)").arg(updated).arg(paths.size())
                  : lastError);
    return updated > 0;
}

bool BeetsService::syncAlbumTags(const QString &libraryArtist, const QString &libraryAlbum,
                                 const QVariantMap &fields, const QStringList &fallbackPaths) {
    if (libraryArtist.isEmpty() || libraryAlbum.isEmpty()) {
        return false;
    }
    const QString query = beetsFieldQuery(QStringLiteral("artist"), libraryArtist) + QLatin1Char(' ')
                          + beetsFieldQuery(QStringLiteral("album"), libraryAlbum);
    if (modifyByQuery(query, fields)) {
        return true;
    }
    return modifyPaths(fallbackPaths, fields);
}

bool BeetsService::syncArtistTags(const QString &libraryArtist, const QVariantMap &fields,
                                  const QStringList &fallbackPaths) {
    if (libraryArtist.isEmpty()) {
        return false;
    }
    const QString query = beetsFieldQuery(QStringLiteral("artist"), libraryArtist);
    if (modifyByQuery(query, fields)) {
        return true;
    }
    return modifyPaths(fallbackPaths, fields);
}

void BeetsService::setBusy(bool busy) {
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

void BeetsService::setStatus(const QString &status) {
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}
