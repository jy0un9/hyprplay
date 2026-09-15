#include <QtTest>

#include "core/AudioDeviceFilter.h"

class AudioDeviceFilterTest : public QObject {
    Q_OBJECT
private slots:
    void normalModeKeepsPipewireSinksOnly();
    void hidesUnavailableHdmi();
    void prefersPipewireOverPulseDuplicates();
    void dacModeUsesAlsaHwUsbFirst();
    void keepsSelectedDeviceEvenIfFiltered();
    void fallsBackToPulseWhenNoPipewire();
    void normalModeDoesNotInventAlsaHw();
    void usesNicknames();
    void labelsBuiltInSpeakers();
};

static AudioDeviceEntry entry(const char *name, const char *description, bool usb = false) {
    return {QString::fromUtf8(name), QString::fromUtf8(description), usb};
}

static QStringList namesOf(const QVariantList &items) {
    QStringList out;
    for (const QVariant &v : items) {
        out.append(v.toMap().value(QStringLiteral("name")).toString());
    }
    return out;
}

static QStringList descriptionsOf(const QVariantList &items) {
    QStringList out;
    for (const QVariant &v : items) {
        out.append(v.toMap().value(QStringLiteral("description")).toString());
    }
    return out;
}

static QList<AlsaHwCard> fakeCards() {
    AlsaHwCard usb;
    usb.index = 0;
    usb.id = QStringLiteral("FC4");
    usb.shortName = QStringLiteral("HiBy FC4");
    usb.longName = QStringLiteral("HiBy HiBy FC4 at usb-0000:00:14.0-9");
    usb.driver = QStringLiteral("USB-Audio");
    usb.usb = true;

    AlsaHwCard builtin;
    builtin.index = 1;
    builtin.id = QStringLiteral("sofhdadsp");
    builtin.shortName = QStringLiteral("sof-hda-dsp");
    builtin.longName = QStringLiteral("LENOVO-ThinkPad");
    builtin.driver = QStringLiteral("sof-hda-dsp");
    builtin.usb = false;

    return {usb, builtin};
}

void AudioDeviceFilterTest::normalModeKeepsPipewireSinksOnly() {
    const QList<AudioDeviceEntry> raw = {
        entry("auto", "Autoselect device"),
        entry("pipewire", "Default (pipewire)"),
        entry("pipewire/alsa_output.Speaker", "Core Ultra HD Audio Speaker"),
        entry("pipewire/alsa_output.FC4", "HiBy FC4 Analog Stereo"),
        entry("pulse/alsa_output.Speaker", "Core Ultra HD Audio Speaker"),
        entry("alsa/lavrate", "Rate Converter Plugin"),
        entry("alsa/surround51:CARD=FC4,DEV=0", "HiBy FC4 surround"),
        entry("alsa/sysdefault:CARD=FC4", "HiBy FC4, USB Audio/Default"),
        entry("jack", "Default (jack)"),
    };

    const QVariantList filtered = filterAudioDevices(raw, false, {}, {}, {});
    QCOMPARE(namesOf(filtered),
             (QStringList{QStringLiteral("pipewire/alsa_output.Speaker"),
                          QStringLiteral("pipewire/alsa_output.FC4")}));
    QCOMPARE(descriptionsOf(filtered),
             (QStringList{QStringLiteral("Built-in speakers"),
                          QStringLiteral("HiBy FC4")}));
}

void AudioDeviceFilterTest::hidesUnavailableHdmi() {
    const QList<AudioDeviceEntry> raw = {
        entry("pipewire/hdmi3", "HDMI 3 Output"),
        entry("pipewire/speaker", "Laptop Speaker"),
        entry("pipewire/fc4", "HiBy FC4 Analog Stereo"),
    };
    QHash<QString, AudioSinkMeta> meta;
    meta.insert(QStringLiteral("hdmi3"), {QStringLiteral("HDMI 3"), true});
    meta.insert(QStringLiteral("speaker"), {QStringLiteral("Speaker"), false});
    meta.insert(QStringLiteral("fc4"), {QStringLiteral("HiBy FC4"), false});

    const QVariantList filtered = filterAudioDevices(raw, false, {}, meta, {});
    QCOMPARE(namesOf(filtered),
             (QStringList{QStringLiteral("pipewire/speaker"), QStringLiteral("pipewire/fc4")}));
    QCOMPARE(descriptionsOf(filtered),
             (QStringList{QStringLiteral("Built-in speakers"), QStringLiteral("HiBy FC4")}));
}

