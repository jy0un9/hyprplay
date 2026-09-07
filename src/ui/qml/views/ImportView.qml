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
                enabled: !App.importInbox.importing
                onClicked: App.importInbox.scanInbox()
            }

            Button {
                text: "Select all"
                enabled: !App.importInbox.importing && App.importInbox.albums.length > 0
                onClicked: App.importInbox.setAllAlbumsSelected(true)
            }

            Button {
                text: "Skip all"
                enabled: !App.importInbox.importing && App.importInbox.albums.length > 0
                onClicked: App.importInbox.setAllAlbumsSelected(false)
            }

            PrimaryButton {
                text: App.importInbox.importing
                      ? "Importing…"
                      : ("Import selected (" + App.importInbox.selectedCount + ")")
                enabled: !App.importInbox.importing && App.importInbox.selectedCount > 0
                onClicked: App.importInbox.startImport(beetsImportField.checked)
            }

            CheckBox {
                id: beetsImportField
                text: "Run beets import"
                checked: App.beets.available
                enabled: App.beets.available && !App.importInbox.importing
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
                  + (App.importInbox.destinationRoot.length > 0
                     ? ("  →  Library: " + App.importInbox.destinationRoot)
                     : "")
            opacity: 0.55
            font.pixelSize: Theme.fontCaption
            color: Theme.foreground
            Layout.fillWidth: true
            elide: Text.ElideMiddle
        }

        Label {
            visible: albumList.count > 0
            text: "Uncheck albums to skip. Each row shows the Opus destination under your library."
            opacity: 0.6
            font.pixelSize: Theme.fontCaption
            color: Theme.foreground
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        ListView {
            id: albumList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: App.importInbox.albums
            visible: count > 0 || App.importInbox.importing
            spacing: 2

            delegate: ItemDelegate {
                id: albumDelegate
                width: albumList.width
                enabled: !App.importInbox.importing
                padding: Theme.spaceSm
                Accessible.name: model.artist + " — " + model.album
                Accessible.role: Accessible.CheckBox
                Accessible.checkable: true
                Accessible.checked: model.selected !== false

                background: Rectangle {
                    radius: 0
                    color: albumDelegate.hovered
                           ? Theme.rgba(Theme.selection, 0.85)
                           : "transparent"
                }

                contentItem: RowLayout {
                    spacing: Theme.spaceSm

                    CheckBox {
                        id: selectBox
                        checked: model.selected !== false
                        enabled: !App.importInbox.importing
                        Accessible.name: "Select " + model.artist + " — " + model.album
                        onClicked: App.importInbox.setAlbumSelected(index, checked)
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: model.artist + " — " + model.album
                            color: Theme.foreground
                            font.bold: selectBox.checked
                            opacity: selectBox.checked ? 1 : 0.45
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Label {
                            text: model.trackCount + " FLAC  →  "
                                  + (model.destDir && model.destDir.length > 0
                                     ? model.destDir
                                     : "(set a library path in Settings)")
                            opacity: selectBox.checked ? 0.6 : 0.35
                            font.pixelSize: Theme.fontCaption
                            color: Theme.foreground
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }
                }
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
