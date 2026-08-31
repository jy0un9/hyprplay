import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Pane {
    id: sidebar
    padding: sidebar.collapsed ? 8 : 12

    property bool collapsed: false
    property int lastExpandedWidth: App.config.layoutSidebarWidth
    readonly property int effectiveWidth: collapsed ? collapsedWidth : expandedWidth

    readonly property int expandedWidth: App.config.layoutSidebarWidth
    readonly property int collapsedWidth: 56

    function toggleCollapsed() {
        if (!collapsed) {
            lastExpandedWidth = Math.max(168, Math.round(width))
            App.config.setLayoutSidebarWidth(lastExpandedWidth)
        }
        collapsed = !collapsed
        App.config.setLayoutSidebarCollapsed(collapsed)
    }

    Component.onCompleted: collapsed = App.config.layoutSidebarCollapsed

    Material.theme: Theme.dark ? Material.Dark : Material.Light
    Material.background: Theme.darkBackground
    Material.foreground: Theme.foreground
    Material.primary: Theme.accent
    Material.accent: Theme.accent

    background: Rectangle {
        color: Theme.darkBackground
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: sidebar.collapsed ? 6 : 8

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 36

            Label {
                text: "Qt Music"
                font.pixelSize: 17
                font.bold: true
                color: Theme.foreground
                Layout.fillWidth: true
                visible: !sidebar.collapsed
                elide: Text.ElideRight
            }

            ToolButton {
                Layout.alignment: sidebar.collapsed ? Qt.AlignHCenter : Qt.AlignRight
                Layout.fillWidth: sidebar.collapsed
                text: sidebar.collapsed ? "»" : "«"
                onClicked: sidebar.toggleCollapsed()
            }
        }

        Label {
            text: App.scanStatus
            font.pixelSize: 11
            opacity: 0.65
            color: Theme.foreground
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            visible: !sidebar.collapsed && App.scanStatus.length > 0
        }

        Button {
            text: "Rescan Library"
            Layout.fillWidth: true
            visible: !sidebar.collapsed
            flat: true
            onClicked: App.rescanLibrary()
        }

        ToolButton {
            Layout.alignment: Qt.AlignHCenter
            text: "↻"
            visible: sidebar.collapsed
            onClicked: App.rescanLibrary()
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.rgba(Theme.border, 0.35)
        }

        Button {
            text: "Browse"
            Layout.fillWidth: true
            visible: !sidebar.collapsed
            flat: App.mainView !== "library"
            highlighted: App.mainView === "library"
            onClicked: App.showLibrary()
        }

        Button {
            text: "Playlists"
            Layout.fillWidth: true
            visible: !sidebar.collapsed
            flat: App.mainView !== "playlists"
            highlighted: App.mainView === "playlists"
            onClicked: App.showPlaylists()
        }

        Button {
            text: "Import"
            Layout.fillWidth: true
            visible: !sidebar.collapsed
            flat: App.mainView !== "import"
            highlighted: App.mainView === "import"
            onClicked: App.showImport()
        }

        Button {
            text: "Settings"
            Layout.fillWidth: true
            visible: !sidebar.collapsed
            flat: App.mainView !== "settings"
            highlighted: App.mainView === "settings"
            onClicked: App.showSettings()
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            visible: sidebar.collapsed

            ToolButton {
                Layout.alignment: Qt.AlignHCenter
                text: "B"
                highlighted: App.mainView === "library"
                onClicked: App.showLibrary()
            }
            ToolButton {
                Layout.alignment: Qt.AlignHCenter
                text: "P"
                highlighted: App.mainView === "playlists"
                onClicked: App.showPlaylists()
            }
            ToolButton {
                Layout.alignment: Qt.AlignHCenter
                text: "I"
                highlighted: App.mainView === "import"
                onClicked: App.showImport()
            }
            ToolButton {
                Layout.alignment: Qt.AlignHCenter
                text: "S"
                highlighted: App.mainView === "settings"
                onClicked: App.showSettings()
            }
        }

        Item { Layout.fillHeight: true }

        Label {
            text: {
                if (App.mainView === "playlists") return App.playlists.playlistCount + " playlists"
                if (App.mainView === "import") return App.importInbox.albums.length + " inbox albums"
                if (App.mainView === "settings") return "Settings"
                return App.library.trackCount + " tracks"
            }
            opacity: 0.5
            font.pixelSize: 11
            color: Theme.foreground
            visible: !sidebar.collapsed
        }
    }
}
