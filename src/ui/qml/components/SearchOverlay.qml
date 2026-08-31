import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: overlay
    modal: true
    focus: true
    padding: 0
    width: Math.min(parent ? parent.width - 80 : 560, 560)
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape

    onOpened: {
        queryField.text = App.librarySearchQuery
        queryField.forceActiveFocus()
        queryField.selectAll()
    }

    onClosed: App.closeLibrarySearch()

    Connections {
        target: App
        function onLibrarySearchOpenChanged() {
            if (App.librarySearchOpen)
                overlay.open()
            else if (overlay.visible)
                overlay.close()
        }
    }

    Component.onCompleted: {
        if (App.librarySearchOpen)
            overlay.open()
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.darkBackground
        border.color: Theme.rgba(Theme.accent, 0.45)
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 10

        TextField {
            id: queryField
            Layout.fillWidth: true
            placeholderText: scopeLabel()
            font.pixelSize: 16
            selectByMouse: true
            onTextChanged: App.setLibrarySearchQuery(text)
            Keys.onEscapePressed: overlay.close()
        }

        Label {
            Layout.fillWidth: true
            text: hintText()
            font.pixelSize: 11
            opacity: 0.6
            color: Theme.foreground
            wrapMode: Text.WordWrap
        }
    }

    function scopeLabel() {
        switch (App.librarySearchScope) {
        case "albums": return "Search albums…"
        case "tracks": return "Search tracks…"
        default: return "Search artists…"
        }
    }

    function hintText() {
        switch (App.librarySearchScope) {
        case "albums":
            return "Hover the albums column to search albums. Press Esc to close."
        case "tracks":
            return "Hover the tracks column to search tracks. Press Esc to close."
        default:
            return "Hover a library column to change scope. Press Esc to close."
        }
    }
}
