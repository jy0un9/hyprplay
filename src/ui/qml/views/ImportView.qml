import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import components 1.0

Pane {
    id: importView
    property bool embedded: false
    signal configureRequested()

    padding: embedded ? 0 : Theme.spaceLg

    background: Rectangle { color: embedded ? "transparent" : Theme.background }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spaceMd

        Label {
            text: "Import Inbox"
            font.pixelSize: Theme.fontDisplay
            font.bold: true
            color: Theme.foreground
            visible: !importView.embedded
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
                enabled: !App.importInbox.awaitingDecision
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
            visible: App.importInbox.importing && App.importInbox.tracksTotal > 0
            text: "Tracks " + App.importInbox.tracksDone + " / " + App.importInbox.tracksTotal
            opacity: 0.55
            font.pixelSize: Theme.fontCaption
            color: Theme.foreground
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
            text: "Uncheck albums to skip. FLACs convert to Opus under your library. Failed converts pause so you can fix — nothing broken is written."
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
            enabled: !App.importInbox.awaitingDecision

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
                      : "Choose an inbox folder in Import setup."
            actionText: App.config.importInbox.length > 0 ? "" : "Configure inbox"
            loading: App.importInbox.importing
            visible: albumList.count === 0 && !App.importInbox.importing
            onActionClicked: {
                if (importView.embedded)
                    importView.configureRequested()
                else
                    App.showSettings()
            }
        }
    }

    Dialog {
        id: decisionDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        Overlay.modal: ThemedModalScrim {}
        closePolicy: Popup.NoAutoClose
        title: App.importInbox.decisionTitle.length > 0
               ? App.importInbox.decisionTitle
               : "Import conversion failed"
        width: Math.min(520, Overlay.overlay ? Overlay.overlay.width - 48 : 520)

        contentItem: ColumnLayout {
            spacing: Theme.spaceMd

            Label {
                text: App.importInbox.decisionMessage
                wrapMode: Text.WordWrap
                color: Theme.foreground
                Layout.fillWidth: true
            }

            Label {
                visible: App.importInbox.decisionDetail.length > 0
                text: App.importInbox.decisionDetail
                wrapMode: Text.WrapAnywhere
                elide: Text.ElideMiddle
                maximumLineCount: 3
                opacity: 0.65
                font.pixelSize: Theme.fontCaption
                color: Theme.foreground
                Layout.fillWidth: true
            }

            Label {
                text: "Fix the problem and Retry, or skip. Partial Opus files are never left in the library."
                wrapMode: Text.WordWrap
                opacity: 0.7
                font.pixelSize: Theme.fontCaption
                color: Theme.muted
                Layout.fillWidth: true
            }
        }

        footer: DialogButtonBox {
            Button {
                text: "Retry"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: {
                    App.importInbox.resolveImportDecision("retry")
                    decisionDialog.close()
                }
            }
            Button {
                text: "Skip track"
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: {
                    App.importInbox.resolveImportDecision("skipTrack")
                    decisionDialog.close()
                }
            }
            Button {
                text: "Skip album"
                DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole
                onClicked: {
                    App.importInbox.resolveImportDecision("skipAlbum")
                    decisionDialog.close()
                }
            }
            Button {
                text: "Abort import"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: {
                    App.importInbox.resolveImportDecision("abort")
                    decisionDialog.close()
                }
            }
        }

        onRejected: {
            if (App.importInbox.awaitingDecision)
                App.importInbox.resolveImportDecision("abort")
        }
    }

    Connections {
        target: App.importInbox
        function onDecisionChanged() {
            if (App.importInbox.awaitingDecision)
                decisionDialog.open()
            else if (decisionDialog.visible)
                decisionDialog.close()
        }
        function onDecisionRequired(title, message, detailPath) {
            decisionDialog.open()
        }
    }
}
