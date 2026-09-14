#include "AudioFixture.h"
#include "TestEnv.h"

#include "core/LibraryService.h"
#include "core/TagService.h"

#include <QSignalSpy>
#include <QtTest>

class TagServiceTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void roundTripTagsPerFormat();
    void albumArtistWrite();
};

void TagServiceTest::initTestCase() {
    if (!AudioFixture::ffmpegAvailable()) {
        QSKIP("ffmpeg not available");
    }
}

void TagServiceTest::roundTripTagsPerFormat() {
    TestEnv env;
    LibraryService library;
    TagService tags(&library);
    const QString dir = env.libraryRoot();

    for (const auto &spec : AudioFixture::allFormatSpecs()) {
        // ADTS AAC often cannot hold rich tags — still verify write doesn't crash.
        const QString path = dir + QLatin1Char('/') + spec.fileName;
        QVERIFY2(AudioFixture::writeTaggedSample(
                     path, spec.codecArgs,
                     {{QStringLiteral("title"), QStringLiteral("Orig")},
                      {QStringLiteral("artist"), QStringLiteral("TagArtist")},
                      {QStringLiteral("album"), QStringLiteral("TagAlbum")}}),
                 qPrintable(spec.fileName));

        const bool ok = tags.writeTagsToFile(
            path, {{QStringLiteral("title"), QStringLiteral("Edited")},
                   {QStringLiteral("artist"), QStringLiteral("TagArtist")},
                   {QStringLiteral("album"), QStringLiteral("TagAlbum")},
                   {QStringLiteral("trackNumber"), 7},
                   {QStringLiteral("year"), 2024},
                   {QStringLiteral("genre"), QStringLiteral("Test")}});
        if (spec.fileName.endsWith(QStringLiteral(".aac"))) {
            // Best-effort for raw ADTS.
            Q_UNUSED(ok);
            continue;
        }
        QVERIFY2(ok, qPrintable(spec.fileName));
        const QVariantMap loaded = tags.loadTags(path);
        QCOMPARE(loaded.value(QStringLiteral("title")).toString(), QStringLiteral("Edited"));
        QCOMPARE(loaded.value(QStringLiteral("artist")).toString(), QStringLiteral("TagArtist"));
    }
}

void TagServiceTest::albumArtistWrite() {
    TestEnv env;
    LibraryService library;
    TagService tags(&library);
    const QString path = env.libraryRoot() + QStringLiteral("/aa.flac");
    QVERIFY(AudioFixture::writeTaggedSample(path, {QStringLiteral("-c:a"), QStringLiteral("flac")}));
    QVERIFY(tags.writeTagsToFile(
        path, {{QStringLiteral("title"), QStringLiteral("X")},
               {QStringLiteral("artist"), QStringLiteral("Performer")},
               {QStringLiteral("albumArtist"), QStringLiteral("Various")},
               {QStringLiteral("album"), QStringLiteral("Comp")}}));
    const QVariantMap loaded = tags.loadTags(path);
    QCOMPARE(loaded.value(QStringLiteral("albumArtist")).toString(), QStringLiteral("Various"));
}

int runTagServiceTests(int argc, char **argv) {
    TagServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_tags.moc"
