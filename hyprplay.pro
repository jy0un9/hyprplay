QT += core gui qml quick quickcontrols2 dbus sql concurrent network
CONFIG += c++17
CONFIG -= app_bundle

TARGET = hyprplay-bin
TEMPLATE = app

DEFINES += APP_ID=\\\"hyprplay\\\"

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
    src/core/ImportService.cpp \
    src/core/ConvertService.cpp \
    src/core/OpusConvert.cpp \
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
    src/core/ImportService.h \
    src/core/ConvertService.h \
    src/core/OpusConvert.h \
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
    src/core/AudioFormats.h \
    src/models/PlaylistListModel.h \
    src/models/ArtistModel.h \
    src/models/TrackListModel.h \
    src/mpris/MprisPlayer.h

RESOURCES += resources/hyprplay.qrc

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
    install.commands = $(MKDIR) $(INSTALL_ROOT)$$PREFIX/lib/hyprplay && rm -f $(INSTALL_ROOT)$$PREFIX/lib/hyprplay/$(QMAKE_TARGET) && cp -f $(QMAKE_TARGET) $(INSTALL_ROOT)$$PREFIX/lib/hyprplay/$(QMAKE_TARGET) && chmod 755 $(INSTALL_ROOT)$$PREFIX/lib/hyprplay/$(QMAKE_TARGET) && $(MKDIR) $(INSTALL_ROOT)$$PREFIX/bin && rm -f $(INSTALL_ROOT)$$PREFIX/bin/hyprplay && cp -f $$PWD/hyprplay $(INSTALL_ROOT)$$PREFIX/bin/hyprplay && chmod 755 $(INSTALL_ROOT)$$PREFIX/bin/hyprplay && $(MKDIR) $(INSTALL_ROOT)$$PREFIX/share/applications && cp -f $$PWD/desktop/hyprplay.desktop $(INSTALL_ROOT)$$PREFIX/share/applications/hyprplay.desktop && sh $$PWD/desktop/fix-desktop-exec.sh $(INSTALL_ROOT)$$PREFIX/share/applications/hyprplay.desktop $$PREFIX && $(MKDIR) $(INSTALL_ROOT)$$PREFIX/share/metainfo && cp -f $$PWD/desktop/org.jy0un9.hyprplay.metainfo.xml $(INSTALL_ROOT)$$PREFIX/share/metainfo/org.jy0un9.hyprplay.metainfo.xml && $(MKDIR) $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/scalable/apps && cp -f $$PWD/desktop/icons/hicolor/scalable/apps/hyprplay.svg $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/scalable/apps/hyprplay.svg && $(MKDIR) $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/256x256/apps && cp -f $$PWD/desktop/icons/hicolor/256x256/apps/hyprplay.png $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/256x256/apps/hyprplay.png && $(MKDIR) $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/128x128/apps && cp -f $$PWD/desktop/icons/hicolor/128x128/apps/hyprplay.png $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/128x128/apps/hyprplay.png
    install.depends = first
    QMAKE_EXTRA_TARGETS += install

    uninstall.target = uninstall
    uninstall.commands = -$(DEL_FILE) $(INSTALL_ROOT)$$PREFIX/lib/hyprplay/$(QMAKE_TARGET) $(INSTALL_ROOT)$$PREFIX/bin/hyprplay $(INSTALL_ROOT)$$PREFIX/share/applications/hyprplay.desktop $(INSTALL_ROOT)$$PREFIX/share/metainfo/org.jy0un9.hyprplay.metainfo.xml $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/scalable/apps/hyprplay.svg $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/256x256/apps/hyprplay.png $(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/128x128/apps/hyprplay.png
    QMAKE_EXTRA_TARGETS += uninstall
}
