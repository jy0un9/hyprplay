#include "AudioFixture.h"
#include "TestEnv.h"

#include "core/ConfigService.h"
#include "core/ConvertService.h"
#include "core/LibraryService.h"
#include "core/OpusConvert.h"

#include <QFile>
#include <QSignalSpy>
#include <QtTest>

class ConvertTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void opusConvertHelperWorks();
    void convertsFlacAlbumInLibrary();
};

void ConvertTest::initTestCase() {
    if (!AudioFixture::ffmpegAvailable()) {
        QSKIP("ffmpeg not available");
    }
}

void ConvertTest::opusConvertHelperWorks() {
    TestEnv env;
    const QString flac = env.cacheHome() + QStringLiteral("/sample.flac");
    QVERIFY(AudioFixture::writeTaggedSample(
        flac, {QStringLiteral("-c:a"), QStringLiteral("flac")},
        {{QStringLiteral("title"), QStringLiteral("One")},
         {QStringLiteral("artist"), QStringLiteral("A")},
         {QStringLiteral("album"), QStringLiteral("B")}}));
    const QString opus = env.cacheHome() + QStringLiteral("/sample.opus");
    QString error;
    QVERIFY2(convertFlacToOpus(flac, opus, 128, &error), qPrintable(error));
    QVERIFY(QFile::exists(opus));
}

void ConvertTest::convertsFlacAlbumInLibrary() {
    TestEnv env;
    const QString libraryRoot = env.libraryRoot();
    const QString albumDir =
        AudioFixture::createAlbumTree(libraryRoot, QStringLiteral("ConvArtist"),
                                      QStringLiteral("ConvAlbum"));
    const QString flacPath = albumDir + QStringLiteral("/01.flac");
    QVERIFY(AudioFixture::writeTaggedSample(
        flacPath, {QStringLiteral("-c:a"), QStringLiteral("flac")},
        {{QStringLiteral("title"), QStringLiteral("One")},
         {QStringLiteral("artist"), QStringLiteral("ConvArtist")},
         {QStringLiteral("album"), QStringLiteral("ConvAlbum")}}));

    ConfigService config;
    config.load();
    config.setLibraryPaths(libraryRoot);
    config.setOpusBitrateKbps(128);
    config.setConvertDeleteSource(true);
    config.save();

    LibraryService library;
    QSignalSpy scanDone(&library, &LibraryService::scanFinished);
    library.rescan({libraryRoot});
    QVERIFY(scanDone.wait(60000));

    ConvertService convert(&config, &library);
    QObject::connect(&convert, &ConvertService::decisionRequired, &convert,
                     [&convert](const QString &, const QString &message, const QString &) {
                         QFAIL(qPrintable(QStringLiteral("Unexpected convert decision: ")
                                          + message));
                         convert.resolveConvertDecision(QStringLiteral("abort"));
                     });

    convert.scanLibrary();
    QTRY_VERIFY(convert.albums().size() >= 1);
    convert.setAllAlbumsSelected(true);

    QSignalSpy finished(&convert, &ConvertService::convertFinished);
    convert.startConvert();
    QVERIFY2(finished.wait(120000), qPrintable(convert.status()));
    QVERIFY(finished.takeFirst().at(0).toBool());

    const QString opusPath = albumDir + QStringLiteral("/01.opus");
    QVERIFY(QFile::exists(opusPath));
    QVERIFY(!QFile::exists(flacPath));
}

int runConvertTests(int argc, char **argv) {
    ConvertTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_convert.moc"
