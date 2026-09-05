import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property string iconName: ""
    property string title: ""
    property string subtitle: ""
    property string actionText: ""
    property bool loading: false
    signal actionClicked()

    spacing: Theme.spaceMd

    AppIcon {
        name: root.iconName
        size: 48
        opacityFactor: root.loading ? 0 : 0.45
        visible: !root.loading
        Layout.alignment: Qt.AlignHCenter
    }

    BusyIndicator {
        running: root.loading
        visible: root.loading
        Layout.alignment: Qt.AlignHCenter
    }

    Label {
        text: root.loading ? "Scanning…" : root.title
        font.pixelSize: Theme.fontTitle
        font.bold: true
        color: Theme.foreground
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        Layout.maximumWidth: 320
        Layout.alignment: Qt.AlignHCenter
    }

    Label {
        text: root.loading ? App.library.scanStatus : root.subtitle
        font.pixelSize: Theme.fontBody
        color: Theme.foreground
        opacity: 0.6
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        visible: text.length > 0
        Layout.fillWidth: true
        Layout.maximumWidth: 340
        Layout.alignment: Qt.AlignHCenter
    }

    PrimaryButton {
        text: root.actionText
        visible: root.actionText.length > 0 && !root.loading
        Layout.alignment: Qt.AlignHCenter
        onClicked: root.actionClicked()
    }
}
