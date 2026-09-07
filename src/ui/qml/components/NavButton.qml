import QtQuick
import QtQuick.Controls

AbstractButton {
    id: control

    property string iconText: ""
    property bool navHighlighted: false

    Accessible.name: control.text
    Accessible.role: Accessible.PageTab
    Accessible.checkable: true
    Accessible.checked: control.navHighlighted

    contentItem: Row {
        spacing: 10
        leftPadding: 10
        rightPadding: 10

        Label {
            text: control.iconText
            font.pixelSize: Theme.fontSubtitle
            opacity: control.navHighlighted ? 1 : 0.75
            anchors.verticalCenter: parent.verticalCenter
            visible: control.iconText.length > 0
        }

        Label {
            text: control.text
            font.pixelSize: Theme.fontBody
            font.weight: control.navHighlighted ? Font.DemiBold : Font.Normal
            color: control.navHighlighted ? Theme.accent : Theme.foreground
            opacity: control.navHighlighted ? 1 : 0.82
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        radius: 0
        color: {
            if (control.hovered || control.pressed)
                return Theme.rgba(Theme.selection, 0.9)
            return "transparent"
        }

        Behavior on color {
            ColorAnimation { duration: 120 }
        }
    }

    padding: Theme.spaceSm
}
