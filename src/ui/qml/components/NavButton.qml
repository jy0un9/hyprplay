import QtQuick
import QtQuick.Controls

AbstractButton {
    id: control

    property string iconText: ""
    property bool navHighlighted: false

    contentItem: Row {
        spacing: 10
        leftPadding: control.flat ? 10 : 12
        rightPadding: control.flat ? 10 : 12

        Label {
            text: control.iconText
            font.pixelSize: 15
            opacity: control.navHighlighted ? 1 : 0.75
            anchors.verticalCenter: parent.verticalCenter
            visible: control.iconText.length > 0
        }

        Label {
            text: control.text
            font.pixelSize: 13
            font.weight: control.navHighlighted ? Font.DemiBold : Font.Normal
            color: control.navHighlighted ? Theme.foreground : Theme.foreground
            opacity: control.navHighlighted ? 1 : 0.82
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: {
            if (control.navHighlighted)
                return Theme.rgba(Theme.accent, 0.2)
            if (control.hovered || control.pressed)
                return Theme.rgba(Theme.selection, 0.9)
            return "transparent"
        }
        border.color: control.navHighlighted ? Theme.rgba(Theme.accent, 0.35) : "transparent"
        border.width: control.navHighlighted ? 1 : 0
    }

    padding: 8
    flat: true
}
