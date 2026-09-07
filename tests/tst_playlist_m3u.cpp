#include <QtTest>

#include "core/PlaylistM3u.h"

class PlaylistM3uTest : public QObject {
    Q_OBJECT
private slots:
    void parsesExtInfEntries();
    void roundTrips();
};

void PlaylistM3uTest::parsesExtInfEntries() {
    const QString content = QStringLiteral(
        "#EXTM3U\n"
        "#EXTINF:180,Pink Floyd - Time\n"
        "Pink Floyd/DSOTM/04 - Time.flac\n"
        "# comment\n"
        "relative/path.opus\n");
    const QVector<M3uEntry> entries = parseM3u8(content);
    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries.at(0).durationSecs, 180);
    QCOMPARE(entries.at(0).display, QStringLiteral("Pink Floyd - Time"));
    QCOMPARE(entries.at(0).sourcePath, QStringLiteral("Pink Floyd/DSOTM/04 - Time.flac"));
    QCOMPARE(entries.at(1).display, QStringLiteral("path"));
    QCOMPARE(entries.at(1).sourcePath, QStringLiteral("relative/path.opus"));
}

void PlaylistM3uTest::roundTrips() {
    QVector<M3uEntry> original;
    original.append({QStringLiteral("a/one.flac"), QStringLiteral("Artist - One"), 120});
    original.append({QStringLiteral("b/two.opus"), QStringLiteral("Two"), 90});

    const QString serialized = serializeM3u8(original);
    QVERIFY(serialized.startsWith(QStringLiteral("#EXTM3U\n")));
    const QVector<M3uEntry> parsed = parseM3u8(serialized);
    QCOMPARE(parsed.size(), original.size());
    for (int i = 0; i < original.size(); ++i) {
        QCOMPARE(parsed.at(i).sourcePath, original.at(i).sourcePath);
        QCOMPARE(parsed.at(i).display, original.at(i).display);
        QCOMPARE(parsed.at(i).durationSecs, original.at(i).durationSecs);
    }
}

#include "tst_playlist_m3u.moc"

int runPlaylistM3uTests(int argc, char **argv) {
    PlaylistM3uTest test;
    return QTest::qExec(&test, argc, argv);
}
