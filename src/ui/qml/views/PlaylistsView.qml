import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: playlistsView
    padding: 0

    function applyLayout() {
        playlistListPane.paneWidth = App.config.layoutPlaylistsListWidth
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

        handle: Rectangle {
            implicitWidth: 5
            color: plSplitHover.hovered ? Theme.rgba(Theme.accent, 0.55) : Theme.rgba(Theme.border, 0.35)
            HoverHandler { id: plSplitHover }

            MouseArea {
                anchors.fill: parent
                propagateComposedEvents: true
                onPressed: (mouse) => mouse.accepted = false
                onReleased: playlistsView.persistLayout()
            }
        }

        Item {
            id: playlistListPane
            property real paneWidth: 260
            SplitView.preferredWidth: paneWidth
            SplitView.minimumWidth: 180
            SplitView.maximumWidth: 420

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                Label {
                    text: "Playlists"
                    font.bold: true
                    font.pixelSize: 16
                    color: Theme.foreground
                    Layout.leftMargin: 12
                    Layout.topMargin: 12
                }

                Label {
                    text: App.playlists.status
                    font.pixelSize: 11
                    opacity: 0.65
                    color: Theme.foreground
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 8
                    Layout.rightMargin: 8
                    spacing: 6

                    TextField {
                        id: newPlaylistField
                        Layout.fillWidth: true
                        placeholderText: "New playlist…"
                        onAccepted: createPlaylistButton.clicked()
                    }

                    Button {
                        id: createPlaylistButton
                        text: "Create"
                        highlighted: true
                        onClicked: {
                            if (newPlaylistField.text.trim().length === 0)
                                return
                            App.createPlaylist(newPlaylistField.text.trim())
                            newPlaylistField.text = ""
                        }
                    }
                }

                ListView {
                    id: playlistList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: App.playlistItems

                    delegate: ItemDelegate {
                        id: playlistDelegate
                        width: playlistList.width
                        text: model.name + "  [" + model.trackCount + "]"
                        highlighted: playlistsView.selectedPlaylist === model.name
                        onClicked: App.selectPlaylist(model.name)

                        background: Rectangle {
                            color: {
                                if (playlistDelegate.highlighted)
                                    return Theme.rgba(Theme.accent, 0.18)
                                if (playlistDelegate.hovered)
                                    return Theme.rgba(Theme.selection, 0.7)
                                if (index % 2 === 1)
                                    return Theme.rgba(Theme.selection, 0.22)
                                return "transparent"
                            }
                        }
                    }
                }

                Button {
                    text: "Delete Playlist"
                    Layout.fillWidth: true
                    Layout.margins: 8
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
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 12
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    visible: playlistsView.selectedPlaylist.length > 0

                    Label {
                        text: playlistsView.selectedPlaylist
                        font.pixelSize: 17
                        font.bold: true
                        color: Theme.foreground
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Button {
                        text: "Play Playlist"
                        highlighted: true
                        onClicked: App.playPlaylist()
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
                            color: {
                                if (playlistTrackDelegate.hovered)
                                    return Theme.rgba(Theme.selection, 0.7)
                                if (index % 2 === 1)
                                    return Theme.rgba(Theme.selection, 0.22)
                                return "transparent"
                            }
                        }

                        contentItem: RowLayout {
                            spacing: 12
                            Label {
                                text: index + 1
                                opacity: 0.55
                                color: Theme.foreground
                                Layout.preferredWidth: 28
                            }
                            Label {
                                text: model.resolved === false ? "[missing] " + model.title : model.title
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                color: Theme.foreground
                                opacity: model.resolved === false ? 0.45 : 1
                            }
                            Label {
                                text: model.resolved === false ? "" : formatDuration(model.durationMs)
                                opacity: 0.55
                                color: Theme.foreground
                            }
                        }

                        onClicked: App.playPlaylistTrackIndex(index)
                    }
                }

                Label {
                    text: "Select a playlist"
                    opacity: 0.45
                    color: Theme.foreground
                    Layout.alignment: Qt.AlignCenter
                    visible: playlistsView.selectedPlaylist.length === 0
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

    function formatDuration(ms) {
        if (!ms || ms <= 0) return "--:--"
        var totalSec = Math.floor(ms / 1000)
        var min = Math.floor(totalSec / 60)
        var sec = totalSec % 60
        return min + ":" + (sec < 10 ? "0" : "") + sec
    }
}
