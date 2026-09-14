import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import components 1.0

Pane {
    id: convertView
    property bool embedded: false

    padding: embedded ? 0 : Theme.spaceLg

    background: Rectangle { color: embedded ? "transparent" : Theme.background }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spaceMd

        Label {
            text: "Convert FLAC → Opus"
            font.pixelSize: Theme.fontDisplay
            font.bold: true
            color: Theme.foreground
            visible: !convertView.embedded
        }

        Label {
            text: App.convert.status
            visible: text.length > 0
            wrapMode: Text.WordWrap
            opacity: 0.75
            color: Theme.foreground
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceSm

            Button {
                text: "Scan library"
                enabled: !App.convert.converting
                onClicked: App.convert.scanLibrary()
            }

            Button {
                text: "Select all"
                enabled: !App.convert.converting && App.convert.albums.length > 0
                onClicked: App.convert.setAllAlbumsSelected(true)
            }

            Button {
                text: "Skip all"
                enabled: !App.convert.converting && App.convert.albums.length > 0
                onClicked: App.convert.setAllAlbumsSelected(false)
            }

            PrimaryButton {
                text: App.convert.converting
                      ? "Converting…"
                      : ("Convert selected (" + App.convert.selectedCount + ")")
                enabled: !App.convert.converting && App.convert.selectedCount > 0
                onClicked: App.convert.startConvert()
            }

            Button {
                text: "Cancel"
                visible: App.convert.converting
                enabled: !App.convert.awaitingDecision
                onClicked: App.convert.cancelConvert()
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: 100
            value: App.convert.progress
            visible: App.convert.converting || App.convert.progress > 0
        }

        Label {
            visible: App.convert.converting && App.convert.tracksTotal > 0
            text: "Tracks " + App.convert.tracksDone + " / " + App.convert.tracksTotal
            opacity: 0.55
            font.pixelSize: Theme.fontCaption
            color: Theme.foreground
        }

        Label {
            visible: albumList.count > 0
            text: "Uncheck albums to skip. Writes Opus next to each FLAC. Failed encodes pause — partial files are never left behind."
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
            model: App.convert.albums
            visible: count > 0 || App.convert.converting
            spacing: 2
            enabled: !App.convert.awaitingDecision

            delegate: ItemDelegate {
                id: albumDelegate
                width: albumList.width
                enabled: !App.convert.converting
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
                        enabled: !App.convert.converting
                        Accessible.name: "Select " + model.artist + " — " + model.album
                        onClicked: App.convert.setAlbumSelected(index, checked)
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
                            text: model.trackCount + " FLAC"
                                  + (model.destDir && model.destDir.length > 0
                                     ? ("  ·  " + model.destDir)
                                     : "")
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
            iconName: "audio-x-generic-symbolic"
            title: "No FLAC albums"
            subtitle: "Scan the library after importing FLAC, or import first."
            loading: App.convert.converting
            visible: albumList.count === 0 && !App.convert.converting
        }
    }

    Dialog {
        id: decisionDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        Overlay.modal: ThemedModalScrim {}
        closePolicy: Popup.NoAutoClose
        title: App.convert.decisionTitle.length > 0
               ? App.convert.decisionTitle
               : "Conversion failed"
        width: Math.min(520, Overlay.overlay ? Overlay.overlay.width - 48 : 520)

        contentItem: ColumnLayout {
            spacing: Theme.spaceMd

            Label {
                text: App.convert.decisionMessage
                wrapMode: Text.WordWrap
                color: Theme.foreground
                Layout.fillWidth: true
            }

            Label {
                visible: App.convert.decisionDetail.length > 0
                text: App.convert.decisionDetail
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
                    App.convert.resolveConvertDecision("retry")
                    decisionDialog.close()
                }
            }
            Button {
                text: "Skip track"
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: {
                    App.convert.resolveConvertDecision("skipTrack")
                    decisionDialog.close()
                }
            }
            Button {
                text: "Skip album"
                DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole
                onClicked: {
                    App.convert.resolveConvertDecision("skipAlbum")
                    decisionDialog.close()
                }
            }
            Button {
                text: "Abort"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: {
                    App.convert.resolveConvertDecision("abort")
                    decisionDialog.close()
                }
            }
        }

        onRejected: {
            if (App.convert.awaitingDecision)
                App.convert.resolveConvertDecision("abort")
        }
    }

    Connections {
        target: App.convert
        function onDecisionChanged() {
            if (App.convert.awaitingDecision)
                decisionDialog.open()
            else if (decisionDialog.visible)
                decisionDialog.close()
        }
        function onDecisionRequired(title, message, detailPath) {
            decisionDialog.open()
        }
    }
}
