import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: panel
    padding: Theme.spaceLg
    visible: App.playback.currentPath.length > 0

    property var media: App.nowPlaying
    property var playback: App.playback

    background: Rectangle { color: Theme.chrome }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(400, panel.width - Theme.spaceLg * 2)

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width, parent.height)
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
                    width: 72
                    height: 72
                    source: Theme.iconUrl("audio-x-generic-symbolic", 72, Theme.foreground)
                    fillMode: Image.PreserveAspectFit
                    opacity: 0.22
                    visible: media.albumArtUrl.length === 0
                    cache: true
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 220
            radius: Theme.radiusSm
            color: Theme.rgba(Theme.background, 0.5)
            border.color: Theme.rgba(Theme.border, 0.35)
            border.width: 1

            Loader {
                anchors.fill: parent
                anchors.margins: Theme.spaceSm
                source: "qrc:/components/KaraokeLyricsView.qml"
            }
        }
    }
}
