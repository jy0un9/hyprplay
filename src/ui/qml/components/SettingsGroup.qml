import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A titled block of settings rows inside one flat, hairline-bordered frame.
ColumnLayout {
    id: group

    property string title: ""
    default property alias content: body.data

    Layout.fillWidth: true
    spacing: Theme.spaceSm

    SectionLabel {
        title: group.title
        visible: group.title.length > 0
        Layout.leftMargin: 2
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: body.implicitHeight
        radius: Theme.radiusSm
        color: Theme.rgba(Theme.surface, 0.5)
        border.width: 1
        border.color: Theme.rgba(Theme.border, 0.3)

        ColumnLayout {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 0
        }
    }
}
