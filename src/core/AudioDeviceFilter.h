#pragma once

#include <QHash>
#include <QString>
#include <QVariantList>

// Collapse mpv's kitchen-sink audio-device-list into the outputs a user would
// actually pick: PipeWire/Pulse sinks in normal mode, exclusive ALSA hw: cards
// when DAC passthrough is on. Optional Pulse/PipeWire sink metadata supplies
// short nicknames and hides HDMI ports that are not plugged in.

struct AudioDeviceEntry {
    QString name;
    QString description;
    bool usb = false;
};

struct AudioSinkMeta {
    QString nick;
    bool unavailable = false;
};

struct AlsaHwCard {
    int index = -1;
    QString id;          // e.g. FC4, sofhdadsp
    QString shortName;   // e.g. HiBy FC4
    QString longName;    // e.g. HiBy HiBy FC4 at usb-...
    QString driver;      // e.g. USB-Audio, sof-hda-dsp
    bool usb = false;
};

// Borrow an ALSA card from PipeWire/WirePlumber by setting its profile to
// "off", then restore the previous profile when done. Leaving the profile off
// is what makes the DAC vanish from OS settings — always restore on exit.
struct AlsaExclusiveLease {
    QString cardId;
    QString pulseCardName;
    QString previousProfile;
    bool held = false;
};

QHash<QString, AudioSinkMeta> loadPulseSinkMeta();
QList<AlsaHwCard> loadAlsaHwCards();
QList<AudioDeviceEntry> alsaHwDeviceEntries(const QList<AlsaHwCard> &cards = loadAlsaHwCards());
QString preferredUsbDacDevice(const QList<AlsaHwCard> &cards = loadAlsaHwCards());
QString friendlyAlsaCardLabel(const AlsaHwCard &card);
bool isAlsaHwDevice(const QString &name);
QString alsaCardIdFromDevice(const QString &name);

QString pulseCardNameForAlsaId(const QString &cardId);
QString pulseCardActiveProfile(const QString &pulseCardName);
bool setPulseCardProfile(const QString &pulseCardName, const QString &profile);
bool acquireAlsaExclusiveLease(const QString &cardId, AlsaExclusiveLease *lease);
void restoreAlsaExclusiveLease(AlsaExclusiveLease *lease);

// Map an exclusive alsa/hw: device back to the PipeWire sink for shared playback,
// or empty string for system default when no sink is found yet.
QString pipewireSinkForAlsaHwDevice(const QString &alsaHwDevice);

QVariantList filterAudioDevices(const QList<AudioDeviceEntry> &raw,
                                bool dacPassthrough,
                                const QString &keepSelected,
                                const QHash<QString, AudioSinkMeta> &sinkMeta,
                                const QList<AlsaHwCard> &cards = loadAlsaHwCards());
