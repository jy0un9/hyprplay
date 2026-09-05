import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: card

    property alias contentItem: layout.data
    default property alias children: layout.data
    property int cardPadding: Theme.spaceMd
    property bool elevated: false

    padding: cardPadding
    background: Item {
        Rectangle {
            x: 0
            y: 2
            width: parent.width
            height: parent.height
            radius: Theme.radiusMd
            color: "black"
            opacity: 0.08
            visible: card.elevated
        }
        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusMd
            color: Theme.surface
            border.color: Theme.rgba(Theme.border, 0.35)
            border.width: 1
        }
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        spacing: Theme.spaceSm
    }
}
