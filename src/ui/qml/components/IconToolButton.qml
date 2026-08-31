import QtQuick
import QtQuick.Controls

ToolButton {
    id: control

    property string iconName: ""
    property int iconSize: 20
    property real iconOpacity: 1

    display: AbstractButton.IconOnly
    contentItem: Image {
        width: control.iconSize
        height: control.iconSize
        anchors.centerIn: parent
        source: control.iconName.length > 0
                ? "image://themeicon/" + control.iconName + "?" + control.iconSize
                : ""
        fillMode: Image.PreserveAspectFit
        opacity: control.iconOpacity
        cache: true
    }
}
