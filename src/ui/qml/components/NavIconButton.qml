import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: control

    property string iconName: ""

    display: AbstractButton.TextBesideIcon

    contentItem: RowLayout {
        spacing: 8
        leftPadding: control.leftPadding
        rightPadding: control.rightPadding

        Image {
            visible: control.iconName.length > 0
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            source: control.iconName.length > 0
                    ? "image://themeicon/" + control.iconName + "?18"
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
