QT += core gui qml quick quickcontrols2 dbus sql concurrent network
CONFIG += c++17
CONFIG -= app_bundle

TARGET = qt-music-bin
TEMPLATE = app

DEFINES += APP_ID=\\\"qt-music\\\"

SOURCES += \
    src/main.cpp \
    src/core/ConfigService.cpp \
    src/core/LibraryService.cpp \
    src/core/PlaybackService.cpp \
    src/core/AppController.cpp \
    src/core/TrackMediaService.cpp \
    src/core/ArtistMediaService.cpp \
    src/core/PlaylistService.cpp \
    src/core/TagService.cpp \
    src/core/DiscogsService.cpp \
    src/core/BeetsService.cpp \
    src/core/ImportService.cpp \
    src/core/LyricsService.cpp \
    src/core/MetadataSearchService.cpp \
    src/core/LibraryEnrichmentService.cpp \
    src/core/OmarchyThemeService.cpp \
    src/core/ThemeIconProvider.cpp \
    src/core/SecretsStore.cpp \
    src/core/LrcParser.cpp \
    src/core/PlaylistM3u.cpp \
    src/core/LyricsParsers.cpp \
    src/core/FuzzyMatch.cpp \
    src/models/PlaylistListModel.cpp \
    src/models/ArtistModel.cpp \
    src/models/TrackListModel.cpp \
    src/mpris/MprisPlayer.cpp

HEADERS += \
    src/core/ConfigService.h \
    src/core/LibraryService.h \
    src/core/PlaybackService.h \
    src/core/AppController.h \
    src/core/TrackMediaService.h \
    src/core/ArtistMediaService.h \
    src/core/PlaylistService.h \
    src/core/TagService.h \
    src/core/DiscogsService.h \
    src/core/BeetsService.h \
    src/core/ImportService.h \
    src/core/LyricsService.h \
    src/core/MetadataSearchService.h \
    src/core/LibraryEnrichmentService.h \
    src/core/OmarchyThemeService.h \
    src/core/ThemeIconProvider.h \
    src/core/SecretsStore.h \
    src/core/LrcParser.h \
    src/core/PlaylistM3u.h \
    src/core/LyricsParsers.h \
    src/core/FuzzyMatch.h \
    src/models/PlaylistListModel.h \
    src/models/ArtistModel.h \
    src/models/TrackListModel.h \
    src/mpris/MprisPlayer.h

RESOURCES += resources/qt-music.qrc

INCLUDEPATH += src /usr/include /usr/include/taglib

LIBS += -lmpv -ltag -lz

unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += libsecret-1
}

QML_IMPORT_PATH = src/ui/qml

isEmpty(PREFIX): PREFIX = /usr/local

unix {
    QMAKE_MKDIR = mkdir -p

    install.target = install
    install.commands = $(MKDIR) $(INSTALL_ROOT)$$PREFIX/lib/qt-music && rm -f $(INSTALL_ROOT)$$PREFIX/lib/qt-music/$(QMAKE_TARGET) && cp -f $(QMAKE_TARGET) $(INSTALL_ROOT)$$PREFIX/lib/qt-music/$(QMAKE_TARGET) && chmod 755 $(INSTALL_ROOT)$$PREFIX/lib/qt-music/$(QMAKE_TARGET) && $(MKDIR) $(INSTALL_ROOT)$$PREFIX/bin && rm -f $(INSTALL_ROOT)$$PREFIX/bin/qt-music && cp -f $$PWD/qt-music $(INSTALL_ROOT)$$PREFIX/bin/qt-music && chmod 755 $(INSTALL_ROOT)$$PREFIX/bin/qt-music && $(MKDIR) $(INSTALL_ROOT)$$PREFIX/share/applications && cp -f $$PWD/desktop/qt-music.desktop $(INSTALL_ROOT)$$PREFIX/share/applications/qt-music.desktop && sh $$PWD/desktop/fix-desktop-exec.sh $(INSTALL_ROOT)$$PREFIX/share/applications/qt-music.desktop $$PREFIX && $(MKDIR) $(INSTALL_ROOT)$$PREFIX/share/metainfo && cp -f $$PWD/desktop/org.jy0un9.qt-music.metainfo.xml $(INSTALL_ROOT)$$PREFIX/share/metainfo/org.jy0un9.qt-music.metainfo.xml && $(MKDIR) $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/scalable/apps && cp -f $$PWD/desktop/icons/hicolor/scalable/apps/qt-music.svg $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/scalable/apps/qt-music.svg && $(MKDIR) $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/256x256/apps && cp -f $$PWD/desktop/icons/hicolor/256x256/apps/qt-music.png $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/256x256/apps/qt-music.png && $(MKDIR) $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/128x128/apps && cp -f $$PWD/desktop/icons/hicolor/128x128/apps/qt-music.png $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/128x128/apps/qt-music.png
    install.depends = first
    QMAKE_EXTRA_TARGETS += install

    uninstall.target = uninstall
    uninstall.commands = -$(DEL_FILE) $(INSTALL_ROOT)$$PREFIX/lib/qt-music/$(QMAKE_TARGET) $(INSTALL_ROOT)$$PREFIX/bin/qt-music $(INSTALL_ROOT)$$PREFIX/share/applications/qt-music.desktop $(INSTALL_ROOT)$$PREFIX/share/metainfo/org.jy0un9.qt-music.metainfo.xml $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/scalable/apps/qt-music.svg $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/256x256/apps/qt-music.png $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/128x128/apps/qt-music.png
    QMAKE_EXTRA_TARGETS += uninstall
}
