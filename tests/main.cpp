#include <QGuiApplication>

int runFuzzyMatchTests(int argc, char **argv);
int runAudioDeviceFilterTests(int argc, char **argv);
int runLrcParserTests(int argc, char **argv);
int runPlaylistM3uTests(int argc, char **argv);
int runLyricsParsersTests(int argc, char **argv);
int runAudioFormatsTests(int argc, char **argv);
int runLibraryTests(int argc, char **argv);
int runPlaybackTests(int argc, char **argv);
int runTrackMediaTests(int argc, char **argv);
int runPlaylistServiceTests(int argc, char **argv);
int runImportTests(int argc, char **argv);
int runConvertTests(int argc, char **argv);
int runTagServiceTests(int argc, char **argv);
int runLyricsServiceTests(int argc, char **argv);
int runDiscogsRankTests(int argc, char **argv);
int runMetadataLiveTests(int argc, char **argv);
int runDesktopCliThemeTests(int argc, char **argv);
int runFormatMatrixTests(int argc, char **argv);

int main(int argc, char **argv) {
    // Gui app for theme palette + QTRY_* event loop; keep tests headless-friendly.
    qputenv("QT_QPA_PLATFORM", qgetenv("QT_QPA_PLATFORM").isEmpty() ? QByteArray("offscreen")
                                                                    : qgetenv("QT_QPA_PLATFORM"));
    qputenv("HYPRPLAY_TEST_AO", qgetenv("HYPRPLAY_TEST_AO").isEmpty() ? QByteArray("null")
                                                                       : qgetenv("HYPRPLAY_TEST_AO"));

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("hyprplay"));
    QGuiApplication::setOrganizationName(QString());

    int status = 0;
    status |= runFuzzyMatchTests(argc, argv);
    status |= runAudioDeviceFilterTests(argc, argv);
    status |= runLrcParserTests(argc, argv);
    status |= runPlaylistM3uTests(argc, argv);
    status |= runLyricsParsersTests(argc, argv);
    status |= runAudioFormatsTests(argc, argv);
    status |= runLibraryTests(argc, argv);
    status |= runPlaybackTests(argc, argv);
    status |= runTrackMediaTests(argc, argv);
    status |= runPlaylistServiceTests(argc, argv);
    status |= runImportTests(argc, argv);
    status |= runConvertTests(argc, argv);
    status |= runTagServiceTests(argc, argv);
    status |= runLyricsServiceTests(argc, argv);
    status |= runDiscogsRankTests(argc, argv);
    status |= runMetadataLiveTests(argc, argv);
    status |= runDesktopCliThemeTests(argc, argv);
    status |= runFormatMatrixTests(argc, argv);
    return status;
}
