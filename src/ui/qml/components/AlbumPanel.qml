import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: panel
    padding: Theme.spaceLg
    visible: App.selectedAlbum.length > 0
    readonly property string artworkUrl: {
        if (App.selectedAlbumArtUrl.length > 0)
            return App.selectedAlbumArtUrl
        if (App.tracks.count > 0) {
            const firstTrack = App.tracks.trackAt(0)
            return App.nowPlaying.albumArtForTrack(firstTrack["path"])
        }
        return ""
    }

    background: Rectangle { color: Theme.darkBackground }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spaceMd

        Label {
            text: "Album"
            font.bold: true
            font.pixelSize: Theme.fontSmall
            opacity: 0.55
            color: Theme.foreground
            Layout.alignment: Qt.AlignHCenter
        }

        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 220
            Layout.preferredHeight: 220
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            
            Rectangle {
                anchors.fill: parent
                color: Theme.rgba(Theme.selection, 0.85)

                Image {
                    id: albumArtwork
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    source: panel.artworkUrl
                    visible: status === Image.Ready
                    cache: true
                }

                Image {
                    anchors.centerIn: parent
                    width: 56
                    height: 56
                    source: Theme.iconUrl("audio-x-generic-symbolic", 56, Theme.foreground)
                    opacity: 0.22
                    visible: albumArtwork.status !== Image.Ready
                }
            }
        }

        Label {
            text: App.selectedAlbum
            font.bold: true
            font.pixelSize: Theme.fontTitle
            color: Theme.foreground
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        Label {
            text: "by " + App.selectedArtist
            opacity: 0.65
            font.pixelSize: Theme.fontSmall
            color: Theme.foreground
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        Label {
            text: App.tracks.count + " tracks"
            opacity: 0.5
            font.pixelSize: Theme.fontCaption
            color: Theme.foreground
            Layout.alignment: Qt.AlignHCenter
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            visible: App.selectedAlbumInfo.length > 0

            Label {
                width: parent.width
                text: App.selectedAlbumInfo
                wrapMode: Text.WordWrap
                lineHeight: 1.3
                opacity: 0.78
                color: Theme.foreground
            }
        }

        Label {
            text: "Right-click the album to fetch information from Discogs."
            visible: App.selectedAlbumInfo.length === 0
            opacity: 0.45
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }
    }
}
