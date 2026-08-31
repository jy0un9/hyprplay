import QtQuick
import QtQuick.Controls

Item {
    id: root

    property string name: ""
    property int size: 24
    property real opacityFactor: 1

    implicitWidth: size
    implicitHeight: size

    Image {
        anchors.centerIn: parent
        width: root.size
        height: root.size
        source: root.name.length > 0 ? "image://themeicon/" + root.name + "?" + root.size : ""
        fillMode: Image.PreserveAspectFit
        opacity: root.opacityFactor
        cache: true
    }
}
