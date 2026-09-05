import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import components 1.0

Pane {
    id: importView
    padding: Theme.spaceLg

    background: Rectangle { color: Theme.background }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spaceMd

        Label {
            text: "Import Inbox"
            font.pixelSize: Theme.fontDisplay
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
            spacing: Theme.spaceSm

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
            font.pixelSize: Theme.fontCaption
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
            visible: count > 0 || App.importInbox.importing

            delegate: ItemDelegate {
                width: albumList.width
                text: model.artist + " — " + model.album + "  [" + model.trackCount + " FLAC]"
            }
        }

        EmptyState {
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconName: "folder-download-symbolic"
            title: App.config.importInbox.length > 0 ? "Inbox is empty" : "No inbox folder set"
            subtitle: App.config.importInbox.length > 0
                      ? "Drop albums into the inbox folder, then scan."
                      : "Set your Music folder in Settings first."
            actionText: App.config.importInbox.length > 0 ? "" : "Open Settings"
            loading: App.importInbox.importing
            visible: albumList.count === 0 && !App.importInbox.importing
            onActionClicked: App.showSettings()
        }
    }
}
