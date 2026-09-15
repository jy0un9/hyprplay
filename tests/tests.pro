QT += core gui testlib network sql concurrent dbus
CONFIG += console c++17 testcase
CONFIG -= app_bundle

TARGET = hyprplay-tests
TEMPLATE = app

DEFINES += APP_ID=\\\"hyprplay\\\"

INCLUDEPATH += ../src /usr/include /usr/include/taglib /usr/include/mpv

LIBS += -lmpv -ltag -lz

unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += libsecret-1
}

SOURCES += \
    main.cpp \
    tst_fuzzymatch.cpp \
    tst_audio_device_filter.cpp \
    tst_lrcparser.cpp \
    tst_playlist_m3u.cpp \
    tst_lyrics_parsers.cpp \
    tst_audio_formats.cpp \
    tst_library.cpp \
    tst_playback.cpp \
    tst_track_media.cpp \
    tst_playlists.cpp \
    tst_import.cpp \
    tst_convert.cpp \
    tst_tags.cpp \
    tst_lyrics_service.cpp \
    tst_discogs_rank.cpp \
    tst_metadata_live.cpp \
    tst_desktop_cli_theme.cpp \
    tst_format_matrix.cpp \
    ../src/core/FuzzyMatch.cpp \
    ../src/core/AudioDeviceFilter.cpp \
    ../src/core/LrcParser.cpp \
    ../src/core/PlaylistM3u.cpp \
    ../src/core/LyricsParsers.cpp \
    ../src/core/ConfigService.cpp \
    ../src/core/SecretsStore.cpp \
    ../src/core/LibraryService.cpp \
    ../src/core/PlaybackService.cpp \
    ../src/core/TrackMediaService.cpp \
    ../src/core/PlaylistService.cpp \
    ../src/core/TagService.cpp \
    ../src/core/DiscogsService.cpp \
    ../src/core/ImportService.cpp \
    ../src/core/ConvertService.cpp \
    ../src/core/OpusConvert.cpp \
    ../src/core/LyricsService.cpp \
    ../src/core/MetadataSearchService.cpp \
    ../src/core/OmarchyThemeService.cpp \
    ../src/mpris/MprisPlayer.cpp

HEADERS += \
    TestEnv.h \
    AudioFixture.h \
    ../src/core/AudioFormats.h \
    ../src/core/FuzzyMatch.h \
    ../src/core/AudioDeviceFilter.h \
    ../src/core/LrcParser.h \
    ../src/core/PlaylistM3u.h \
    ../src/core/LyricsParsers.h \
    ../src/core/ConfigService.h \
    ../src/core/SecretsStore.h \
    ../src/core/LibraryService.h \
    ../src/core/PlaybackService.h \
    ../src/core/TrackMediaService.h \
    ../src/core/PlaylistService.h \
    ../src/core/TagService.h \
    ../src/core/DiscogsService.h \
    ../src/core/ImportService.h \
    ../src/core/ConvertService.h \
    ../src/core/OpusConvert.h \
    ../src/core/LyricsService.h \
    ../src/core/MetadataSearchService.h \
    ../src/core/OmarchyThemeService.h \
    ../src/mpris/MprisPlayer.h
