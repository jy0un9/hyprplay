import QtQuick
import QtQuick.Controls

Rectangle {
    id: handle

    implicitWidth: orientation === Qt.Horizontal ? 5 : parent.width
    implicitHeight: orientation === Qt.Vertical ? 5 : parent.height

    property int orientation: Qt.Horizontal

    color: pressedHandler.pressed || hoverHandler.hovered
           ? Theme.rgba(Theme.accent, 0.55)
           : Theme.rgba(Theme.border, 0.35)

    HoverHandler { id: hoverHandler }
    PressHandler { id: pressedHandler }
}
