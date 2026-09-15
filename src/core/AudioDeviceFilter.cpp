#include "AudioDeviceFilter.h"

#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QThread>

namespace {

bool isBackendDefault(const QString &name) {
    return !name.contains(QLatin1Char('/'))
           || name == QLatin1String("auto")
           || name.endsWith(QLatin1String("/"));
}

QString backendOf(const QString &name) {
    const int slash = name.indexOf(QLatin1Char('/'));
    return slash < 0 ? name : name.left(slash);
}

QString sinkIdOf(const QString &name) {
    const int slash = name.indexOf(QLatin1Char('/'));
    return slash < 0 ? QString() : name.mid(slash + 1);
}

bool isPipewireOrPulseSink(const QString &name) {
    const QString backend = backendOf(name);
    return (backend == QLatin1String("pipewire") || backend == QLatin1String("pulse"))
           && !sinkIdOf(name).isEmpty();
}

bool looksBuiltInDriver(const QString &driver, const QString &id) {
    const QString d = driver.toLower();
    const QString i = id.toLower();
    return d.contains(QLatin1String("hda"))
           || d.contains(QLatin1String("sof-"))
           || d.contains(QLatin1String("sof_"))
           || i.contains(QLatin1String("sofhd"))
           || i.contains(QLatin1String("pch"))
           || i.contains(QLatin1String("hdmi"));
}

QString shortenDescription(const QString &description) {
    QString out = description;
    static const QStringList suffixes = {
        QStringLiteral(" Analog Stereo"),
        QStringLiteral(" Digital Stereo"),
        QStringLiteral(" Analog Surround 5.1"),
        QStringLiteral(" Analog Surround 7.1"),
    };
    for (const QString &suffix : suffixes) {
        if (out.endsWith(suffix)) {
            out.chop(suffix.size());
            break;
        }
    }
    return out;
}

QString displayNameFor(const AudioDeviceEntry &entry,
                       const QHash<QString, AudioSinkMeta> &sinkMeta,
                       const QHash<QString, AlsaHwCard> &cardsById) {
    if (isAlsaHwDevice(entry.name)) {
        const QString cardId = alsaCardIdFromDevice(entry.name);
        const auto it = cardsById.constFind(cardId);
        if (it != cardsById.cend()) {
            return friendlyAlsaCardLabel(*it);
        }
        if (!entry.description.isEmpty()) {
            return entry.description;
        }
        return cardId.isEmpty() ? entry.name : cardId;
    }

    const QString sinkId = sinkIdOf(entry.name);
    if (!sinkId.isEmpty()) {
        const auto it = sinkMeta.constFind(sinkId);
        if (it != sinkMeta.cend() && !it->nick.isEmpty()) {
            // PipeWire often uses "Speaker" — make it explicit for laptop audio.
            if (it->nick.compare(QLatin1String("Speaker"), Qt::CaseInsensitive) == 0
                || it->nick.compare(QLatin1String("Speakers"), Qt::CaseInsensitive) == 0) {
                return QStringLiteral("Built-in speakers");
            }
            return it->nick;
        }
    }
    if (!entry.description.isEmpty()) {
        const QString shortDesc = shortenDescription(entry.description);
        if (shortDesc.endsWith(QLatin1String(" Speaker"), Qt::CaseInsensitive)
            || shortDesc.compare(QLatin1String("Speaker"), Qt::CaseInsensitive) == 0) {
            return QStringLiteral("Built-in speakers");
        }
        return shortDesc;
    }
    return sinkId.isEmpty() ? entry.name : sinkId;
}

bool sinkUnavailable(const QString &name, const QHash<QString, AudioSinkMeta> &sinkMeta) {
    const QString sinkId = sinkIdOf(name);
    if (sinkId.isEmpty()) {
        return false;
    }
    const auto it = sinkMeta.constFind(sinkId);
    return it != sinkMeta.cend() && it->unavailable;
}

QVariantMap toItem(const AudioDeviceEntry &entry,
                   const QHash<QString, AudioSinkMeta> &sinkMeta,
                   const QHash<QString, AlsaHwCard> &cardsById) {
    QVariantMap item;
    item.insert(QStringLiteral("name"), entry.name);
    item.insert(QStringLiteral("description"), displayNameFor(entry, sinkMeta, cardsById));
    item.insert(QStringLiteral("usb"), entry.usb);
    return item;
}

QHash<QString, AlsaHwCard> indexCards(const QList<AlsaHwCard> &cards) {
    QHash<QString, AlsaHwCard> out;
    for (const AlsaHwCard &card : cards) {
        out.insert(card.id, card);
    }
    return out;
}

} // namespace

