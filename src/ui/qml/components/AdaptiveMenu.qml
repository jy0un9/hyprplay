import QtQuick
import QtQuick.Controls

Menu {
    id: control

    property int minimumMenuWidth: 220
    property int maximumMenuWidth: 640

    implicitWidth: minimumMenuWidth
    leftPadding: 2
    rightPadding: 2

    delegate: ThemedMenuItem {}

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusSm
        border.width: 1
        border.color: Theme.accent
    }

    TextMetrics {
        id: textMetrics
        font: control.font
    }

    function resizeToContents() {
        let measuredWidth = minimumMenuWidth
        for (let i = 0; i < count; ++i) {
            const menuItem = itemAt(i)
            if (!menuItem)
                continue
            const label = menuItem.text !== undefined
                          ? menuItem.text
                          : (menuItem.title !== undefined ? menuItem.title : "")
            if (!label || label.length === 0)
                continue
            textMetrics.text = label
            // Allow for item padding, icons/checkmarks, and submenu arrows.
            measuredWidth = Math.max(measuredWidth, Math.ceil(textMetrics.advanceWidth) + 88)
        }
        width = Math.min(maximumMenuWidth, measuredWidth)
    }

    onAboutToShow: resizeToContents()
    onCountChanged: {
        if (visible)
            Qt.callLater(resizeToContents)
    }
}
