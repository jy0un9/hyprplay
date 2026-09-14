import QtQuick
import QtQuick.Controls

ItemDelegate {
    id: control

    property int itemRadius: 0
    property bool searchHighlight: false
    property bool accented: control.highlighted
    property bool multiSelected: false
    property string accessibleName: control.text
    leftPadding: Theme.spaceSm + 4
    rightPadding: Theme.spaceSm
    topPadding: Theme.spaceSm
    bottomPadding: Theme.spaceSm

    Accessible.name: accessibleName
    Accessible.role: Accessible.ListItem
    Accessible.checkable: true
    Accessible.checked: control.highlighted || control.multiSelected

    background: Rectangle {
        radius: control.itemRadius
        color: {
            if (control.highlighted || control.multiSelected)
                return Theme.rgba(Theme.accent, control.highlighted ? 0.22 : 0.14)
            if (control.hovered)
                return Theme.rgba(Theme.selection, 0.85)
            return "transparent"
        }
        Behavior on color {
            ColorAnimation { duration: 120 }
        }

        Rectangle {
            width: 3
            height: parent.height - 8
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            radius: 0
            color: Theme.accent
            opacity: control.accented ? 1 : 0

            Behavior on opacity {
                NumberAnimation { duration: 120 }
            }
        }
    }

    contentItem: Label {
        id: delegateLabel
        text: control.searchHighlight ? App.highlightSearchMatch(control.text, Theme.accent) : control.text
        textFormat: control.searchHighlight ? Text.RichText : Text.PlainText
        font: control.font
        color: control.highlighted ? Theme.foreground : Theme.foreground
        opacity: control.enabled ? (control.highlighted ? 1 : 0.92) : 0.45
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        HoverHandler { id: delegateHover }
        ElisionPopup {
            visible: delegateHover.hovered && delegateLabel.truncated
            text: control.text
        }
    }
}