bool isAlsaHwDevice(const QString &name) {
    return name.startsWith(QLatin1String("alsa/hw:CARD="));
}

QString alsaCardIdFromDevice(const QString &name) {
    // alsa/hw:CARD=FC4,DEV=0  or  alsa/sysdefault:CARD=FC4
    const int cardKey = name.indexOf(QLatin1String("CARD="));
    if (cardKey < 0) {
        return {};
    }
    QString id = name.mid(cardKey + 5);
    const int comma = id.indexOf(QLatin1Char(','));
    if (comma >= 0) {
        id = id.left(comma);
    }
    return id.trimmed();
}

QString friendlyAlsaCardLabel(const AlsaHwCard &card) {
    if (!card.usb && looksBuiltInDriver(card.driver, card.id)) {
        return QStringLiteral("Built-in speakers");
    }
    if (!card.shortName.isEmpty()
        && card.shortName.compare(card.id, Qt::CaseInsensitive) != 0
        && !card.shortName.startsWith(QLatin1String("sof-"), Qt::CaseInsensitive)) {
        return card.shortName;
    }
    // "HiBy HiBy FC4 at usb-..." → "HiBy FC4"
    QString longName = card.longName;
    const int at = longName.indexOf(QLatin1String(" at "));
    if (at > 0) {
        longName = longName.left(at).trimmed();
    }
    longName.replace(QRegularExpression(QStringLiteral("\\b(\\w+)\\s+\\1\\b")),
                     QStringLiteral("\\1"));
    if (!longName.isEmpty()) {
        return longName;
    }
    return card.id;
}

QList<AlsaHwCard> loadAlsaHwCards() {
    QList<AlsaHwCard> out;
    QFile file(QStringLiteral("/proc/asound/cards"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return out;
    }
    // /proc files often report size 0; don't use QTextStream::atEnd().
    const QString text = QString::fromUtf8(file.readAll());

    const QRegularExpression headerRe(
        QStringLiteral("^\\s*(\\d+)\\s+\\[([^\\]]+)\\]\\s*:\\s*(.+)$"));

    AlsaHwCard pending;
    bool havePending = false;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch match = headerRe.match(line);
        if (match.hasMatch()) {
            if (havePending) {
                out.append(pending);
            }
            pending = AlsaHwCard{};
            pending.index = match.captured(1).toInt();
            pending.id = match.captured(2).trimmed();
            const QString rest = match.captured(3).trimmed();
            const int sep = rest.indexOf(QLatin1String(" - "));
            if (sep >= 0) {
                pending.driver = rest.left(sep).trimmed();
                pending.shortName = rest.mid(sep + 3).trimmed();
            } else {
                pending.driver = rest;
                pending.shortName = pending.id;
            }
            pending.usb = pending.driver.contains(QLatin1String("USB"), Qt::CaseInsensitive);
            havePending = true;
            continue;
        }
        if (havePending && !line.trimmed().isEmpty() && line.startsWith(QLatin1Char(' '))) {
            pending.longName = line.trimmed();
            if (QFile::exists(QStringLiteral("/proc/asound/card%1/usbid").arg(pending.index))) {
                pending.usb = true;
            }
        }
    }
    if (havePending) {
        if (QFile::exists(QStringLiteral("/proc/asound/card%1/usbid").arg(pending.index))) {
            pending.usb = true;
        }
        out.append(pending);
    }
    return out;
}

