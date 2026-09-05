import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import components 1.0

Pane {
    id: playlistsView
    padding: 0

    function applyLayout() {
        playlistListPane.paneWidth = App.config.layoutPlaylistsListWidth
    }

    function playSelected() {
        if (App.playlistTracks.count > 0)
            App.playPlaylistTrackIndex(0)
        else
            App.playPlaylist()
    }

    function removeCurrentPlaylistTrack() {
        if (trackList.currentIndex >= 0 && trackList.count > 0)
            App.removePlaylistTrack(trackList.currentIndex)
    }

    function focusNewPlaylistField() {
        newPlaylistField.forceActiveFocus()
        newPlaylistField.selectAll()
    }

    function persistLayout() {
        App.config.setLayoutPlaylistsListWidth(Math.round(playlistListPane.width))
    }

    Component.onCompleted: Qt.callLater(applyLayout)

    background: Rectangle { color: Theme.background }

    property string selectedPlaylist: App.playlists.selectedPlaylist

    SplitView {
        id: playlistSplit
        anchors.fill: parent
        orientation: Qt.Horizontal

        handle: SplitHandle {
            orientation: Qt.Horizontal
            onReleased: playlistsView.persistLayout()
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

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.spaceSm
                    Layout.rightMargin: Theme.spaceSm
                    spacing: Theme.spaceXs + 2

                    TextField {
                        id: newPlaylistField
                        Layout.fillWidth: true
                        placeholderText: "New playlist…"
                        onAccepted: createPlaylistButton.clicked()
                    }

                    PrimaryButton {
                        id: createPlaylistButton
                        text: "Create"
                        ToolTip.visible: App.config.tooltipsEnabled && hovered
                        ToolTip.text: "Create playlist"
                        onClicked: {
                            if (newPlaylistField.text.trim().length === 0)
                                return
                            App.createPlaylist(newPlaylistField.text.trim())
                            newPlaylistField.text = ""
                        }
                    }
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

                    delegate: AppListDelegate {
                        id: playlistDelegate
                        width: playlistList.width
                        text: model.name
                        highlighted: playlistsView.selectedPlaylist === model.name
                        onClicked: App.selectPlaylist(model.name)

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
                                    text: playlistDelegate.text
                                    font: playlistDelegate.font
                                    color: Theme.foreground
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
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

                Button {
                    text: "Delete"
                    Layout.fillWidth: true
                    Layout.margins: Theme.spaceSm
                    enabled: playlistsView.selectedPlaylist.length > 0
                    onClicked: deleteDialog.open()
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
                        text: playlistsView.selectedPlaylist
                        font.pixelSize: Theme.fontHeading
                        font.bold: true
                        color: Theme.foreground
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Label {
                        text: playlistTrackCount() + " tracks"
                        font.pixelSize: Theme.fontCaption
                        opacity: 0.6
                        color: Theme.foreground
                    }

                    PrimaryButton {
                        text: "Play"
                        onClicked: App.playPlaylist()
                    }

                    Button {
                        text: "Shuffle"
                        Material.roundedScale: Material.SmallScale
                        onClicked: {
                            App.playback.setShuffle(true)
                            App.playPlaylist()
                        }
                    }

                    Button {
                        text: "Rename"
                        Material.roundedScale: Material.SmallScale
                        onClicked: renameDialog.open()
                    }
                }

                ListView {
                    id: trackList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: App.playlistTracks
                    visible: playlistsView.selectedPlaylist.length > 0

                    delegate: ItemDelegate {
                        id: playlistTrackDelegate
                        width: trackList.width
                        enabled: model.resolved !== false

                        background: Rectangle {
                            radius: 0
                            color: playlistTrackDelegate.hovered
                                   ? Theme.rgba(Theme.selection, 0.85)
                                   : "transparent"

                            Behavior on color {
                                ColorAnimation { duration: 120 }
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
                                    text: index + 1
                                    opacity: App.playback.currentPath === model.path ? 1 : 0.55
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    color: Theme.foreground
                                }
                            }
                            Label {
                                text: model.resolved === false ? "[missing] " + model.title : model.title
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                color: App.playback.currentPath === model.path
                                       ? Theme.accent : Theme.foreground
                                opacity: model.resolved === false ? 0.45 : 1
                            }
                            Label {
                                text: model.resolved === false ? "" : formatDuration(model.durationMs)
                                opacity: 0.55
                                color: Theme.foreground
                            }
                        }

                        Button {
                            anchors.right: parent.right
                            anchors.rightMargin: Theme.spaceSm
                            anchors.verticalCenter: parent.verticalCenter
                            width: 28
                            height: 28
                            text: "×"
                            flat: true
                            opacity: playlistTrackDelegate.hovered ? 1 : 0
                            enabled: playlistTrackDelegate.hovered
                            ToolTip.visible: App.config.tooltipsEnabled && hovered && enabled
                            ToolTip.text: "Remove from playlist"
                            onClicked: App.removePlaylistTrack(index)

                            Behavior on opacity {
                                NumberAnimation { duration: 120 }
                            }
                        }

                        onClicked: App.playPlaylistTrackIndex(index)
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
                    subtitle: "Add tracks from the library."
                    visible: playlistsView.selectedPlaylist.length > 0 && trackList.count === 0
                }
            }
        }
    }

    Dialog {
        id: deleteDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        title: "Delete playlist?"
        modal: true
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
