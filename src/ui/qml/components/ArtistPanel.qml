import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: panel
    padding: Theme.spaceLg
    visible: App.selectedArtist.length > 0

    property var media: App.selectedArtistMedia

    background: Rectangle { color: Theme.chrome }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spaceMd

        Label {
            text: App.selectedArtist
            font.bold: true
            font.pixelSize: Theme.fontHeading
            color: Theme.foreground
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

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
                    source: media.artistImageUrl
                    fillMode: Image.PreserveAspectCrop
                    visible: media.artistImageUrl.length > 0
                }

                Image {
                    anchors.centerIn: parent
                    width: 72
                    height: 72
                    source: Theme.iconUrl("avatar-default-symbolic", 72, Theme.foreground)
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
            font.pixelSize: Theme.fontSmall
            opacity: 0.55
            color: Theme.foreground
        }

        Label {
            text: App.discogs.status
            visible: App.discogs.status.length > 0
            wrapMode: Text.WordWrap
            opacity: 0.6
            font.pixelSize: Theme.fontCaption
            color: Theme.foreground
            Layout.fillWidth: true
        }

        ScrollView {
            id: profileScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            Label {
                width: profileScroll.availableWidth
                text: media.hasProfile ? media.profileText : "No profile.txt found for this artist."
                wrapMode: Text.WordWrap
                opacity: media.hasProfile ? 0.92 : 0.45
                lineHeight: 1.35
                padding: 2
                font.pixelSize: Theme.fontBody
                color: Theme.foreground
            }
        }
    }
}
