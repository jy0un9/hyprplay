import QtQuick
import QtQuick.Controls

ItemDelegate {
    id: control

    property int itemRadius: Theme.radiusSm

    background: Rectangle {
        radius: control.itemRadius
        color: {
            if (control.highlighted)
                return Theme.rgba(Theme.accent, 0.22)
            if (control.hovered)
                return Theme.rgba(Theme.selection, 0.85)
            return "transparent"
        }
        border.color: control.highlighted ? Theme.rgba(Theme.accent, 0.45) : "transparent"
        border.width: control.highlighted ? 1 : 0

        Behavior on color {
            ColorAnimation { duration: 120 }
        }
    }

    contentItem: Label {
        text: control.text
        font: control.font
        color: control.highlighted ? Theme.foreground : Theme.foreground
        opacity: control.enabled ? (control.highlighted ? 1 : 0.92) : 0.45
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
}
