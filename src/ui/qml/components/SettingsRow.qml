import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One setting: label and description on the left, control on the right.
// Stacks vertically when the row is narrow or the control wants the full width.
Item {
    id: row

    property string title: ""
    property string description: ""
    property string value: ""
    property color valueColor: Theme.muted
    property bool wideControl: false
    property int controlWidth: -1
    readonly property bool stacked: row.wideControl || row.width < 420

    default property alias controls: slot.data

    Layout.fillWidth: true
    implicitHeight: grid.implicitHeight + Theme.spaceMd * 2

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.rgba(Theme.border, 0.22)
        visible: row.y > 0
    }

    GridLayout {
        id: grid
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Theme.spaceMd
        anchors.rightMargin: Theme.spaceMd
        columns: row.stacked ? 1 : 2
        columnSpacing: Theme.spaceLg
        rowSpacing: Theme.spaceSm

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                text: row.title
                visible: text.length > 0
                color: Theme.foreground
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Label {
                text: row.description
                visible: text.length > 0
                color: Theme.muted
                font.pixelSize: Theme.fontCaption
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Label {
                text: row.value
                visible: text.length > 0
                color: row.valueColor
                font.pixelSize: Theme.fontCaption
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        RowLayout {
            id: slot
            spacing: Theme.spaceSm
            visible: children.length > 0
            Layout.fillWidth: row.stacked
            Layout.preferredWidth: row.controlWidth > 0 ? row.controlWidth : -1
            Layout.alignment: row.stacked ? Qt.AlignLeft : (Qt.AlignRight | Qt.AlignVCenter)
        }
    }
}