QList<AudioDeviceEntry> alsaHwDeviceEntries(const QList<AlsaHwCard> &cards) {
    QList<AudioDeviceEntry> out;
    QList<AudioDeviceEntry> builtin;
    out.reserve(cards.size());
    for (const AlsaHwCard &card : cards) {
        // Only expose the main playback PCM (device 0). HDMI on sof-hda is DEV=3+.
        if (!QFile::exists(QStringLiteral("/proc/asound/card%1/pcm0p").arg(card.index))
            && !QFile::exists(QStringLiteral("/dev/snd/pcmC%1D0p").arg(card.index))) {
            continue;
        }
        AudioDeviceEntry entry;
        entry.name = QStringLiteral("alsa/hw:CARD=%1,DEV=0").arg(card.id);
        entry.description = friendlyAlsaCardLabel(card);
        entry.usb = card.usb;
        if (card.usb) {
            out.append(entry);
        } else {
            builtin.append(entry);
        }
    }
    out.append(builtin);
    return out;
}

QString preferredUsbDacDevice(const QList<AlsaHwCard> &cards) {
    const QList<AudioDeviceEntry> entries = alsaHwDeviceEntries(cards);
    for (const AudioDeviceEntry &entry : entries) {
        if (entry.usb) {
            return entry.name;
        }
    }
    return {};
}

QString pulseCardNameForAlsaId(const QString &cardId) {
    if (cardId.isEmpty()) {
        return {};
    }
    QProcess proc;
    proc.start(QStringLiteral("pactl"), {QStringLiteral("list"), QStringLiteral("cards")});
    if (!proc.waitForFinished(2000) || proc.exitCode() != 0) {
        return {};
    }
    const QString text = QString::fromUtf8(proc.readAllStandardOutput());
    QString currentName;
    for (QString line : text.split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (line.startsWith(QLatin1String("Name: "))) {
            currentName = line.mid(6).trimmed();
            continue;
        }
        if (line.contains(QStringLiteral("alsa.id = \"%1\"").arg(cardId))) {
            return currentName;
        }
    }
    // Fallback: card name embeds the ALSA id.
    for (QString line : text.split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (line.startsWith(QLatin1String("Name: "))) {
            const QString name = line.mid(6).trimmed();
            if (name.contains(cardId, Qt::CaseInsensitive)) {
                return name;
            }
        }
    }
    return {};
}

QString pulseCardActiveProfile(const QString &pulseCardName) {
    if (pulseCardName.isEmpty()) {
        return {};
    }
    QProcess proc;
    proc.start(QStringLiteral("pactl"),
               {QStringLiteral("list"), QStringLiteral("cards")});
    if (!proc.waitForFinished(2000) || proc.exitCode() != 0) {
        return {};
    }
    const QString text = QString::fromUtf8(proc.readAllStandardOutput());
    bool inCard = false;
    for (QString line : text.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1String("Name: "))) {
            inCard = trimmed.mid(6).trimmed() == pulseCardName;
            continue;
        }
        if (!inCard) {
            continue;
        }
        if (trimmed.startsWith(QLatin1String("Card #"))) {
            break;
        }
        if (trimmed.startsWith(QLatin1String("Active Profile:"))) {
            return trimmed.mid(QStringLiteral("Active Profile:").size()).trimmed();
        }
    }
    return {};
}

bool setPulseCardProfile(const QString &pulseCardName, const QString &profile) {
    if (pulseCardName.isEmpty() || profile.isEmpty()) {
        return false;
    }
    QProcess proc;
    proc.start(QStringLiteral("pactl"),
               {QStringLiteral("set-card-profile"), pulseCardName, profile});
    if (!proc.waitForFinished(3000) || proc.exitCode() != 0) {
        qWarning("pactl set-card-profile %s %s failed: %s",
                 qPrintable(pulseCardName), qPrintable(profile),
                 qPrintable(QString::fromUtf8(proc.readAllStandardError())));
        return false;
    }
    return true;
}

