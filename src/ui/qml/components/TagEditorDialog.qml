import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: tagEditor
    title: App.tagEditorTitle.length > 0 ? App.tagEditorTitle : "Edit Tags"
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(480, parent ? parent.width * 0.9 : 480)
    visible: App.tagEditorOpen && !App.tagFetchOpen
    standardButtons: Dialog.Save | Dialog.Cancel
    onAccepted: saveTagEditor()
    onRejected: App.closeTagEditor()

    Material.theme: Theme.dark ? Material.Dark : Material.Light

    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.surface
        border.color: Theme.rgba(Theme.border, 0.35)
        border.width: 1
    }

    property alias fields: fieldGrid

    contentItem: ColumnLayout {
        spacing: Theme.spaceSm

        Label {
            text: {
                if (App.tagEditorMode === "album")
                    return "Album-wide tags (all tracks on this album)"
                if (App.tagEditorMode === "artist")
                    return "Artist-wide tags (all tracks by this artist)"
                return "Track tags (this file only)"
            }
            opacity: 0.7
            Layout.fillWidth: true
        }

        GridLayout {
            id: fieldGrid
            columns: 2
            columnSpacing: Theme.spaceMd
            rowSpacing: Theme.spaceSm
            Layout.fillWidth: true

            Label { text: "Title"; visible: App.tagEditorMode === "track" }
            TextField {
                id: titleField
                Layout.fillWidth: true
                visible: App.tagEditorMode === "track"
                text: App.tagEditorFields.title || ""
            }

            Label { text: "Artist" }
            TextField {
                id: artistField
                Layout.fillWidth: true
                text: App.tagEditorFields.artist || ""
            }

            Label { text: "Album"; visible: App.tagEditorMode !== "artist" }
            TextField {
                id: albumField
                Layout.fillWidth: true
                visible: App.tagEditorMode !== "artist"
                text: App.tagEditorFields.album || ""
            }

            Label { text: "Album artist" }
            TextField {
                id: albumArtistField
                Layout.fillWidth: true
                text: App.tagEditorFields.albumArtist || ""
            }

            Label { text: "Track #" ; visible: App.tagEditorMode === "track" }
            SpinBox {
                id: trackField
                visible: App.tagEditorMode === "track"
                from: 0
                to: 999
                value: App.tagEditorFields.trackNumber || 0
            }

            Label { text: "Year" }
            SpinBox {
                id: yearField
                from: 0
                to: 9999
                value: App.tagEditorFields.year || 0
            }

            Label { text: "Genre" }
            TextField {
                id: genreField
                Layout.fillWidth: true
                text: App.tagEditorFields.genre || ""
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceSm

            Button {
                text: "Lookup metadata…"
                onClicked: {
                    var fields = {
                        artist: artistField.text.trim(),
                        albumArtist: albumArtistField.text.trim(),
                        year: yearField.value,
                        genre: genreField.text.trim()
                    }
                    if (App.tagEditorMode !== "artist")
                        fields.album = albumField.text.trim()
                    if (App.tagEditorMode === "track") {
                        fields.title = titleField.text.trim()
                        fields.trackNumber = trackField.value
                    }
                    App.openTagFetch(fields)
                }
            }

            Item { Layout.fillWidth: true }
        }

        Label {
            text: App.tags.status
            opacity: 0.65
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    function saveTagEditor() {
        var fields = {
            artist: artistField.text.trim(),
            album: albumField.text.trim(),
            albumArtist: albumArtistField.text.trim(),
            year: yearField.value,
            genre: genreField.text.trim()
        }
        if (App.tagEditorMode === "track") {
            fields.title = titleField.text.trim()
            fields.trackNumber = trackField.value
        } else if (App.tagEditorMode === "album") {
            // album, artist, albumArtist, year, genre only
        } else if (App.tagEditorMode === "artist") {
            delete fields.album
        }
        App.saveTagEditor(fields)
    }

    Connections {
        target: App
        function onTagEditorChanged() {
            titleField.text = App.tagEditorFields.title || ""
            artistField.text = App.tagEditorFields.artist || ""
            albumField.text = App.tagEditorFields.album || ""
            albumArtistField.text = App.tagEditorFields.albumArtist || ""
            trackField.value = App.tagEditorFields.trackNumber || 0
            yearField.value = App.tagEditorFields.year || 0
            genreField.text = App.tagEditorFields.genre || ""
        }
    }
}
