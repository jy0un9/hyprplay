#include "AudioFixture.h"
#include "TestEnv.h"

#include "core/AudioFormats.h"
#include "core/ConfigService.h"
#include "core/LibraryService.h"
#include "core/TagService.h"

#include <QSignalSpy>
#include <QtTest>

class LibraryTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void scansSupportedFormats();
    void rejectsUnsupportedExtension();
    void reingestAfterTagChange();
};

void LibraryTest::initTestCase() {
    if (!AudioFixture::ffmpegAvailable()) {
        QSKIP("ffmpeg not available");
    }
}

void LibraryTest::scansSupportedFormats() {
    TestEnv env;
    const QString root = env.libraryRoot();
    const QString albumDir =
        AudioFixture::createAlbumTree(root, QStringLiteral("Artist A"), QStringLiteral("Album A"));

    for (const auto &spec : AudioFixture::allFormatSpecs()) {
        const QString path = albumDir + QLatin1Char('/') + spec.fileName;
        QVERIFY2(AudioFixture::writeTaggedSample(
                     path, spec.codecArgs,
                     {{QStringLiteral("title"), QStringLiteral("Format Probe")},
                      {QStringLiteral("artist"), QStringLiteral("Artist A")},
                      {QStringLiteral("album"), QStringLiteral("Album A")}}),
                 qPrintable(spec.fileName));
    }

    LibraryService library;
    QSignalSpy finished(&library, &LibraryService::scanFinished);
    library.ensureLibrary({root});
    QVERIFY(finished.wait(60000));
    QVERIFY(finished.takeFirst().at(0).toBool());
    QVERIFY(library.trackCount() >= 5);
    QVERIFY(library.trackCount() <= AudioFixture::allFormatSpecs().size());
    QVERIFY(library.artists().contains(QStringLiteral("Artist A")));
    QCOMPARE(library.albumsForArtist(QStringLiteral("Artist A")),
             QStringList{QStringLiteral("Album A")});
    QVERIFY(library.tracksForAlbum(QStringLiteral("Artist A"), QStringLiteral("Album A")).size()
            >= 5);
}

void LibraryTest::rejectsUnsupportedExtension() {
    QVERIFY(!isSupportedAudioFile(QStringLiteral("/tmp/x.wav")));
    QVERIFY(!isSupportedAudioFile(QStringLiteral("/tmp/x.txt")));
    QVERIFY(isSupportedAudioFile(QStringLiteral("/tmp/x.oga")));
}

void LibraryTest::reingestAfterTagChange() {
    TestEnv env;
    const QString root = env.libraryRoot();
    const QString albumDir =
        AudioFixture::createAlbumTree(root, QStringLiteral("Reingest"), QStringLiteral("Album"));
    const QString path = albumDir + QStringLiteral("/song.flac");
    QVERIFY(AudioFixture::writeTaggedSample(
        path, {QStringLiteral("-c:a"), QStringLiteral("flac")},
        {{QStringLiteral("title"), QStringLiteral("Before")},
         {QStringLiteral("artist"), QStringLiteral("Reingest")},
         {QStringLiteral("album"), QStringLiteral("Album")}}));

    LibraryService library;
    QSignalSpy finished(&library, &LibraryService::scanFinished);
    library.ensureLibrary({root});
    QVERIFY(finished.wait(60000));

    TagService tags(&library);
    QVERIFY(tags.writeTagsToFile(path, {{QStringLiteral("title"), QStringLiteral("After")}}));
    QVERIFY(library.reingestFile(path));
    QCOMPARE(library.trackByPath(path).value(QStringLiteral("title")).toString(),
             QStringLiteral("After"));
}

int runLibraryTests(int argc, char **argv) {
    LibraryTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_library.moc"
