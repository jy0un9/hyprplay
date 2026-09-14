import QtQuick
import QtQuick.Controls as QQC

Rectangle {
    id: handle

    property int orientation: Qt.Horizontal

    implicitWidth: orientation === Qt.Horizontal ? 5 : parent.width
    implicitHeight: orientation === Qt.Vertical ? 5 : parent.height

    color: QQC.SplitHandle.pressed
           ? Theme.rgba(Theme.accent, 0.85)
           : QQC.SplitHandle.hovered
             ? Theme.rgba(Theme.accent, 0.55)
           : Theme.rgba(Theme.border, 0.35)

    Behavior on color { ColorAnimation { duration: 120 } }

    containmentMask: Item {
        x: handle.orientation === Qt.Horizontal ? (handle.width - width) / 2 : 0
        y: handle.orientation === Qt.Vertical ? (handle.height - height) / 2 : 0
        width: handle.orientation === Qt.Horizontal ? 18 : handle.width
        height: handle.orientation === Qt.Vertical ? 18 : handle.height
    }

    HoverHandler {
        cursorShape: handle.orientation === Qt.Horizontal ? Qt.SplitHCursor : Qt.SplitVCursor
    }
}
