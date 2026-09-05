import QtQuick
import QtQuick.Controls

ToolButton {
    id: control

    property string iconName: ""
    property int iconSize: 20
    property real iconOpacity: 1
    property color iconColor: Theme.foreground

    display: AbstractButton.IconOnly
    contentItem: Image {
        width: control.iconSize
        height: control.iconSize
        anchors.centerIn: parent
        source: control.iconName.length > 0
                ? Theme.iconUrl(control.iconName, control.iconSize, control.iconColor)
                : ""
        fillMode: Image.PreserveAspectFit
        opacity: control.pressed ? control.iconOpacity * 0.55 : control.iconOpacity
        cache: true
    }
}
