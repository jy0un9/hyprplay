import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import components 1.0

ApplicationWindow {
    id: root
    width: 1280
    height: 800
    visible: true
    title: "Qt Music"
    color: Theme.background
    font.family: App.config.uiFontFamily

    onClosing: root.persistAllLayout()

    function applyAllLayout() {
        sidebar.collapsed = App.config.layoutSidebarCollapsed
        sidebarPane.applyWidth()
        sidePanelPane.applyWidth()
        nowPlayingPane.paneHeight = App.config.layoutNowPlayingHeight
    }

    function syncLayoutWidths() {
        App.config.setLayoutSidebarCollapsed(sidebar.collapsed)
        if (!sidebar.collapsed)
            App.config.setLayoutSidebarWidth(Math.round(sidebarPane.width))
        if (root.showingSidePanel && sidePanelPane.width > 0)
            App.config.setLayoutSidePanelWidth(Math.round(sidePanelPane.width))
        App.config.setLayoutNowPlayingHeight(Math.round(nowPlayingPane.height))
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

    Material.theme: Theme.dark ? Material.Dark : Material.Light
    Material.primary: Theme.accent
    Material.accent: Theme.accent
    Material.background: Theme.background
    Material.foreground: Theme.foreground

    readonly property bool showingNowPlaying: App.playback.currentPath.length > 0
    readonly property bool showingArtistPanel: !showingNowPlaying && App.selectedArtist.length > 0
                                              && App.mainView === "library"
    readonly property bool showingSidePanel: showingNowPlaying || showingArtistPanel

    Component.onCompleted: Qt.callLater(function() {
        applyAllLayout()
        Qt.callLater(applyAllLayout)
    })

    Connections {
        target: sidebar
        function onCollapsedChanged() {
            sidebarPane.applyWidth()
        }
    }

    Connections {
        target: root
        function onShowingSidePanelChanged() {
            sidePanelPane.applyWidth()
        }
    }

    component SplitDragHandle: Rectangle {
        id: dragHandle
        required property var targetSplit
        property color idleColor: Theme.rgba(Theme.border, 0.35)
        property color hoverColor: Theme.rgba(Theme.accent, 0.55)

        implicitWidth: 5
        implicitHeight: 5
        color: splitHover.hovered ? hoverColor : idleColor

        HoverHandler { id: splitHover }

        MouseArea {
            anchors.fill: parent
            propagateComposedEvents: true
            onPressed: (mouse) => mouse.accepted = false
            onReleased: root.syncLayoutWidths()
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
        sequence: "Space"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (!root.textInputFocused())
                App.playback.togglePlayPause()
        }
    }

    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: App.librarySearchOpen
        onActivated: App.closeLibrarySearch()
    }

    SplitView {
        id: mainSplit
        anchors.fill: parent
        orientation: Qt.Vertical

        handle: SplitDragHandle {
            implicitHeight: 5
            targetSplit: mainSplit
        }

        Item {
            SplitView.fillHeight: true
            SplitView.minimumHeight: 240

            SplitView {
                id: contentSplit
                anchors.fill: parent
                orientation: Qt.Horizontal

                handle: SplitDragHandle {
                    implicitWidth: 5
                    targetSplit: contentSplit
                }

            Item {
                id: sidebarPane
                property real paneWidth: 208

                function applyWidth() {
                    paneWidth = sidebar.collapsed ? sidebar.collapsedWidth : App.config.layoutSidebarWidth
                }

                SplitView.preferredWidth: paneWidth
                SplitView.minimumWidth: sidebar.collapsed ? sidebar.collapsedWidth : 168
                SplitView.maximumWidth: sidebar.collapsed ? sidebar.collapsedWidth : 360

                Sidebar {
                    id: sidebar
                    anchors.fill: parent
                }
            }

            Item {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 320

                Loader {
                    id: libraryLoader
                    anchors.fill: parent
                    active: true
                    visible: App.mainView === "library"
                    source: "qrc:/views/LibraryView.qml"
                    onLoaded: if (item && item.applyLayout)
                                  item.applyLayout()
                }

                Loader {
                    id: playlistsLoader
                    anchors.fill: parent
                    active: true
                    visible: App.mainView === "playlists"
                    source: "qrc:/views/PlaylistsView.qml"
                    onLoaded: if (item && item.applyLayout)
                                  item.applyLayout()
                }

                Loader {
                    anchors.fill: parent
                    active: true
                    visible: App.mainView === "settings"
                    source: "qrc:/views/SettingsView.qml"
                }

                Loader {
                    anchors.fill: parent
                    active: true
                    visible: App.mainView === "import"
                    source: "qrc:/views/ImportView.qml"
                }
            }

            Item {
                id: sidePanelPane
                property real paneWidth: 0

                function applyWidth() {
                    paneWidth = root.showingSidePanel ? App.config.layoutSidePanelWidth : 0
                }

                SplitView.preferredWidth: paneWidth
                SplitView.minimumWidth: root.showingSidePanel ? 220 : 0
                SplitView.maximumWidth: root.showingSidePanel ? 480 : 0
                visible: root.showingSidePanel

                Loader {
                    anchors.fill: parent
                    source: root.showingNowPlaying
                            ? "qrc:/components/NowPlayingPanel.qml"
                            : "qrc:/components/ArtistPanel.qml"
                }
            }
            }
        }

        Item {
            id: nowPlayingPane
            property real paneHeight: 108

            SplitView.preferredHeight: paneHeight
            SplitView.minimumHeight: 108
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
}
