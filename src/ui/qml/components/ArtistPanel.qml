import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: panel
    padding: 16
    visible: App.selectedArtist.length > 0

    property var media: App.selectedArtistMedia

    background: Rectangle { color: Theme.darkBackground }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            text: App.selectedArtist
            font.bold: true
            font.pixelSize: 17
            color: Theme.foreground
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 200

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - 8, 200)
                height: width
                radius: Theme.radiusMd
                color: Theme.rgba(Theme.selection, 0.85)
                clip: true

                Image {
                    anchors.fill: parent
                    source: media.artistImageUrl
                    fillMode: Image.PreserveAspectCrop
                    visible: media.artistImageUrl.length > 0
                }

                Image {
                    anchors.centerIn: parent
                    width: 56
                    height: 56
                    source: "image://themeicon/avatar-default-symbolic?56"
                    fillMode: Image.PreserveAspectFit
                    opacity: 0.22
                    visible: media.artistImageUrl.length === 0
                    cache: true
                }
            }
        }

        Label {
            text: "Artist profile"
            font.bold: true
            font.pixelSize: 12
            opacity: 0.55
            color: Theme.foreground
        }

        Label {
            text: App.discogs.status
            visible: App.discogs.status.length > 0
            wrapMode: Text.WordWrap
            opacity: 0.6
            font.pixelSize: 11
            color: Theme.foreground
            Layout.fillWidth: true
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Label {
                width: parent.width
                text: media.hasProfile ? media.profileText : "No profile.txt found for this artist."
                wrapMode: Text.WordWrap
                opacity: media.hasProfile ? 0.92 : 0.45
                lineHeight: 1.35
                font.pixelSize: 13
                color: Theme.foreground
            }
        }
    }
}
