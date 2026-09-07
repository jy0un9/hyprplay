#include <QtTest>

#include "core/LrcParser.h"

class LrcParserTest : public QObject {
    Q_OBJECT
private slots:
    void parsesMmSsTimestamps();
    void parsesOffsetTag();
    void parsesMultiStampLine();
    void stripsEnhancedTags();
};

void LrcParserTest::parsesMmSsTimestamps() {
    double secs = 0.0;
    QVERIFY(parseLrcTimestamp(QStringLiteral("01:02.50"), &secs));
    QCOMPARE(secs, 62.5);
    QVERIFY(parseLrcTimestamp(QStringLiteral("1:05:03.00"), &secs));
    QCOMPARE(secs, 3903.0);
}

void LrcParserTest::parsesOffsetTag() {
    double offset = 0.0;
    QVERIFY(parseLrcOffsetMs(QStringLiteral("[offset:500]"), &offset));
    QCOMPARE(offset, 0.5);
}

void LrcParserTest::parsesMultiStampLine() {
    const QString content = QStringLiteral(
        "[offset:1000]\n"
        "[00:10.00][00:20.00]Hello\n"
        "[00:30.00]World\n");
    const QVector<LrcLine> lines = parseLrcContent(content);
    QCOMPARE(lines.size(), 3);
    QCOMPARE(lines.at(0).time, 11.0);
    QCOMPARE(lines.at(0).text, QStringLiteral("Hello"));
    QCOMPARE(lines.at(1).time, 21.0);
    QCOMPARE(lines.at(1).text, QStringLiteral("Hello"));
    QCOMPARE(lines.at(2).time, 31.0);
    QCOMPARE(lines.at(2).text, QStringLiteral("World"));
}

void LrcParserTest::stripsEnhancedTags() {
    QCOMPARE(stripEnhancedLrcTags(QStringLiteral("A <00:01.00>word")), QStringLiteral("A word"));
}

#include "tst_lrcparser.moc"

int runLrcParserTests(int argc, char **argv) {
    LrcParserTest test;
    return QTest::qExec(&test, argc, argv);
}
