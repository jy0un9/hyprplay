import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: card

    property alias contentItem: layout.data
    default property alias children: layout.data
    property int cardPadding: 12
    property bool elevated: false

    padding: cardPadding
    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.surface
        border.color: Theme.rgba(Theme.border, 0.55)
        border.width: 1
        layer.enabled: card.elevated
        layer.effect: null
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        spacing: 8
    }
}
