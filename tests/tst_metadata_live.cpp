#include "AudioFixture.h"
#include "TestEnv.h"

#include "core/ConfigService.h"
#include "core/DiscogsService.h"
#include "core/MetadataSearchService.h"
#include "core/SecretsStore.h"

#include <QSignalSpy>
#include <QtTest>

class MetadataLiveTest : public QObject {
    Q_OBJECT
private slots:
    void musicBrainzSearchLive();
    void discogsSearchLive();
};

void MetadataLiveTest::musicBrainzSearchLive() {
    if (!qEnvironmentVariableIsSet("HYPRPLAY_LIVE_NETWORK")) {
        QSKIP("Set HYPRPLAY_LIVE_NETWORK=1 for MusicBrainz/Discogs live tests");
    }
    TestEnv env;
    ConfigService config;
    config.load();
    DiscogsService discogs(&config);
    MetadataSearchService meta(&config, &discogs);

    meta.setCurrentFields({{QStringLiteral("artist"), QStringLiteral("Radiohead")},
                           {QStringLiteral("album"), QStringLiteral("OK Computer")},
                           {QStringLiteral("title"), QStringLiteral("Paranoid Android")}});
    QSignalSpy finished(&meta, &MetadataSearchService::searchFinished);
    meta.searchRelease(QStringLiteral("Radiohead"), QStringLiteral("OK Computer"));
    QVERIFY(finished.wait(90000));
    QVERIFY(finished.takeFirst().at(0).toBool());
    QVERIFY(meta.candidates().size() >= 1);
}

void MetadataLiveTest::discogsSearchLive() {
    if (!qEnvironmentVariableIsSet("HYPRPLAY_LIVE_NETWORK")) {
        QSKIP("Set HYPRPLAY_LIVE_NETWORK=1 for MusicBrainz/Discogs live tests");
    }
    const QByteArray token = qgetenv("DISCOGS_TOKEN");
    if (token.isEmpty()) {
        QSKIP("Set DISCOGS_TOKEN for live Discogs search");
    }

    TestEnv env;
    ConfigService config;
    config.load();
    DiscogsService discogs(&config);
    discogs.setToken(QString::fromUtf8(token));
    QVERIFY(discogs.hasToken());

    QSignalSpy finished(&discogs, &DiscogsService::releasesSearchFinished);
    discogs.searchReleases(QStringLiteral("Radiohead"), QStringLiteral("OK Computer"));
    QVERIFY(finished.wait(90000));
    const QVariantList results = finished.takeFirst().at(0).toList();
    QVERIFY2(!results.isEmpty(), "Discogs should return OK Computer releases");
}

int runMetadataLiveTests(int argc, char **argv) {
    MetadataLiveTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_metadata_live.moc"
