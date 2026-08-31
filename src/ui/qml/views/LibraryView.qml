import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: libraryView
    padding: 0

    function applyLayout() {
        artistsPane.paneWidth = App.config.layoutLibraryArtistsWidth
        albumsPane.applyWidth()
    }

    function persistLayout() {
        App.config.setLayoutLibraryArtistsWidth(Math.round(artistsPane.width))
        if (App.selectedArtist.length > 0 && albumsPane.width > 0)
            App.config.setLayoutLibraryAlbumsWidth(Math.round(albumsPane.width))
    }

    Component.onCompleted: Qt.callLater(applyLayout)

    Connections {
        target: App
        function onSelectedArtistChanged() {
            albumsPane.applyWidth()
        }
    }

    background: Rectangle { color: Theme.background }

    function stripeColor(rowIndex, highlighted, hovered) {
        if (highlighted)
            return Theme.rgba(Theme.accent, 0.18)
        if (hovered)
            return Theme.rgba(Theme.selection, 0.7)
        if (rowIndex % 2 === 1)
            return Theme.rgba(Theme.selection, 0.22)
        return "transparent"
    }

    Menu {
        id: artistContextMenu
        property string artistName: ""

        MenuItem {
            text: "Edit artist tags"
            onTriggered: {
                App.selectArtist(artistContextMenu.artistName)
                App.openArtistTagEditor()
            }
        }
        MenuSeparator {}
        MenuItem {
            text: App.discogs.busy ? "Fetching from Discogs…" : "Fetch from Discogs"
            enabled: !App.discogs.busy
            onTriggered: {
                App.selectArtist(artistContextMenu.artistName)
                App.fetchDiscogsForSelectedArtist()
            }
        }
    }

    Menu {
        id: albumContextMenu
        property string albumName: ""

        MenuItem {
            text: "Edit album tags"
            onTriggered: {
                App.selectAlbum(albumContextMenu.albumName)
                App.openAlbumTagEditor()
            }
        }
    }

    Menu {
        id: trackContextMenu
        property int trackIndex: -1

        MenuItem {
            text: "Edit tags"
            onTriggered: App.openTrackTagEditor(trackContextMenu.trackIndex)
        }
    }

    component ListDelegate: ItemDelegate {
        id: listDelegate
        property int rowIndex: 0
        property int itemRadius: 4

        background: Rectangle {
            radius: listDelegate.itemRadius
            color: libraryView.stripeColor(listDelegate.rowIndex,
                                           listDelegate.highlighted,
                                           listDelegate.hovered)
        }
    }

    readonly property int searchInset: 12
    readonly property int searchReservedHeight: App.librarySearchOpen
                                                    ? searchInset + searchBarHost.height + 8
                                                    : 0

    SplitView {
        id: librarySplit
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        anchors.topMargin: libraryView.searchReservedHeight
        orientation: Qt.Horizontal

            handle: Rectangle {
                implicitWidth: 5
                color: libSplitHover.hovered ? Theme.rgba(Theme.accent, 0.55) : Theme.rgba(Theme.border, 0.35)
                HoverHandler { id: libSplitHover }

                MouseArea {
                    anchors.fill: parent
                    propagateComposedEvents: true
                    onPressed: (mouse) => mouse.accepted = false
                    onReleased: libraryView.persistLayout()
                }
            }

            Item {
                id: artistsPane
                property real paneWidth: 220
                SplitView.preferredWidth: paneWidth
                SplitView.minimumWidth: 140
                SplitView.maximumWidth: 420

                HoverHandler {
                    onHoveredChanged: if (hovered) App.setLibrarySearchScope("artists")
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    Label {
                        text: "Artists"
                        font.bold: true
                        color: Theme.foreground
                        Layout.leftMargin: 12
                        Layout.topMargin: 12
                    }

                    Label {
                        text: "Press / to search"
                        font.pixelSize: 11
                        opacity: 0.45
                        color: Theme.foreground
                        Layout.leftMargin: 12
                        Layout.rightMargin: 12
                        visible: !App.librarySearchOpen
                    }

                    ListView {
                        id: artistList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        model: App.artists

                        delegate: ListDelegate {
                            width: artistList.width
                            rowIndex: index
                            text: model.name
                            highlighted: App.selectedArtist === model.name
                            onClicked: App.selectArtist(model.name)

                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: {
                                    artistContextMenu.artistName = model.name
                                    artistContextMenu.popup()
                                }
                            }
                        }
                    }
                }
            }

            Item {
                id: albumsPane
                property real paneWidth: 0

                function applyWidth() {
                    paneWidth = App.selectedArtist.length > 0 ? App.config.layoutLibraryAlbumsWidth : 0
                }

                SplitView.preferredWidth: paneWidth
                SplitView.minimumWidth: App.selectedArtist.length > 0 ? 140 : 0
                SplitView.maximumWidth: App.selectedArtist.length > 0 ? 420 : 0

                HoverHandler {
                    onHoveredChanged: if (hovered) App.setLibrarySearchScope("albums")
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8
                    visible: App.selectedArtist.length > 0

                    Label {
                        text: App.selectedArtist
                        font.bold: true
                        color: Theme.foreground
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        Layout.leftMargin: 12
                        Layout.topMargin: 12
                        Layout.rightMargin: 12
                    }

                    ListView {
                        id: albumList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        model: App.albums

                        delegate: ListDelegate {
                            width: albumList.width
                            rowIndex: index
                            text: modelData
                            highlighted: App.selectedAlbum === modelData
                            onClicked: App.selectAlbum(modelData)

                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: {
                                    albumContextMenu.albumName = modelData
                                    albumContextMenu.popup()
                                }
                            }
                            TapHandler {
                                acceptedButtons: Qt.LeftButton
                                onDoubleTapped: {
                                    App.selectAlbum(modelData)
                                    App.playAlbum()
                                }
                            }
                        }
                    }
                }
            }

            Item {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 240

                HoverHandler {
                    onHoveredChanged: if (hovered) App.setLibrarySearchScope("tracks")
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    Label {
                        text: App.selectedAlbum.length > 0 ? App.selectedAlbum : "Tracks"
                        font.pixelSize: 17
                        font.bold: true
                        color: Theme.foreground
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        Layout.topMargin: 12
                        Layout.leftMargin: 12
                        Layout.rightMargin: 12
                        visible: App.selectedAlbum.length > 0
                                  || (App.librarySearchOpen && App.librarySearchScope === "tracks")
                    }

                    Label {
                        text: "Select an artist and album"
                        opacity: 0.45
                        color: Theme.foreground
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 24
                        visible: App.selectedAlbum.length === 0
                                  && !(App.librarySearchOpen && App.librarySearchScope === "tracks")
                    }

                    ListView {
                        id: trackList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        model: App.tracks

                        delegate: ItemDelegate {
                            id: trackDelegate
                            width: trackList.width

                            background: Rectangle {
                                color: libraryView.stripeColor(index,
                                                                 false,
                                                                 trackDelegate.hovered)
                            }

                            contentItem: RowLayout {
                                spacing: 8
                                Label {
                                    text: model.trackNumber > 0 ? model.trackNumber : index + 1
                                    opacity: 0.55
                                    color: Theme.foreground
                                    Layout.preferredWidth: 28
                                }
                                Label {
                                    text: model.title
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    color: Theme.foreground
                                }
                                Label {
                                    text: formatDuration(model.durationMs)
                                    opacity: 0.55
                                    color: Theme.foreground
                                }
                            }

                            onClicked: App.playTrackIndex(index)

                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: {
                                    trackContextMenu.trackIndex = index
                                    trackContextMenu.popup()
                                }
                            }
                        }
                    }
                }
            }
    }

    Item {
        id: searchBarHost
        visible: App.librarySearchOpen
        z: 1
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: libraryView.searchInset
        anchors.rightMargin: libraryView.searchInset
        anchors.topMargin: libraryView.searchInset
        implicitHeight: searchRow.implicitHeight + 16
        height: implicitHeight

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusSm
            color: Theme.rgba(Theme.accent, 0.08)
            border.color: Theme.rgba(Theme.accent, 0.35)
            border.width: 1

            RowLayout {
                id: searchRow
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Rectangle {
                    Layout.preferredWidth: scopeBadge.implicitWidth + 12
                    Layout.preferredHeight: scopeBadge.implicitHeight + 8
                    radius: 4
                    color: Theme.rgba(Theme.accent, 0.12)

                    Label {
                        id: scopeBadge
                        anchors.centerIn: parent
                        text: scopeHint()
                        font.pixelSize: 11
                        font.bold: true
                        color: Theme.accent
                    }
                }

                TextField {
                    id: librarySearchField
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    placeholderText: searchPlaceholder()
                    selectByMouse: true
                    onTextChanged: {
                        if (App.librarySearchQuery !== text)
                            App.setLibrarySearchQuery(text)
                    }
                    Keys.onEscapePressed: function(event) {
                        event.accepted = true
                        App.closeLibrarySearch()
                    }
                }

                ToolButton {
                    Layout.alignment: Qt.AlignVCenter
                    text: "Esc"
                    display: AbstractButton.TextOnly
                    onClicked: App.closeLibrarySearch()
                }
            }
        }
    }

    Connections {
        target: App
        function onLibrarySearchOpenChanged() {
            if (!App.librarySearchOpen) {
                librarySearchField.text = ""
                librarySearchField.focus = false
                libraryView.forceActiveFocus()
                return
            }
            librarySearchField.text = App.librarySearchQuery
            librarySearchField.forceActiveFocus()
            if (App.librarySearchQuery.length === 0)
                librarySearchField.selectAll()
        }
        function onLibrarySearchFocusRequested() {
            librarySearchField.forceActiveFocus()
            librarySearchField.selectAll()
        }
    }

    function searchPlaceholder() {
        switch (App.librarySearchScope) {
        case "albums": return "Search albums…"
        case "tracks": return "Search tracks…"
        default: return "Search artists…"
        }
    }

    function scopeHint() {
        switch (App.librarySearchScope) {
        case "albums": return "Albums"
        case "tracks": return "Tracks"
        default: return "Artists"
        }
    }

    function formatDuration(ms) {
        if (!ms || ms <= 0) return "--:--"
        var totalSec = Math.floor(ms / 1000)
        var min = Math.floor(totalSec / 60)
        var sec = totalSec % 60
        return min + ":" + (sec < 10 ? "0" : "") + sec
    }
}
