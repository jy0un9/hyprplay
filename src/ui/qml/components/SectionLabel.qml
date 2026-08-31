import QtQuick
import QtQuick.Controls

Label {
    property string title: ""

    font.pixelSize: 11
    font.weight: Font.DemiBold
    font.letterSpacing: 0.8
    opacity: 0.55
    color: Theme.foreground
    text: title.toUpperCase()
}
