QT += core testlib
CONFIG += console c++17 testcase
CONFIG -= app_bundle

TARGET = hyprplay-tests
TEMPLATE = app

INCLUDEPATH += ../src

SOURCES += \
    main.cpp \
    tst_fuzzymatch.cpp \
    tst_lrcparser.cpp \
    tst_playlist_m3u.cpp \
    tst_lyrics_parsers.cpp \
    ../src/core/FuzzyMatch.cpp \
    ../src/core/LrcParser.cpp \
    ../src/core/PlaylistM3u.cpp \
    ../src/core/LyricsParsers.cpp

HEADERS += \
    ../src/core/FuzzyMatch.h \
    ../src/core/LrcParser.h \
    ../src/core/PlaylistM3u.h \
    ../src/core/LyricsParsers.h
