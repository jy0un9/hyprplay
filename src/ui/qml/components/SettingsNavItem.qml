import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

AbstractButton {
    id: control

    property string iconName: ""
    property bool selected: false

    implicitHeight: 48
    Accessible.name: text
    Accessible.role: Accessible.PageTab
    Accessible.checkable: true
    Accessible.checked: selected

    contentItem: RowLayout {
        spacing: Theme.spaceMd

        AppIcon {
            name: control.iconName
            size: 19
            iconColor: control.selected ? Theme.accent : Theme.foreground
            opacityFactor: control.selected ? 1 : 0.7
        }

        Label {
            text: control.text
            color: control.selected ? Theme.foreground : Theme.muted
            font.pixelSize: Theme.fontBody
            font.weight: control.selected ? Font.DemiBold : Font.Normal
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: control.selected
               ? Theme.surface
               : (control.hovered || control.pressed
                  ? Theme.rgba(Theme.selection, 0.82)
                  : "transparent")
        border.width: control.selected ? 1 : 0
        border.color: Theme.rgba(Theme.border, 0.35)

        Rectangle {
            width: 3
            height: parent.height - 16
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            radius: 2
            color: Theme.accent
            visible: control.selected
        }

        Behavior on color {
            ColorAnimation { duration: 120 }
        }
    }

    leftPadding: Theme.spaceMd + 4
    rightPadding: Theme.spaceMd
}
