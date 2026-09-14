#include "core/DiscogsService.h"

#include <QtTest>

class DiscogsRankTest : public QObject {
    Q_OBJECT
private slots:
    void prefersCdAndMatchingMediaCount();
    void emptyInput();
};

void DiscogsRankTest::prefersCdAndMatchingMediaCount() {
    QVariantList results;
    results << QVariantMap{{QStringLiteral("id"), 1},
                           {QStringLiteral("title"), QStringLiteral("A - Vinyl")},
                           {QStringLiteral("mediaCount"), 1},
                           {QStringLiteral("format"), QStringLiteral("Vinyl")}};
    results << QVariantMap{{QStringLiteral("id"), 2},
                           {QStringLiteral("title"), QStringLiteral("A - CD")},
                           {QStringLiteral("mediaCount"), 1},
                           {QStringLiteral("format"), QStringLiteral("CD")}};
    results << QVariantMap{{QStringLiteral("id"), 3},
                           {QStringLiteral("title"), QStringLiteral("A - CD multi")},
                           {QStringLiteral("mediaCount"), 3},
                           {QStringLiteral("format"), QStringLiteral("CD")}};

    const QVariantList ranked = rankDiscogsAlbumCandidates(results, 10);
    QCOMPARE(ranked.size(), 3);
    // CD preferred over Vinyl; among CDs, mediaCount closer to expected discs (~1).
    QCOMPARE(ranked.first().toMap().value(QStringLiteral("id")).toInt(), 2);
}

void DiscogsRankTest::emptyInput() {
    QCOMPARE(rankDiscogsAlbumCandidates({}, 5).size(), 0);
}

int runDiscogsRankTests(int argc, char **argv) {
    DiscogsRankTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_discogs_rank.moc"
