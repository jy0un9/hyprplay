import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: tagFetchDialog
    title: "Lookup metadata"
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(620, parent ? parent.width * 0.92 : 620)
    height: Math.min(560, parent ? parent.height * 0.85 : 560)

    Material.theme: Theme.dark ? Material.Dark : Material.Light

    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.surface
        border.color: Theme.rgba(Theme.border, 0.45)
        border.width: 1
    }

    onClosed: App.closeTagFetch()

    Connections {
        target: App
        function onTagFetchChanged() {
            if (App.tagFetchOpen)
                tagFetchDialog.open()
            else
                tagFetchDialog.close()
        }
    }

    footer: DialogButtonBox {
        Button {
            text: "Cancel"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: tagFetchDialog.reject()
        }
        Button {
            text: "Apply selected tags"
            highlighted: true
            enabled: App.metadataSearch.fieldChoices.length > 0 && !App.metadataSearch.searching
            onClicked: App.applyTagFetch(syncBeetsField.checked)
        }
    }

    contentItem: ColumnLayout {
        spacing: 10

        Label {
            text: App.metadataSearch.status
            wrapMode: Text.WordWrap
            opacity: 0.75
            Layout.fillWidth: true
        }

        BusyIndicator {
            running: App.metadataSearch.searching
            Layout.alignment: Qt.AlignHCenter
            visible: App.metadataSearch.searching
        }

        Label {
            text: "Matching releases"
            font.bold: true
        }

        ListView {
            id: candidateList
            Layout.fillWidth: true
            Layout.preferredHeight: 160
            clip: true
            model: App.metadataSearch.candidates
            currentIndex: App.metadataSearch.selectedIndex

            delegate: ItemDelegate {
                width: candidateList.width
                highlighted: App.metadataSearch.selectedIndex === index
                onClicked: App.metadataSearch.setSelectedIndex(index)

                contentItem: ColumnLayout {
                    spacing: 2
                    Label {
                        text: (modelData.source || "") + ": " + (modelData.title || "")
                        font.bold: parent.parent.highlighted
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Label {
                        text: modelData.detail || ""
                        opacity: 0.65
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }
            }
        }

        Label {
            text: "Tags to apply"
            font.bold: true
            visible: App.metadataSearch.fieldChoices.length > 0
        }

        ListView {
            id: fieldList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: App.metadataSearch.fieldChoices
            visible: count > 0

            delegate: CheckBox {
                id: fieldChoice
                width: fieldList.width
                property string fieldKey: modelData.key || ""
                property string fieldLabel: modelData.label || fieldKey
                property string fieldCurrent: modelData.current || ""
                property string fieldProposed: modelData.proposed || ""

                checked: modelData.checked === true
                text: fieldLabel + ":  \"" + fieldCurrent + "\"  →  \"" + fieldProposed + "\""
                onClicked: App.metadataSearch.setFieldChecked(fieldKey, checked)
            }

        }

        Label {
            text: "Select a release above to compare tags."
            opacity: 0.5
            visible: App.metadataSearch.fieldChoices.length === 0 && !App.metadataSearch.searching
            Layout.fillWidth: true
        }

        CheckBox {
            id: syncBeetsField
            text: "Also update beets library (if imported)"
            visible: App.beets.available
            checked: App.beets.available
        }

        Label {
            text: App.beets.status
            visible: App.beets.available && App.beets.status.length > 0
            opacity: 0.6
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
