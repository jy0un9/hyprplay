#include <QtTest>

#include "core/FuzzyMatch.h"

class FuzzyMatchTest : public QObject {
    Q_OBJECT
private slots:
    void matchesSubstring();
    void rejectsMiss();
    void filtersAndRanks();
    void filtersTracks();
};

void FuzzyMatchTest::matchesSubstring() {
    QVERIFY(fuzzyMatch(QStringLiteral("Dark Side of the Moon"), QStringLiteral("dark")));
    QVERIFY(fuzzyMatch(QStringLiteral("Abbey Road"), QStringLiteral("abyrd")));
}

void FuzzyMatchTest::rejectsMiss() {
    QVERIFY(!fuzzyMatch(QStringLiteral("Hello"), QStringLiteral("xyz")));
}

void FuzzyMatchTest::filtersAndRanks() {
    const QStringList items = {QStringLiteral("Pink Floyd"), QStringLiteral("Fleetwood Mac"),
                               QStringLiteral("Radiohead")};
    const QStringList filtered = filterFuzzy(items, QStringLiteral("floyd"));
    QCOMPARE(filtered.size(), 1);
    QCOMPARE(filtered.first(), QStringLiteral("Pink Floyd"));
}

void FuzzyMatchTest::filtersTracks() {
    QVariantList tracks;
    tracks << QVariantMap{{QStringLiteral("title"), QStringLiteral("Time")},
                          {QStringLiteral("artist"), QStringLiteral("Pink Floyd")},
                          {QStringLiteral("album"), QStringLiteral("DSOTM")},
                          {QStringLiteral("trackNumber"), 4}};
    tracks << QVariantMap{{QStringLiteral("title"), QStringLiteral("Dreams")},
                          {QStringLiteral("artist"), QStringLiteral("Fleetwood Mac")},
                          {QStringLiteral("album"), QStringLiteral("Rumours")},
                          {QStringLiteral("trackNumber"), 2}};
    const QVariantList filtered = filterTracksFuzzy(tracks, QStringLiteral("dream"));
    QCOMPARE(filtered.size(), 1);
    QCOMPARE(filtered.first().toMap().value(QStringLiteral("title")).toString(),
             QStringLiteral("Dreams"));
}

#include "tst_fuzzymatch.moc"

int runFuzzyMatchTests(int argc, char **argv) {
    FuzzyMatchTest test;
    return QTest::qExec(&test, argc, argv);
}
