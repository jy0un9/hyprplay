import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import components 1.0

Pane {
    id: playlistsView
    padding: 0

    function applyLayout() {
        playlistListPane.paneWidth = App.config.layoutPlaylistsListWidth
    }

    function playSelected() {
        if (App.playlistFocusColumn === "tracks" && App.playlistTracks.count > 0) {
            const index = App.selectedPlaylistTrackIndex >= 0
                          ? App.selectedPlaylistTrackIndex : 0
            App.playPlaylistTrackIndex(index)
            return
        }
        if (App.playlists.selectedPlaylist.length > 0)
            App.playPlaylist()
    }

    function handleDelete() {
        if (App.playlistFocusColumn === "playlists") {
            if (App.playlists.selectedPlaylist.length > 0)
                deleteDialog.open()
            return
        }
        if (App.selectedPlaylistTrackIndex >= 0 && App.playlistTracks.count > 0)
            App.removePlaylistTrack(App.selectedPlaylistTrackIndex)
    }

    function focusNewPlaylistField() {
        createDialog.open()
    }

    function persistLayout() {
        App.config.setLayoutPlaylistsListWidth(Math.round(playlistListPane.width))
    }

    function ensurePlaylistSelectionVisible() {
        const names = App.playlists.playlistNames
        const idx = names.indexOf(App.playlists.selectedPlaylist)
        if (idx >= 0)
            playlistList.positionViewAtIndex(idx, ListView.Contain)
    }

    function ensureTrackSelectionVisible() {
        if (App.selectedPlaylistTrackIndex >= 0 && App.selectedPlaylistTrackIndex < trackList.count)
            trackList.positionViewAtIndex(App.selectedPlaylistTrackIndex, ListView.Contain)
    }

    Component.onCompleted: Qt.callLater(applyLayout)

    background: Rectangle { color: Theme.background }

    property string selectedPlaylist: App.playlists.selectedPlaylist

    AdaptiveMenu {
        id: playlistContextMenu
        property string playlistName: ""

        ThemedMenuItem {
            text: "Rename playlist"
            onTriggered: {
                App.setPlaylistFocusColumn("playlists")
                App.selectPlaylist(playlistContextMenu.playlistName)
                renameDialog.open()
            }
        }
        MenuSeparator {}
        ThemedMenuItem {
            text: "Delete playlist"
            onTriggered: {
                App.setPlaylistFocusColumn("playlists")
                App.selectPlaylist(playlistContextMenu.playlistName)
                deleteDialog.open()
            }
        }
    }

    AdaptiveMenu {
        id: trackContextMenu
        property int trackIndex: -1

        ThemedMenuItem {
            text: "Remove from playlist"
            enabled: trackContextMenu.trackIndex >= 0
            onTriggered: App.removePlaylistTrack(trackContextMenu.trackIndex)
        }
    }

    Connections {
        target: App
        function onPlaylistFocusChanged() {
            if (App.playlistFocusColumn === "playlists")
                ensurePlaylistSelectionVisible()
            else
                ensureTrackSelectionVisible()
        }
    }

    SplitView {
        id: playlistSplit
        anchors.fill: parent
        orientation: Qt.Horizontal
        onResizingChanged: {
            if (!resizing)
                playlistsView.persistLayout()
        }

        handle: SplitHandle {
            orientation: Qt.Horizontal
        }

        Item {
            id: playlistListPane
            property real paneWidth: 260
            SplitView.preferredWidth: paneWidth
            SplitView.minimumWidth: 180
            SplitView.maximumWidth: 420

            onWidthChanged: {
                if (width >= SplitView.minimumWidth && width <= SplitView.maximumWidth)
                    App.config.setLayoutPlaylistsListWidth(Math.round(width))
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spaceSm

                Label {
                    text: "Playlists"
                    font.bold: true
                    font.pixelSize: Theme.fontTitle
                    color: Theme.foreground
                    Layout.leftMargin: Theme.spaceMd
                    Layout.topMargin: Theme.spaceMd
                }

                Label {
                    text: App.playlists.status
                    font.pixelSize: Theme.fontCaption
                    opacity: 0.65
                    color: Theme.foreground
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.spaceMd
                    Layout.rightMargin: Theme.spaceMd
                }

                EmptyState {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    iconName: "media-playlist-consecutive-symbolic"
                    title: "No playlists yet"
                    subtitle: "Create one above to get started."
                    visible: playlistList.count === 0
                }

                ListView {
                    id: playlistList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: App.playlistItems
                    visible: count > 0
                    currentIndex: {
                        const names = App.playlists.playlistNames
                        return names.indexOf(App.playlists.selectedPlaylist)
                    }

                    delegate: AppListDelegate {
                        id: playlistDelegate
                        width: playlistList.width
                        text: model.name
                        accessibleName: model.name + ", " + model.trackCount + " tracks"
                        highlighted: playlistsView.selectedPlaylist === model.name
                        accented: App.playlistFocusColumn === "playlists"
                                  && playlistsView.selectedPlaylist === model.name
                        onClicked: {
                            App.setPlaylistFocusColumn("playlists")
                            App.selectPlaylist(model.name)
                        }

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: {
                                playlistContextMenu.playlistName = model.name
                                playlistContextMenu.popup()
                            }
                        }

                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onDoubleTapped: {
                                App.setPlaylistFocusColumn("playlists")
                                App.selectPlaylist(model.name)
                                App.playPlaylist()
                            }
                        }

                        contentItem: RowLayout {
                            spacing: Theme.spaceSm

                            Label {
                                text: "♪"
                                color: playlistDelegate.highlighted ? Theme.accent : Theme.foreground
                                opacity: playlistDelegate.highlighted ? 1 : 0.6
                                font.pixelSize: Theme.fontSubtitle
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1

                                Label {
                                    id: playlistNameLabel
                                    text: playlistDelegate.text
                                    font: playlistDelegate.font
                                    color: Theme.foreground
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                    HoverHandler { id: playlistNameHover }
                                    ElisionPopup {
                                        visible: playlistNameHover.hovered && playlistNameLabel.truncated
                                        text: playlistDelegate.text
                                    }

                                }

                                Label {
                                    text: model.trackCount + " tracks"
                                    font.pixelSize: Theme.fontCaption
                                    opacity: 0.55
                                    color: Theme.foreground
                                }
                            }
                        }
                    }
                }

                PrimaryButton {
                    text: "Create playlist"
                    Layout.fillWidth: true
                    Layout.margins: Theme.spaceSm
                    onClicked: createDialog.open()
                }
            }
        }

        Item {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 240

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spaceSm

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.spaceMd
                    Layout.leftMargin: Theme.spaceMd
                    Layout.rightMargin: Theme.spaceMd
                    visible: playlistsView.selectedPlaylist.length > 0

                    Label {
                        id: selectedPlaylistLabel
                        text: playlistsView.selectedPlaylist
                        font.pixelSize: Theme.fontHeading
                        font.bold: true
                        color: Theme.foreground
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        HoverHandler { id: selectedPlaylistHover }
                        ElisionPopup {
                            visible: selectedPlaylistHover.hovered && selectedPlaylistLabel.truncated
                            text: selectedPlaylistLabel.text
                        }

                    }

                    Label {
                        text: playlistTrackCount() + " tracks"
                        font.pixelSize: Theme.fontCaption
                        opacity: 0.6
                        color: Theme.foreground
                    }
                }

                ListView {
                    id: trackList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: App.playlistTracks
                    visible: playlistsView.selectedPlaylist.length > 0
                    currentIndex: App.selectedPlaylistTrackIndex
                    property int draggingIndex: -1
                    property int dropIndex: -1

                    function indexAtContentY(contentY) {
                        const x = Math.min(48, width * 0.5)
                        let idx = indexAt(x, contentY)
                        if (idx < 0 && count > 0) {
                            if (contentY <= 0)
                                idx = 0
                            else
                                idx = count - 1
                        }
                        return idx
                    }

                    onCurrentIndexChanged: {
                        if (draggingIndex >= 0)
                            return
                        if (App.selectedPlaylistTrackIndex >= 0)
                            positionViewAtIndex(App.selectedPlaylistTrackIndex, ListView.Contain)
                    }

                    delegate: ItemDelegate {
                        id: playlistTrackDelegate
                        width: trackList.width
                        highlighted: index === App.selectedPlaylistTrackIndex
                        Accessible.name: (model.resolved === false ? "Missing track: " : "")
                                         + model.title
                        Accessible.role: Accessible.ListItem
                        Accessible.checkable: true
                        Accessible.checked: highlighted
                        opacity: trackList.draggingIndex === index ? 0.55 : 1

                        background: Rectangle {
                            radius: 0
                            color: {
                                if (playlistTrackDelegate.highlighted)
                                    return Theme.rgba(Theme.accent, 0.22)
                                if (playlistTrackDelegate.hovered && trackList.draggingIndex < 0)
                                    return Theme.rgba(Theme.selection, 0.85)
                                return "transparent"
                            }

                            Behavior on color {
                                ColorAnimation { duration: 120 }
                            }

                            Rectangle {
                                width: 3
                                height: parent.height - 8
                                anchors.left: parent.left
                                anchors.leftMargin: 2
                                anchors.verticalCenter: parent.verticalCenter
                                radius: 0
                                color: Theme.accent
                                opacity: App.playlistFocusColumn === "tracks"
                                         && playlistTrackDelegate.highlighted ? 1 : 0

                                Behavior on opacity {
                                    NumberAnimation { duration: 120 }
                                }
                            }

                            Rectangle {
                                anchors.top: parent.top
                                width: parent.width
                                height: 2
                                color: Theme.accent
                                visible: trackList.dropIndex === index
                                         && trackList.draggingIndex >= 0
                                         && trackList.draggingIndex !== index
                            }
                        }

                        contentItem: RowLayout {
                            spacing: Theme.spaceSm
                            Rectangle {
                                Layout.preferredWidth: 3
                                Layout.preferredHeight: 22
                                Layout.alignment: Qt.AlignVCenter
                                radius: 1
                                color: Theme.accent
                                visible: App.playback.currentPath === model.path
                                          && App.playback.currentPath.length > 0
                            }
                            Item {
                                Layout.preferredWidth: 28
                                Layout.minimumWidth: 28
                                Layout.maximumWidth: 28
                                Layout.fillHeight: true

                                Label {
                                    anchors.fill: parent
                                    text: trackList.draggingIndex === index ? "⋮⋮" : (index + 1)
                                    opacity: App.playback.currentPath === model.path
                                             || playlistTrackDelegate.highlighted ? 1 : 0.55
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    color: Theme.foreground
                                    font.pixelSize: trackList.draggingIndex === index
                                                    ? Theme.fontCaption : Theme.fontBody
                                }
                            }
                            Label {
                                id: playlistTrackTitleLabel
                                text: model.resolved === false ? "[missing] " + model.title : model.title
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                color: App.playback.currentPath === model.path
                                       ? Theme.accent : Theme.foreground
                                opacity: model.resolved === false ? 0.45
                                         : (playlistTrackDelegate.highlighted ? 1 : 0.92)
                                HoverHandler { id: playlistTrackTitleHover }
                                ElisionPopup {
                                    visible: playlistTrackTitleHover.hovered && playlistTrackTitleLabel.truncated
                                    text: playlistTrackTitleLabel.text
                                }

                            }
                            Label {
                                text: model.resolved === false ? "" : formatDuration(model.durationMs)
                                opacity: 0.55
                                color: Theme.foreground
                            }
                        }

                        // Overlay on the Control itself — contentItem MouseAreas do not receive
                        // presses because ItemDelegate owns the pointer handling.
                        MouseArea {
                            id: dragHandle
                            z: 20
                            width: 40
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            cursorShape: Qt.SizeVerCursor
                            preventStealing: true
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton
                            ToolTip.visible: App.config.tooltipsEnabled && containsMouse
                                             && trackList.draggingIndex < 0
                            ToolTip.text: "Drag to reorder"
                            onPressed: function(mouse) {
                                mouse.accepted = true
                                App.setPlaylistFocusColumn("tracks")
                                trackList.draggingIndex = index
                                trackList.dropIndex = index
                            }
                            onPositionChanged: function(mouse) {
                                if (trackList.draggingIndex < 0 || !pressed)
                                    return
                                const p = mapToItem(trackList.contentItem, mouse.x, mouse.y)
                                const idx = trackList.indexAtContentY(p.y)
                                if (idx >= 0)
                                    trackList.dropIndex = idx
                            }
                            onReleased: function(mouse) {
                                mouse.accepted = true
                                const from = trackList.draggingIndex
                                let to = trackList.dropIndex
                                if (from >= 0) {
                                    const p = mapToItem(trackList.contentItem, mouse.x, mouse.y)
                                    const idx = trackList.indexAtContentY(p.y)
                                    if (idx >= 0)
                                        to = idx
                                }
                                trackList.draggingIndex = -1
                                trackList.dropIndex = -1
                                if (from >= 0 && to >= 0 && from !== to)
                                    App.movePlaylistTrack(from, to)
                                else if (from >= 0)
                                    App.setSelectedPlaylistTrackIndex(from)
                            }
                            onCanceled: {
                                trackList.draggingIndex = -1
                                trackList.dropIndex = -1
                            }
                        }

                        Button {
                            anchors.right: parent.right
                            anchors.rightMargin: Theme.spaceSm
                            anchors.verticalCenter: parent.verticalCenter
                            z: 21
                            width: 28
                            height: 28
                            text: "×"
                            flat: true
                            Accessible.name: "Remove from playlist"
                            Accessible.role: Accessible.Button
                            opacity: playlistTrackDelegate.hovered && trackList.draggingIndex < 0 ? 1 : 0
                            enabled: playlistTrackDelegate.hovered && trackList.draggingIndex < 0
                            ToolTip.visible: App.config.tooltipsEnabled && hovered && enabled
                            ToolTip.text: "Remove from playlist"
                            onClicked: App.removePlaylistTrack(index)

                            Behavior on opacity {
                                NumberAnimation { duration: 120 }
                            }
                        }

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: {
                                App.setPlaylistFocusColumn("tracks")
                                App.setSelectedPlaylistTrackIndex(index)
                                trackContextMenu.trackIndex = index
                                trackContextMenu.popup()
                            }
                        }

                        onClicked: {
                            if (trackList.draggingIndex >= 0)
                                return
                            App.setPlaylistFocusColumn("tracks")
                            App.setSelectedPlaylistTrackIndex(index)
                            if (model.resolved !== false)
                                App.playPlaylistTrackIndex(index)
                        }
                    }
                }

                EmptyState {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    iconName: "media-playlist-consecutive-symbolic"
                    title: "Select a playlist"
                    subtitle: "Your tracks will appear here."
                    visible: playlistsView.selectedPlaylist.length === 0
                }

                EmptyState {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    iconName: "audio-x-generic-symbolic"
                    title: "This playlist is empty"
                    subtitle: "In Browse: right-click tracks/albums, or Ctrl/Shift-select tracks → Add to playlist. Drag the track number to reorder."
                    visible: playlistsView.selectedPlaylist.length > 0 && trackList.count === 0
                }
            }
        }
    }

    Dialog {
        id: createDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        title: "Create playlist"
        modal: true
        Overlay.modal: ThemedModalScrim {}
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: TextField {
            id: createField
            placeholderText: "Playlist name"
            selectByMouse: true
            onAccepted: {
                if (text.trim().length > 0)
                    createDialog.accept()
            }
        }

        onOpened: {
            createField.text = ""
            createField.forceActiveFocus()
        }
        onAccepted: {
            const name = createField.text.trim()
            if (name.length > 0)
                App.createPlaylist(name)
        }
    }

    Dialog {
        id: deleteDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        title: "Delete playlist?"
        modal: true
        Overlay.modal: ThemedModalScrim {}
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label {
            text: "Delete \"" + playlistsView.selectedPlaylist + "\" permanently?"
            wrapMode: Text.WordWrap
        }
        onAccepted: App.deleteSelectedPlaylist()
    }

    Dialog {
        id: renameDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        title: "Rename playlist"
        modal: true
        Overlay.modal: ThemedModalScrim {}
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: TextField {
            id: renameField
            placeholderText: "Playlist name"
            text: playlistsView.selectedPlaylist
            selectByMouse: true
            onAccepted: renameDialog.accept()
        }

        onOpened: {
            renameField.text = playlistsView.selectedPlaylist
            renameField.selectAll()
            renameField.forceActiveFocus()
        }
        onAccepted: {
            if (renameField.text.trim().length > 0)
                App.renameSelectedPlaylist(renameField.text.trim())
        }
    }

    function formatDuration(ms) {
        if (!ms || ms <= 0) return "--:--"
        var totalSec = Math.floor(ms / 1000)
        var min = Math.floor(totalSec / 60)
        var sec = totalSec % 60
        return min + ":" + (sec < 10 ? "0" : "") + sec
    }

    function playlistTrackCount() {
        return trackList.count
    }
}
