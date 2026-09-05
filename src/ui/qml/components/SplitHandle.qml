import QtQuick

Rectangle {
    id: handle

    property int orientation: Qt.Horizontal
    signal released()

    implicitWidth: orientation === Qt.Horizontal
                   ? (hoverHandler.hovered || pressArea.pressed ? 7 : 5)
                   : parent.width
    implicitHeight: orientation === Qt.Vertical
                    ? (hoverHandler.hovered || pressArea.pressed ? 7 : 5)
                    : parent.height

    color: pressArea.pressed || hoverHandler.hovered
           ? Theme.rgba(Theme.accent, 0.55)
           : Theme.rgba(Theme.border, 0.35)

    Behavior on color { ColorAnimation { duration: 120 } }
    Behavior on implicitWidth { NumberAnimation { duration: 80 } }
    Behavior on implicitHeight { NumberAnimation { duration: 80 } }

    HoverHandler {
        id: hoverHandler
        cursorShape: handle.orientation === Qt.Horizontal ? Qt.SplitHCursor : Qt.SplitVCursor
    }

    MouseArea {
        id: pressArea
        anchors.fill: parent
        propagateComposedEvents: true
        onPressed: (mouse) => mouse.accepted = false
        onReleased: handle.released()
    }
}
