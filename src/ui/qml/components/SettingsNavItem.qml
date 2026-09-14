import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

AbstractButton {
    id: control

    property string iconName: ""
    property bool selected: false
    property string badge: ""

    implicitHeight: 32
    Accessible.name: text
    Accessible.role: Accessible.PageTab
    Accessible.checkable: true
    Accessible.checked: selected

    contentItem: RowLayout {
        spacing: Theme.spaceSm

        AppIcon {
            name: control.iconName
            size: 15
            iconColor: control.selected ? Theme.accent : Theme.foreground
            opacityFactor: control.selected ? 1 : 0.55
        }

        Label {
            text: control.text
            color: control.selected ? Theme.accent : Theme.foreground
            opacity: control.selected ? 1 : 0.8
            font.pixelSize: Theme.fontBody
            font.weight: control.selected ? Font.DemiBold : Font.Normal
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Label {
            text: control.badge
            visible: control.badge.length > 0
            color: Theme.accent
            font.pixelSize: Theme.fontCaption
        }
    }

    background: Rectangle {
        color: control.selected
               ? Theme.rgba(Theme.selection, 0.9)
               : (control.hovered || control.pressed
                  ? Theme.rgba(Theme.selection, 0.5)
                  : "transparent")

        Rectangle {
            width: 2
            height: parent.height
            anchors.left: parent.left
            color: Theme.accent
            visible: control.selected
        }

        Behavior on color {
            ColorAnimation { duration: 110 }
        }
    }

    leftPadding: Theme.spaceMd
    rightPadding: Theme.spaceMd
}
