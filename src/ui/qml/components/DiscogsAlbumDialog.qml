import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    title: "Choose Discogs album"
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(680, parent ? parent.width * 0.92 : 680)
    height: Math.min(560, parent ? parent.height * 0.85 : 560)

    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.surface
        border.color: Theme.rgba(Theme.border, 0.35)
        border.width: 1
    }

    onClosed: App.closeDiscogsForSelectedAlbum()

    Connections {
        target: App
        function onAlbumDiscogsChanged() {
            if (App.albumDiscogsOpen)
                dialog.open()
            else
                dialog.close()
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.spaceSm

        Label {
            text: "Select the matching release for " + App.selectedAlbum
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            visible: App.discogs.busy && App.albumDiscogsCandidates.length === 0
            text: App.discogs.status.length > 0 ? App.discogs.status : "Searching Discogs…"
            opacity: 0.7
            Layout.fillWidth: true
        }

        ListView {
            id: candidates
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: App.albumDiscogsCandidates
            currentIndex: App.albumDiscogsSelectedIndex
            visible: !App.discogs.busy || App.albumDiscogsCandidates.length > 0

            delegate: ItemDelegate {
                width: candidates.width
                highlighted: App.albumDiscogsSelectedIndex === index
                onClicked: App.setAlbumDiscogsSelectedIndex(index)

                contentItem: ColumnLayout {
                    spacing: 2
                    Label {
                        text: modelData.title || ""
                        font.bold: parent.parent.highlighted
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: [modelData.year || "", modelData.label || "",
                               modelData.format || "", modelData.country || ""]
                              .filter(function(value) { return value !== "" }).join(" · ")
                        opacity: 0.65
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }
        }

        Label {
            text: App.discogs.busy && App.albumDiscogsCandidates.length === 0
                  ? ""
                  : (App.albumDiscogsCandidates.length === 0
                     ? "No Discogs releases found."
                     : "Choose one release, then fetch its description and artwork.")
            opacity: 0.6
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            visible: text.length > 0
        }
    }

    footer: DialogButtonBox {
        Button {
            text: "Cancel"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: dialog.reject()
        }
        Button {
            text: "Fetch selected"
            enabled: App.albumDiscogsSelectedIndex >= 0 && !App.discogs.busy
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            onClicked: App.fetchSelectedDiscogsAlbum()
        }
    }
}
