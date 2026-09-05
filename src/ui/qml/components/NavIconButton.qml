import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: control

    property string iconName: ""
    property color iconColor: Theme.foreground

    display: AbstractButton.TextBesideIcon

    contentItem: RowLayout {
        spacing: Theme.spaceSm
        leftPadding: control.leftPadding
        rightPadding: control.rightPadding

        Image {
            visible: control.iconName.length > 0
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            source: control.iconName.length > 0
                    ? Theme.iconUrl(control.iconName, 18, control.iconColor)
                    : ""
            fillMode: Image.PreserveAspectFit
            cache: true
        }

        Label {
            text: control.text
            font: control.font
            color: control.visualFocus ? control.palette.highlightedText : control.palette.buttonText
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
    }
}
