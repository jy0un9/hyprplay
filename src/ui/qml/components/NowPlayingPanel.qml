import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: panel
    padding: 16
    visible: App.playback.currentPath.length > 0

    property var media: App.nowPlaying
    property var playback: App.playback

    background: Rectangle { color: Theme.darkBackground }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label {
            text: "Now Playing"
            font.bold: true
            font.pixelSize: 12
            opacity: 0.55
            color: Theme.foreground
            Layout.alignment: Qt.AlignHCenter
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(220, panel.width - 32)
            Layout.maximumHeight: 240

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - 8, parent.height, 220)
                height: width
                radius: Theme.radiusMd
                color: Theme.rgba(Theme.selection, 0.85)
                clip: true

                Image {
                    anchors.fill: parent
                    source: media.albumArtUrl
                    fillMode: Image.PreserveAspectCrop
                    visible: media.albumArtUrl.length > 0
                }

                Image {
                    anchors.centerIn: parent
                    width: 56
                    height: 56
                    source: "image://themeicon/audio-x-generic-symbolic?56"
                    fillMode: Image.PreserveAspectFit
                    opacity: 0.22
                    visible: media.albumArtUrl.length === 0
                    cache: true
                }
            }
        }

        Label {
            text: playback.title
            font.bold: true
            font.pixelSize: 15
            color: Theme.foreground
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            maximumLineCount: 2
        }

        Label {
            text: playback.artist + " — " + playback.album
            opacity: 0.65
            font.pixelSize: 12
            color: Theme.foreground
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            maximumLineCount: 2
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 220
            radius: Theme.radiusSm
            color: Theme.rgba(Theme.background, 0.5)
            border.color: Theme.rgba(Theme.border, 0.3)
            border.width: 1

            Loader {
                anchors.fill: parent
                anchors.margins: 8
                source: "qrc:/components/KaraokeLyricsView.qml"
            }
        }
    }
}
