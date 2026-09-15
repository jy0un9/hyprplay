import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    property string message: ""
    property string kind: "info"
    property int serial: 0
    property string shownMessage: ""
    property string shownKind: "info"

    readonly property color barColor: {
        switch (root.shownKind) {
        case "success": return Theme.success
        case "error": return Theme.error
        default: return Theme.accent
        }
    }

    width: Math.min(parent ? parent.width - 48 : 420, Math.max(280, noteLabel.implicitWidth + 56))
    implicitHeight: Math.max(40, noteLabel.implicitHeight + 20)
    radius: Theme.radiusMd
    color: Theme.chrome
    border.color: Theme.rgba(root.barColor, 0.55)
    border.width: 1
    opacity: 0
    visible: opacity > 0

    Behavior on opacity { NumberAnimation { duration: 180 } }

    Rectangle {
        width: 3
        height: parent.height - 12
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        radius: 1
        color: root.barColor
    }

    Label {
        id: noteLabel
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 12
        anchors.topMargin: 10
        anchors.bottomMargin: 10
        verticalAlignment: Text.AlignVCenter
        text: root.shownMessage
        color: Theme.chromeIcon
        font.pixelSize: Theme.fontBody
        wrapMode: Text.WordWrap
        elide: Text.ElideRight
        maximumLineCount: 2
    }

    Timer {
        id: hideTimer
        interval: 3800
        onTriggered: root.opacity = 0
    }

    function showNotice(text, kind) {
        root.shownMessage = text
        root.shownKind = kind && kind.length > 0 ? kind : "info"
        root.opacity = 1
        hideTimer.restart()
    }

    onSerialChanged: {
        if (root.serial > 0 && root.message.length > 0)
            showNotice(root.message, root.kind)
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            hideTimer.stop()
            root.opacity = 0
        }
    }
}