bool acquireAlsaExclusiveLease(const QString &cardId, AlsaExclusiveLease *lease) {
    if (!lease || cardId.isEmpty()) {
        return false;
    }
    // Already holding this card.
    if (lease->held && lease->cardId == cardId) {
        return true;
    }
    // Switching cards: restore the previous lease first.
    if (lease->held) {
        restoreAlsaExclusiveLease(lease);
    }

    const QString pulseName = pulseCardNameForAlsaId(cardId);
    if (pulseName.isEmpty()) {
        // No PipeWire/Pulse card — ALSA may already be free.
        lease->cardId = cardId;
        lease->pulseCardName.clear();
        lease->previousProfile.clear();
        lease->held = true;
        return true;
    }

    const QString active = pulseCardActiveProfile(pulseName);
    if (active == QLatin1String("off")) {
        // Someone else already parked it; remember a sensible restore target.
        lease->cardId = cardId;
        lease->pulseCardName = pulseName;
        lease->previousProfile = QStringLiteral("output:analog-stereo");
        lease->held = true;
        return true;
    }

    if (!setPulseCardProfile(pulseName, QStringLiteral("off"))) {
        return false;
    }
    // WirePlumber needs a beat to drop the ALSA PCM.
    QThread::msleep(700);

    lease->cardId = cardId;
    lease->pulseCardName = pulseName;
    lease->previousProfile =
        active.isEmpty() ? QStringLiteral("output:analog-stereo") : active;
    lease->held = true;
    return true;
}

void restoreAlsaExclusiveLease(AlsaExclusiveLease *lease) {
    if (!lease || !lease->held) {
        return;
    }
    if (!lease->pulseCardName.isEmpty()) {
        const QString profile = lease->previousProfile.isEmpty()
                                    ? QStringLiteral("output:analog-stereo")
                                    : lease->previousProfile;
        if (profile != QLatin1String("off")) {
            setPulseCardProfile(lease->pulseCardName, profile);
            QThread::msleep(500);
        }
    }
    lease->held = false;
    lease->cardId.clear();
    lease->pulseCardName.clear();
    lease->previousProfile.clear();
}

QString pipewireSinkForAlsaHwDevice(const QString &alsaHwDevice) {
    const QString cardId = alsaCardIdFromDevice(alsaHwDevice);
    if (cardId.isEmpty()) {
        return {};
    }
    QProcess proc;
    proc.start(QStringLiteral("pactl"), {QStringLiteral("list"), QStringLiteral("sinks"),
                                         QStringLiteral("short")});
    if (!proc.waitForFinished(2000) || proc.exitCode() != 0) {
        return {};
    }
    // short lines: ID\tname\tdriver\t...
    const QString text = QString::fromUtf8(proc.readAllStandardOutput());
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QStringList parts = line.split(QLatin1Char('\t'));
        if (parts.size() < 2) {
            continue;
        }
        const QString sinkName = parts.at(1);
        if (sinkName.contains(cardId, Qt::CaseInsensitive)
            || (cardId.compare(QLatin1String("FC4"), Qt::CaseInsensitive) == 0
                && sinkName.contains(QLatin1String("HiBy"), Qt::CaseInsensitive))) {
            return QStringLiteral("pipewire/") + sinkName;
        }
    }
    return {};
}

QHash<QString, AudioSinkMeta> loadPulseSinkMeta() {
    QHash<QString, AudioSinkMeta> out;
    QProcess proc;
    proc.start(QStringLiteral("pactl"), {QStringLiteral("list"), QStringLiteral("sinks")});
    if (!proc.waitForStarted(500) || !proc.waitForFinished(2000)
        || proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        return out;
    }

    const QString text = QString::fromUtf8(proc.readAllStandardOutput());
    AudioSinkMeta current;
    QString currentName;
    QString activePort;
    QHash<QString, QString> portMeta;

    auto flush = [&]() {
        if (currentName.isEmpty()) {
            return;
        }
        if (!activePort.isEmpty()) {
            if (portMeta.contains(activePort)) {
                current.unavailable =
                    portMeta.value(activePort).contains(QLatin1String("not available"));
            } else {
                const QString trimmed = activePort.trimmed();
                for (auto it = portMeta.constBegin(); it != portMeta.constEnd(); ++it) {
                    if (it.key().trimmed() == trimmed || it.key().endsWith(trimmed)) {
                        current.unavailable =
                            it.value().contains(QLatin1String("not available"));
                        break;
                    }
                }
            }
        }
        out.insert(currentName, current);
        current = AudioSinkMeta{};
        currentName.clear();
        activePort.clear();
        portMeta.clear();
    };

    const QRegularExpression propRe(
        QStringLiteral("^\\t\\t([^=]+)\\s*=\\s*\"([^\"]*)\"$"));
    const QRegularExpression portRe(QStringLiteral("^\\t\\t([^:]+): .*?\\(([^)]*)\\)"));

    for (const QString &line : text.split(QLatin1Char('\n'))) {
        if (line.startsWith(QLatin1String("Sink #"))) {
            flush();
            continue;
        }
        if (line.startsWith(QLatin1String("\tName: "))) {
            currentName = line.mid(7).trimmed();
            continue;
        }
        if (line.startsWith(QLatin1String("\tActive Port: "))) {
            activePort = line.mid(14).trimmed();
            continue;
        }
        const QRegularExpressionMatch prop = propRe.match(line);
        if (prop.hasMatch()) {
            const QString key = prop.captured(1).trimmed();
            const QString value = prop.captured(2);
            if (key == QLatin1String("node.nick") && current.nick.isEmpty()) {
                current.nick = value;
            } else if (key == QLatin1String("device.nick") && current.nick.isEmpty()) {
                current.nick = value;
            } else if (key == QLatin1String("device.description") && current.nick.isEmpty()) {
                current.nick = value;
            }
            continue;
        }
        const QRegularExpressionMatch port = portRe.match(line);
        if (port.hasMatch()) {
            portMeta.insert(port.captured(1), port.captured(2));
        }
    }
    flush();
    return out;
}

