import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: importView
    padding: 16

    background: Rectangle { color: Theme.background }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            text: "Import Inbox"
            font.pixelSize: 22
            font.bold: true
            color: Theme.foreground
        }

        Label {
            text: App.importInbox.status
            wrapMode: Text.WordWrap
            opacity: 0.75
            color: Theme.foreground
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: "Scan Inbox"
                onClicked: App.importInbox.scanInbox()
            }

            Button {
                text: App.importInbox.importing ? "Importing…" : "Import All"
                highlighted: true
                enabled: !App.importInbox.importing && App.importInbox.albums.length > 0
                onClicked: App.importInbox.startImport(beetsImportField.checked)
            }

            CheckBox {
                id: beetsImportField
                text: "Run beets import"
                checked: App.beets.available
                enabled: App.beets.available
            }

            Button {
                text: "Cancel"
                visible: App.importInbox.importing
                onClicked: App.importInbox.cancelImport()
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: 100
            value: App.importInbox.progress
            visible: App.importInbox.importing || App.importInbox.progress > 0
        }

        Label {
            text: "Inbox: " + (App.config.importInbox.length > 0 ? App.config.importInbox : "(not configured)")
            opacity: 0.55
            font.pixelSize: 11
            color: Theme.foreground
            Layout.fillWidth: true
        }

        ListView {
            id: albumList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: App.importInbox.albums

            delegate: ItemDelegate {
                width: albumList.width
                text: model.artist + " — " + model.album + "  [" + model.trackCount + " FLAC]"
            }
        }
    }
}
