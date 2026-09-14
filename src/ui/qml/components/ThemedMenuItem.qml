import QtQuick
import QtQuick.Controls

MenuItem {
    id: control

    background: Rectangle {
        color: control.highlighted || control.hovered || control.down
               ? Theme.selection
               : "transparent"
        radius: Math.max(0, Theme.radiusSm - 2)
    }
}
