import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Button {
    id: control

    Material.roundedScale: Material.SmallScale

    Accessible.name: control.text
    Accessible.role: Accessible.Button

    scale: control.pressed ? 0.98 : 1
    Behavior on scale { NumberAnimation { duration: 80 } }

    contentItem: Label {
        text: control.text
        font: control.font
        color: control.enabled ? "white" : Theme.rgba(Theme.foreground, 0.5)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: {
            if (!control.enabled)
                return Theme.rgba(Theme.muted, 0.35)
            if (control.pressed)
                return Qt.darker(Theme.accent, 1.2)
            if (control.hovered)
                return Qt.lighter(Theme.accent, 1.08)
            return Theme.accent
        }

        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