void AudioDeviceFilterTest::prefersPipewireOverPulseDuplicates() {
    const QList<AudioDeviceEntry> raw = {
        entry("pipewire/sinkA", "A"),
        entry("pulse/sinkA", "A via pulse"),
        entry("pulse/sinkB", "B via pulse"),
    };
    QCOMPARE(namesOf(filterAudioDevices(raw, false, {}, {}, {})),
             QStringList{QStringLiteral("pipewire/sinkA")});
}

void AudioDeviceFilterTest::dacModeUsesAlsaHwUsbFirst() {
    const QList<AlsaHwCard> cards = fakeCards();
    // Pretend pcm0p exists by using empty raw — filter builds from cards.
    // alsaHwDeviceEntries checks /proc; for unit test call filter with keep and
    // inject via cards that may skip missing pcm — so assert helpers directly.
    QCOMPARE(friendlyAlsaCardLabel(cards.at(0)), QStringLiteral("HiBy FC4"));
    QCOMPARE(friendlyAlsaCardLabel(cards.at(1)), QStringLiteral("Built-in speakers"));
    QCOMPARE(preferredUsbDacDevice(cards), QStringLiteral("alsa/hw:CARD=FC4,DEV=0"));
    QVERIFY(isAlsaHwDevice(QStringLiteral("alsa/hw:CARD=FC4,DEV=0")));
    QCOMPARE(alsaCardIdFromDevice(QStringLiteral("alsa/hw:CARD=FC4,DEV=0")),
             QStringLiteral("FC4"));

    // When /proc pcm nodes exist on this machine, DAC mode list should be hw: entries.
    const QVariantList filtered = filterAudioDevices({}, true, {}, {}, cards);
    QVERIFY(!filtered.isEmpty());
    QCOMPARE(filtered.first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("alsa/hw:CARD=FC4,DEV=0"));
    QCOMPARE(filtered.first().toMap().value(QStringLiteral("description")).toString(),
             QStringLiteral("HiBy FC4"));
}

void AudioDeviceFilterTest::keepsSelectedDeviceEvenIfFiltered() {
    const QList<AudioDeviceEntry> raw = {
        entry("pipewire/speaker", "Speaker"),
        entry("pipewire/hdmi1", "HDMI 1"),
    };
    QHash<QString, AudioSinkMeta> meta;
    meta.insert(QStringLiteral("hdmi1"), {QStringLiteral("HDMI 1"), true});

    const QVariantList filtered =
        filterAudioDevices(raw, false, QStringLiteral("pipewire/hdmi1"), meta, {});
    QCOMPARE(namesOf(filtered),
             (QStringList{QStringLiteral("pipewire/speaker"), QStringLiteral("pipewire/hdmi1")}));
}

void AudioDeviceFilterTest::fallsBackToPulseWhenNoPipewire() {
    const QList<AudioDeviceEntry> raw = {
        entry("pulse/speaker", "Speaker"),
        entry("alsa/sysdefault:CARD=X", "X, Default"),
    };
    QCOMPARE(namesOf(filterAudioDevices(raw, false, {}, {}, {})),
             QStringList{QStringLiteral("pulse/speaker")});
}

void AudioDeviceFilterTest::normalModeDoesNotInventAlsaHw() {
    // Empty mpv list in normal mode → empty picker (wait for engine), not hw: spam.
    QCOMPARE(filterAudioDevices({}, false, {}, {}, fakeCards()).size(), 0);
}

void AudioDeviceFilterTest::usesNicknames() {
    const QList<AudioDeviceEntry> raw = {
        entry("pipewire/long", "Very Long Hardware Description Speaker"),
    };
    QHash<QString, AudioSinkMeta> meta;
    meta.insert(QStringLiteral("long"), {QStringLiteral("Speaker"), false});
    QCOMPARE(descriptionsOf(filterAudioDevices(raw, false, {}, meta, {})),
             QStringList{QStringLiteral("Built-in speakers")});
}

void AudioDeviceFilterTest::labelsBuiltInSpeakers() {
    AlsaHwCard card;
    card.id = QStringLiteral("sofhdadsp");
    card.shortName = QStringLiteral("sof-hda-dsp");
    card.driver = QStringLiteral("sof-hda-dsp");
    card.usb = false;
    QCOMPARE(friendlyAlsaCardLabel(card), QStringLiteral("Built-in speakers"));
}

int runAudioDeviceFilterTests(int argc, char **argv) {
    AudioDeviceFilterTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_audio_device_filter.moc"
