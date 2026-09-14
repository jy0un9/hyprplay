#include "AudioFixture.h"
#include "TestEnv.h"

#include "core/ConfigService.h"
#include "core/ImportService.h"
#include "core/LibraryService.h"
#include "core/TagService.h"

#include <QSignalSpy>
#include <QtTest>

class ImportTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void copyModeImportsAlbum();
};

void ImportTest::initTestCase() {
    if (!AudioFixture::ffmpegAvailable()) {
        QSKIP("ffmpeg not available");
    }
}

void ImportTest::copyModeImportsAlbum() {
    TestEnv env;
    const QString libraryRoot = env.libraryRoot();
    const QString inbox = env.inboxDir();
    const QString srcAlbum =
        AudioFixture::createAlbumTree(inbox, QStringLiteral("ImpArtist"), QStringLiteral("ImpAlbum"));
    QVERIFY(AudioFixture::writeTaggedSample(
        srcAlbum + QStringLiteral("/01.flac"), {QStringLiteral("-c:a"), QStringLiteral("flac")},
        {{QStringLiteral("title"), QStringLiteral("One")},
         {QStringLiteral("artist"), QStringLiteral("ImpArtist")},
         {QStringLiteral("album"), QStringLiteral("ImpAlbum")}}));
    QVERIFY(AudioFixture::writeTaggedSample(
        srcAlbum + QStringLiteral("/02.flac"), {QStringLiteral("-c:a"), QStringLiteral("flac")},
        {{QStringLiteral("title"), QStringLiteral("Two")},
         {QStringLiteral("artist"), QStringLiteral("ImpArtist")},
         {QStringLiteral("album"), QStringLiteral("ImpAlbum")}}));

    ConfigService config;
    config.load();
    config.setLibraryPaths(libraryRoot);
    config.setImportInbox(inbox);
    config.save();

    LibraryService library;
    TagService tags(&library);
    ImportService imports(&config, &library, &tags);

    imports.scanInbox();
    QTRY_VERIFY(imports.albums().size() >= 1);
    imports.setAllAlbumsSelected(true);

    QSignalSpy finished(&imports, &ImportService::importFinished);
    imports.startImport();
    QVERIFY(finished.wait(120000));
    QVERIFY(finished.takeFirst().at(0).toBool());

    QSignalSpy scanDone(&library, &LibraryService::scanFinished);
    library.rescan({libraryRoot});
    QVERIFY(scanDone.wait(60000));
    QVERIFY(library.artists().contains(QStringLiteral("ImpArtist"))
            || library.trackCount() >= 2);
}

int runImportTests(int argc, char **argv) {
    ImportTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_import.moc"
