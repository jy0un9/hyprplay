#include <QtTest>

#include "core/LyricsParsers.h"

#include <QJsonDocument>

class LyricsParsersTest : public QObject {
    Q_OBJECT
private slots:
    void parsesOvLyrics();
    void parsesNeteaseSynced();
    void picksNeteaseMatch();
    void picksNeteaseMatchNormalized();
    void normalizesQueryText();
    void picksLrclibMatchScored();
    void picksLrclibMatchRejects();
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

void LyricsParsersTest::picksNeteaseMatchNormalized() {
    // "feat." suffix and "(Remastered)" parenthetical must not block the match.
    const auto doc = QJsonDocument::fromJson(R"({
      "result": {
        "songs": [
          {
            "id": 7,
            "name": "Yellow",
            "duration": 266000,
            "artists": [{"name": "Coldplay"}]
          }
        ]
      }
    })");
    QCOMPARE(LyricsParsers::pickNeteaseMatch(doc, QStringLiteral("Coldplay"),
                                             QStringLiteral("Yellow (Remastered)"), 266.0),
             7);
}

void LyricsParsersTest::normalizesQueryText() {
    QCOMPARE(LyricsParsers::normalizeQueryText(QStringLiteral("01 - Yellow (Remastered)")),
             QStringLiteral("yellow"));
    QCOMPARE(LyricsParsers::normalizeQueryText(QStringLiteral("Yellow feat. Beyonce")),
             QStringLiteral("yellow"));
    // Unicode letters survive; parentheticals drop.
    QVERIFY(!LyricsParsers::normalizeQueryText(QStringLiteral("夜曲 (Live)")).isEmpty());
}

void LyricsParsersTest::picksLrclibMatchScored() {
    const auto doc = QJsonDocument::fromJson(R"([
      {"id": 1, "trackName": "Yellowish", "artistName": "Coldplay",
       "albumName": "Other", "duration": 266.0,
       "syncedLyrics": "[00:01.00]x\n", "instrumental": false},
      {"id": 2, "trackName": "Yellow", "artistName": "Coldplay",
       "albumName": "Parachutes", "duration": 266.4,
       "syncedLyrics": "[00:01.00]y\n", "instrumental": false}
    ])");
    // Exact title + album bonus beats the near miss despite both matching.
    QCOMPARE(LyricsParsers::pickLrclibMatch(doc.array(), QStringLiteral("Coldplay"),
                                            QStringLiteral("Yellow"),
                                            QStringLiteral("Parachutes"), 266.0),
             2);
}

void LyricsParsersTest::picksLrclibMatchRejects() {
    const auto doc = QJsonDocument::fromJson(R"([
      {"id": 1, "trackName": "Yellow", "artistName": "Coldplay",
       "duration": 266.0, "syncedLyrics": "", "instrumental": false},
      {"id": 2, "trackName": "Yellow", "artistName": "Coldplay",
       "duration": 266.0, "syncedLyrics": "[00:01.00]y\n", "instrumental": true},
      {"id": 3, "trackName": "Yellow", "artistName": "Coldplay",
       "duration": 400.0, "syncedLyrics": "[00:01.00]y\n", "instrumental": false}
    ])");
    // Untimed, instrumental, and duration-outlier candidates all rejected.
    QCOMPARE(LyricsParsers::pickLrclibMatch(doc.array(), QStringLiteral("Coldplay"),
                                            QStringLiteral("Yellow"), QString(), 266.0),
             0);
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
