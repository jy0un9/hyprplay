#include <QtTest>

#include "core/LyricsParsers.h"

#include <QJsonDocument>

class LyricsParsersTest : public QObject {
    Q_OBJECT
private slots:
    void parsesOvLyrics();
    void parsesNeteaseSynced();
    void picksNeteaseMatch();
    void scrapesGeniusHtml();
    void buildsSummary();
};

void LyricsParsersTest::parsesOvLyrics() {
    const auto doc = QJsonDocument::fromJson(R"({"lyrics":"line one\nline two"})");
    QCOMPARE(LyricsParsers::ovPlainFromDoc(doc), QStringLiteral("line one\nline two"));
}

void LyricsParsersTest::parsesNeteaseSynced() {
    const auto doc = QJsonDocument::fromJson(
        R"({"code":200,"lrc":{"lyric":"[00:01.00]hi\n"}})");
    QCOMPARE(LyricsParsers::neteaseSyncedFromDoc(doc), QStringLiteral("[00:01.00]hi\n"));
    const auto bad = QJsonDocument::fromJson(R"({"code":200,"nolyric":true})");
    QVERIFY(LyricsParsers::neteaseSyncedFromDoc(bad).isEmpty());
}

void LyricsParsersTest::picksNeteaseMatch() {
    const auto doc = QJsonDocument::fromJson(R"({
      "result": {
        "songs": [
          {
            "id": 42,
            "name": "Time",
            "duration": 421000,
            "artists": [{"name": "Pink Floyd"}]
          }
        ]
      }
    })");
    QCOMPARE(LyricsParsers::pickNeteaseMatch(doc, QStringLiteral("Pink Floyd"),
                                             QStringLiteral("Time"), 420.0),
             42);
}

void LyricsParsersTest::scrapesGeniusHtml() {
    const QString html = QStringLiteral(
        "<div data-lyrics-container=\"true\">"
        "This is a longer first lyric line for scrape validation<br/>"
        "Second lyric line continues the verse content here<br/>"
        "Third lyric line keeps the character count rising<br/>"
        "Fourth lyric line is required for the line threshold<br/>"
        "Fifth lyric line adds more readable text content<br/>"
        "Sixth lyric line still going with more words now<br/>"
        "Seventh lyric line almost finished with the block<br/>"
        "Eighth lyric line completes the container contents"
        "</div>");
    const QString text = LyricsParsers::scrapeGeniusHtml(html);
    QVERIFY(!text.isEmpty());
    QVERIFY(text.contains(QStringLiteral("longer first lyric")));
}

void LyricsParsersTest::buildsSummary() {
    QCOMPARE(LyricsParsers::buildSummary(0, 0, 0, 0, 0, 2, 0, 0),
             QStringLiteral("Lyrics already present"));
    QVERIFY(LyricsParsers::buildSummary(3, 2, 1, 0, 0, 0, 0, 0)
                .startsWith(QStringLiteral("Lyrics: 3 fetched")));
}

#include "tst_lyrics_parsers.moc"

int runLyricsParsersTests(int argc, char **argv) {
    LyricsParsersTest test;
    return QTest::qExec(&test, argc, argv);
}
