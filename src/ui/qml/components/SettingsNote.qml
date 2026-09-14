import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Full-width inline note inside a SettingsGroup.
Item {
    id: note

    property string text: ""
    property string kind: "info" // info | warning | success | accent

    readonly property color tone: kind === "warning" ? Theme.warning
                                  : kind === "success" ? Theme.success
                                  : kind === "accent" ? Theme.accent
                                  : Theme.muted

    Layout.fillWidth: true
    implicitHeight: label.implicitHeight + Theme.spaceMd * 2
    visible: note.text.length > 0

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.rgba(Theme.border, 0.22)
        visible: note.y > 0
    }

    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: Theme.spaceMd
        anchors.verticalCenter: parent.verticalCenter
        width: 2
        height: label.implicitHeight
        radius: 1
        color: note.tone
        opacity: note.kind === "info" ? 0.35 : 0.8
    }

    Label {
        id: label
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Theme.spaceMd + Theme.spaceSm + 2
        anchors.rightMargin: Theme.spaceMd
        text: note.text
        color: note.kind === "info" ? Theme.muted : note.tone
        font.pixelSize: Theme.fontCaption
        wrapMode: Text.WordWrap
    }
}
