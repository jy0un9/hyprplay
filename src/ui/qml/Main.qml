import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import components 1.0

ApplicationWindow {
    id: root
    width: 1280
    height: 800
    minimumWidth: 1024
    minimumHeight: 640
    visible: true
    title: "Qt Music"
    color: Theme.background
    font.family: App.config.uiFontFamily
    font.pixelSize: App.config.uiFontSize

    onClosing: root.persistAllLayout()

    function applyAllLayout() {
        sidePanelPane.applyWidth()
        nowPlayingPane.paneHeight = Math.max(128, App.config.layoutNowPlayingHeight)
    }

    function syncLayoutWidths() {
        if (root.showingSidePanel && sidePanelPane.width > 0)
            App.config.setLayoutSidePanelWidth(Math.round(sidePanelPane.width))
        App.config.setLayoutNowPlayingHeight(Math.max(128, Math.round(nowPlayingPane.height)))
    }

    function persistAllLayout() {
        syncLayoutWidths()
        if (libraryLoader.item && libraryLoader.item.persistLayout)
            libraryLoader.item.persistLayout()
        if (playlistsLoader.item && playlistsLoader.item.persistLayout)
            playlistsLoader.item.persistLayout()
    }

    function textInputFocused() {
        let item = root.activeFocusItem
        while (item) {
            if (item instanceof TextField || item instanceof TextArea)
                return true
            item = item.parent
        }
        return false
    }

    function steppingFocused() {
        let item = root.activeFocusItem
        while (item) {
            if (item instanceof TextField || item instanceof TextArea
                || item instanceof Slider || item instanceof SpinBox
                || item instanceof ComboBox)
                return true
            item = item.parent
        }
        return false
    }

    function playSelected() {
        if (App.mainView === "library" && libraryLoader.item && libraryLoader.item.playSelected)
            libraryLoader.item.playSelected()
        else if (App.mainView === "playlists" && playlistsLoader.item && playlistsLoader.item.playSelected)
            playlistsLoader.item.playSelected()
    }

    Material.theme: Theme.dark ? Material.Dark : Material.Light
    Material.primary: Theme.accent
    Material.accent: Theme.accent
    Material.background: Theme.background
    Material.foreground: Theme.foreground

    readonly property bool showingNowPlaying: App.nowPlayingFocused
                                              && App.playback.currentPath.length > 0
    readonly property bool showingAlbumPanel: !showingNowPlaying
                                              && App.selectedAlbum.length > 0
                                              && App.mainView === "library"
    readonly property bool showingArtistPanel: !showingNowPlaying
                                              && !showingAlbumPanel
                                              && App.selectedArtist.length > 0
                                              && App.mainView === "library"
    readonly property bool showingSidePanel: showingNowPlaying || showingAlbumPanel
                                             || showingArtistPanel

    Component.onCompleted: Qt.callLater(function() {
        applyAllLayout()
        Qt.callLater(applyAllLayout)
    })

    Connections {
        target: root
        function onShowingSidePanelChanged() {
            sidePanelPane.applyWidth()
        }
    }

    Shortcut {
        sequence: "/"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (App.mainView === "library")
                App.openLibrarySearch()
        }
    }

    Shortcut {
        sequences: [StandardKey.Find, "Ctrl+K"]
        context: Qt.ApplicationShortcut
        onActivated: {
            if (App.mainView === "library")
                App.openLibrarySearch()
        }
    }

    Shortcut {
        sequence: "Space"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.textInputFocused())
                App.playback.togglePlayPause()
        }
    }

    Shortcut {
        sequence: "Up"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.textInputFocused() && App.mainView === "library")
                App.libraryMoveUp()
        }
    }

    Shortcut {
        sequence: "Down"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.textInputFocused() && App.mainView === "library")
                App.libraryMoveDown()
        }
    }

    Shortcut {
        sequence: "Left"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.steppingFocused() && App.mainView === "library")
                App.libraryMoveLeft()
        }
    }

    Shortcut {
        sequence: "Right"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.steppingFocused() && App.mainView === "library")
                App.libraryMoveRight()
        }
    }

    Shortcut {
        sequences: ["PgUp", "Shift+Left"]
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.steppingFocused() && App.playback.duration > 0)
                App.playback.seekRelative(-App.config.seekStepSecs)
        }
    }

    Shortcut {
        sequences: ["PgDown", "Shift+Right"]
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.steppingFocused() && App.playback.duration > 0)
                App.playback.seekRelative(App.config.seekStepSecs)
        }
    }

    Shortcut {
        sequence: "w"
        enabled: App.config.wasdNavigation
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.textInputFocused() && App.mainView === "library")
                App.libraryMoveUp()
        }
    }

    Shortcut {
        sequence: "s"
        enabled: App.config.wasdNavigation
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.textInputFocused() && App.mainView === "library")
                App.libraryMoveDown()
        }
    }

    Shortcut {
        sequence: "a"
        enabled: App.config.wasdNavigation
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.steppingFocused() && App.mainView === "library")
                App.libraryMoveLeft()
        }
    }

    Shortcut {
        sequence: "d"
        enabled: App.config.wasdNavigation
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.steppingFocused() && App.mainView === "library")
                App.libraryMoveRight()
        }
    }

    Shortcut {
        sequence: "j"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.textInputFocused())
                App.playback.next()
        }
    }

    Shortcut {
        sequence: "k"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.textInputFocused())
                App.playback.previous()
        }
    }

    Shortcut {
        sequence: "Return"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.textInputFocused())
                root.playSelected()
        }
    }

    Shortcut {
        sequence: "Enter"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.textInputFocused())
                root.playSelected()
        }
    }

    Shortcut {
        sequence: "Delete"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.textInputFocused() && App.mainView === "playlists"
                && playlistsLoader.item && playlistsLoader.item.removeCurrentPlaylistTrack)
                playlistsLoader.item.removeCurrentPlaylistTrack()
        }
    }

    Shortcut {
        sequence: "Ctrl+,"
        context: Qt.ApplicationShortcut
        onActivated: App.showSettings()
    }

    Shortcut {
        sequence: "?"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.textInputFocused())
                helpDialog.open()
        }
    }

    Shortcut {
        sequence: "Ctrl+N"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.textInputFocused()) {
                if (App.mainView !== "playlists")
                    App.showPlaylists()
                if (playlistsLoader.item && playlistsLoader.item.focusNewPlaylistField)
                    playlistsLoader.item.focusNewPlaylistField()
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: App.librarySearchOpen || helpDialog.visible
        onActivated: {
            if (helpDialog.visible)
                helpDialog.close()
            else
                App.closeLibrarySearch()
        }
    }

    SplitView {
        id: mainSplit
        anchors.fill: parent
        orientation: Qt.Vertical

        handle: SplitHandle {
            orientation: Qt.Vertical
            onReleased: root.syncLayoutWidths()
        }

        Item {
            SplitView.fillHeight: true
            SplitView.minimumHeight: 240

            SplitView {
                id: contentSplit
                anchors.fill: parent
                orientation: Qt.Horizontal

                handle: SplitHandle {
                    orientation: Qt.Horizontal
                    onReleased: root.syncLayoutWidths()
                }

             Item {
                 SplitView.fillWidth: true
                 SplitView.minimumWidth: 320

                 ColumnLayout {
                     anchors.fill: parent
                     spacing: 0

                     Rectangle {
                         id: topBar
                         Layout.fillWidth: true
                         Layout.preferredHeight: 52
                         color: Theme.darkBackground
                         border.color: Theme.rgba(Theme.border, 0.35)
                         border.width: 1

                         RowLayout {
                             id: navRow
                             anchors.fill: parent
                             anchors.leftMargin: Theme.spaceLg
                             anchors.rightMargin: Theme.spaceLg
                             spacing: Theme.spaceXs

                             Label {
                                 text: "Qt Music"
                                 font.pixelSize: Theme.fontHeading
                                 font.bold: true
                                 color: Theme.foreground
                                 Layout.rightMargin: Theme.spaceLg + 2
                             }

                             NavButton {
                                 id: browseTab
                                 text: "Browse"
                                 navHighlighted: App.mainView === "library"
                                 onClicked: App.showLibrary()
                             }

                             NavButton {
                                 id: playlistsTab
                                 text: "Playlists"
                                 navHighlighted: App.mainView === "playlists"
                                 onClicked: App.showPlaylists()
                             }

                             NavButton {
                                 id: importTab
                                 text: "Import"
                                 navHighlighted: App.mainView === "import"
                                 onClicked: App.showImport()
                             }

                             NavButton {
                                 id: settingsTab
                                 text: "Settings"
                                 navHighlighted: App.mainView === "settings"
                                 onClicked: App.showSettings()
                             }

                             Item { Layout.fillWidth: true }

                             Label {
                                 text: App.library.trackCount + " tracks  ·  "
                                       + App.playlists.playlistCount + " playlists"
                                 opacity: 0.65
                                 color: Theme.foreground
                                 font.pixelSize: Theme.fontCaption
                                 elide: Text.ElideRight
                             }

                             Label {
                                 text: App.scanStatus
                                 visible: text.length > 0
                                 opacity: 0.45
                                 color: Theme.muted
                                 font.pixelSize: Theme.fontCaption
                                 elide: Text.ElideRight
                                 Layout.maximumWidth: 220
                             }
                         }

                         Rectangle {
                             z: 2
                             x: navRow.x + (App.mainView === "library"
                                             ? browseTab.x
                                             : App.mainView === "playlists"
                                               ? playlistsTab.x
                                               : App.mainView === "import"
                                                 ? importTab.x : settingsTab.x)
                             width: App.mainView === "library"
                                    ? browseTab.width
                                    : App.mainView === "playlists"
                                      ? playlistsTab.width
                                      : App.mainView === "import"
                                        ? importTab.width : settingsTab.width
                             height: 2
                             y: topBar.height - height
                             color: Theme.accent

                             Behavior on x {
                                 NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                             }
                             Behavior on width {
                                 NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                             }
                         }
                     }

                     Loader {
                         id: libraryLoader
                         Layout.fillWidth: true
                         Layout.fillHeight: true
                         active: true
                         visible: App.mainView === "library"
                         source: "qrc:/views/LibraryView.qml"
                         onLoaded: if (item && item.applyLayout)
                                       item.applyLayout()
                     }

                     Loader {
                         id: playlistsLoader
                         Layout.fillWidth: true
                         Layout.fillHeight: true
                         active: true
                         visible: App.mainView === "playlists"
                         source: "qrc:/views/PlaylistsView.qml"
                         onLoaded: if (item && item.applyLayout)
                                       item.applyLayout()
                     }

                     Loader {
                         id: importLoader
                         Layout.fillWidth: true
                         Layout.fillHeight: true
                         active: true
                         visible: App.mainView === "import"
                         source: "qrc:/views/ImportView.qml"
                         onStatusChanged: {
                             if (status === Loader.Error)
                                 console.error("ImportView failed to load")
                         }
                     }

                     Loader {
                         id: settingsLoader
                         Layout.fillWidth: true
                         Layout.fillHeight: true
                         active: true
                         visible: App.mainView === "settings"
                         source: "qrc:/views/SettingsView.qml"
                         onStatusChanged: {
                             if (status === Loader.Error)
                                 console.error("SettingsView failed to load")
                         }
                     }
                 }
             }

            Item {
                id: sidePanelPane
                property real paneWidth: 0

                function applyWidth() {
                    paneWidth = root.showingSidePanel ? App.config.layoutSidePanelWidth : 0
                }

                SplitView.preferredWidth: paneWidth
                SplitView.minimumWidth: 0
                SplitView.maximumWidth: 480
                visible: root.showingSidePanel

                Loader {
                    anchors.fill: parent
                    source: root.showingNowPlaying
                            ? "qrc:/components/NowPlayingPanel.qml"
                            : root.showingAlbumPanel
                              ? "qrc:/components/AlbumPanel.qml"
                              : "qrc:/components/ArtistPanel.qml"
                }
            }
            }
        }

        Item {
            id: nowPlayingPane
             property real paneHeight: 128

             SplitView.preferredHeight: paneHeight
             SplitView.minimumHeight: 128
            SplitView.maximumHeight: 240

            Loader {
                anchors.fill: parent
                source: "qrc:/components/NowPlayingBar.qml"
                onStatusChanged: {
                    if (status === Loader.Error)
                        console.error("NowPlayingBar failed to load")
                }
            }
        }
    }

    Loader {
        active: true
        source: "qrc:/components/TagEditorDialog.qml"
    }

    Loader {
        active: true
        source: "qrc:/components/TagFetchDialog.qml"
    }

    Loader {
        active: true
        source: "qrc:/components/DiscogsAlbumDialog.qml"
    }

    Snackbar {
        z: 10
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 152
        message: App.noticeText
        kind: App.noticeKind
        serial: App.noticeSerial
    }

    Dialog {
        id: helpDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        title: "Keyboard shortcuts"
        modal: true
        standardButtons: Dialog.Close

        contentItem: GridLayout {
            columns: 2
            columnSpacing: Theme.spaceLg
            rowSpacing: Theme.spaceXs

            Label { text: "/, Ctrl+F, Ctrl+K"; font.bold: true; color: Theme.foreground }
            Label { text: "Open library search"; color: Theme.foreground }
            Label { text: "Tab (in search)"; font.bold: true; color: Theme.foreground }
            Label { text: "Cycle search scope"; color: Theme.foreground }
            Label { text: "Space"; font.bold: true; color: Theme.foreground }
            Label { text: "Play / pause"; color: Theme.foreground }
            Label { text: "↑ / ↓"; font.bold: true; color: Theme.foreground }
            Label { text: "Move in artists / albums list"; color: Theme.foreground }
            Label { text: "← / →"; font.bold: true; color: Theme.foreground }
            Label { text: "Back / forward (drill in-out)"; color: Theme.foreground }
            Label { text: "PgUp / PgDn"; font.bold: true; color: Theme.foreground }
            Label { text: "Seek backward / forward"; color: Theme.foreground }
            Label { text: "W/A/S/D (optional)"; font.bold: true; color: Theme.foreground }
            Label { text: "Same as arrows when enabled in Settings"; color: Theme.foreground }
            Label { text: "J / K"; font.bold: true; color: Theme.foreground }
            Label { text: "Next / previous track"; color: Theme.foreground }
            Label { text: "Enter"; font.bold: true; color: Theme.foreground }
            Label { text: "Play selected track"; color: Theme.foreground }
            Label { text: "Delete (playlists)"; font.bold: true; color: Theme.foreground }
            Label { text: "Remove playlist track"; color: Theme.foreground }
            Label { text: "Ctrl+N"; font.bold: true; color: Theme.foreground }
            Label { text: "New playlist"; color: Theme.foreground }
            Label { text: "Ctrl+,"; font.bold: true; color: Theme.foreground }
            Label { text: "Open settings"; color: Theme.foreground }
            Label { text: "?"; font.bold: true; color: Theme.foreground }
            Label { text: "This help"; color: Theme.foreground }
            Label { text: "Esc"; font.bold: true; color: Theme.foreground }
            Label { text: "Close dialog / search"; color: Theme.foreground }
        }
    }
}
