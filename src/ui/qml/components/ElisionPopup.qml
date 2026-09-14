import QtQuick
import QtQuick.Controls

// Truncation tip as a real ToolTip/Popup so list clipping cannot hide it.
// Visibility is bound by the call site to a HoverHandler — no Overlay
// reparenting, so hover-leave keeps working.
ToolTip {
    id: root

    delay: 350
    timeout: 5000
    padding: Theme.spaceSm

    // Sit just under the label so the tip does not cover the hover target.
    x: 0
    y: parent ? parent.height + 6 : 0

    contentItem: Label {
        text: root.text
        color: Theme.foreground
        font.pixelSize: Theme.fontSmall
        wrapMode: Text.WordWrap
        width: Math.min(420 - Theme.spaceSm * 2, implicitWidth)
    }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusSm
        border.width: 1
        border.color: Theme.accent
    }
}