QVariantList filterAudioDevices(const QList<AudioDeviceEntry> &raw,
                                bool dacPassthrough,
                                const QString &keepSelected,
                                const QHash<QString, AudioSinkMeta> &sinkMeta,
                                const QList<AlsaHwCard> &cards) {
    const QHash<QString, AlsaHwCard> cardsById = indexCards(cards);
    QList<AudioDeviceEntry> pipewire;
    QList<AudioDeviceEntry> pulse;
    QHash<QString, AudioDeviceEntry> byName;

    for (const AudioDeviceEntry &entry : raw) {
        if (entry.name.isEmpty() || entry.name == QLatin1String("auto") || isBackendDefault(entry.name)) {
            continue;
        }
        byName.insert(entry.name, entry);
        if (backendOf(entry.name) == QLatin1String("pipewire") && isPipewireOrPulseSink(entry.name)) {
            pipewire.append(entry);
        } else if (backendOf(entry.name) == QLatin1String("pulse") && isPipewireOrPulseSink(entry.name)) {
            pulse.append(entry);
        }
    }

    QList<AudioDeviceEntry> chosen;
    if (dacPassthrough) {
        chosen = alsaHwDeviceEntries(cards);
        for (const AudioDeviceEntry &entry : chosen) {
            byName.insert(entry.name, entry);
        }
    } else if (!pipewire.isEmpty()) {
        chosen = pipewire;
    } else if (!pulse.isEmpty()) {
        chosen = pulse;
    }
    // Normal mode: do not invent ALSA hw: entries — those are exclusive/DAC-only.

    QVariantList out;
    QSet<QString> seenSinkIds;
    QSet<QString> seenNames;
    out.reserve(chosen.size() + 1);

    for (const AudioDeviceEntry &entry : chosen) {
        const QString sinkId = sinkIdOf(entry.name);
        if (!sinkId.isEmpty() && !isAlsaHwDevice(entry.name)) {
            if (seenSinkIds.contains(sinkId)) {
                continue;
            }
            if (entry.name != keepSelected && sinkUnavailable(entry.name, sinkMeta)) {
                continue;
            }
            seenSinkIds.insert(sinkId);
        }
        if (seenNames.contains(entry.name)) {
            continue;
        }
        seenNames.insert(entry.name);
        out.append(toItem(entry, sinkMeta, cardsById));
    }

    if (!keepSelected.isEmpty() && !seenNames.contains(keepSelected)) {
        AudioDeviceEntry kept = byName.value(keepSelected);
        if (kept.name.isEmpty()) {
            kept.name = keepSelected;
            kept.description = keepSelected;
            if (isAlsaHwDevice(keepSelected)) {
                const QString cardId = alsaCardIdFromDevice(keepSelected);
                const auto it = cardsById.constFind(cardId);
                if (it != cardsById.cend()) {
                    kept.description = friendlyAlsaCardLabel(*it);
                    kept.usb = it->usb;
                }
            }
        }
        out.append(toItem(kept, sinkMeta, cardsById));
    }

    return out;
}
