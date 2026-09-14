import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import components 1.0

Pane {
    id: libraryView
    padding: 0

    function applyLayout() {
        artistsPane.paneWidth = App.config.layoutLibraryArtistsWidth
        albumsPane.applyWidth()
    }

    function currentTrackIndex() {
        if (App.selectedTrackIndex >= 0 && App.selectedTrackIndex < App.tracks.count)
            return App.selectedTrackIndex
        for (let i = 0; i < App.tracks.count; i++) {
            const track = App.tracks.trackAt(i)
            if (track.path === App.playback.currentPath && track.path.length > 0)
                return i
        }
        return 0
    }

    function playSelected() {
        if (App.tracks.count > 0)
            App.playTrackIndex(Math.max(0, currentTrackIndex()))
        else
            App.playAlbum()
    }

    function persistLayout() {
        App.config.setLayoutLibraryArtistsWidth(Math.round(artistsPane.width))
        if (App.selectedArtist.length > 0 && albumsPane.width > 0)
            App.config.setLayoutLibraryAlbumsWidth(Math.round(albumsPane.width))
    }

    Component.onCompleted: Qt.callLater(applyLayout)

    function scrollSelectionIntoView() {
        if (App.selectedTrackIndex >= 0 && App.selectedTrackIndex < trackList.count)
            trackList.positionViewAtIndex(App.selectedTrackIndex, ListView.Contain)
        if (App.selectedArtist.length > 0) {
            for (let i = 0; i < artistList.count; i++) {
                if (App.artists.artistAt(i) === App.selectedArtist) {
                    artistList.positionViewAtIndex(i, ListView.Center)
                    break
                }
            }
        }
        if (App.selectedAlbum.length > 0) {
            for (let j = 0; j < albumList.count; j++) {
                if (albumList.model[j] === App.selectedAlbum) {
                    albumList.positionViewAtIndex(j, ListView.Contain)
                    break
                }
            }
        }
    }

    Connections {
        target: App
        function onSelectedArtistChanged() {
            albumsPane.applyWidth()
        }
        function onSelectionChanged() {
            // Defer until ListView model rows are committed after search close/refresh.
            Qt.callLater(libraryView.scrollSelectionIntoView)
        }
    }

    background: Rectangle { color: Theme.background }

    AdaptiveMenu {
        id: artistContextMenu
        property string artistName: ""

        ThemedMenuItem {
            text: "Edit artist tags"
            Accessible.name: text
            onTriggered: {
                App.selectArtist(artistContextMenu.artistName)
                App.openArtistTagEditor()
            }
        }
        MenuSeparator {}
        ThemedMenuItem {
            text: App.discogs.busy ? "Fetching from Discogs…" : "Fetch from Discogs"
            Accessible.name: text
            enabled: !App.discogs.busy && !App.bulkDiscogsAlbumsActive
            onTriggered: {
                App.selectArtist(artistContextMenu.artistName)
                App.fetchDiscogsForSelectedArtist()
            }
        }
        ThemedMenuItem {
            text: App.bulkDiscogsAlbumsActive
                  ? "Fetching album info…"
                  : "Fetch album info for all albums"
            Accessible.name: text
            enabled: !App.discogs.busy && !App.bulkDiscogsAlbumsActive
            onTriggered: {
                App.selectArtist(artistContextMenu.artistName)
                App.fetchDiscogsForAllArtistAlbums()
            }
        }
        MenuSeparator {}
        ThemedMenuItem {
            text: App.lyrics.busy ? "Fetching lyrics…" : "Fetch lyrics for all tracks"
            Accessible.name: text
            enabled: !App.lyrics.busy
            onTriggered: {
                App.selectArtist(artistContextMenu.artistName)
                App.fetchLyricsForArtist(artistContextMenu.artistName)
            }
        }
    }

    AdaptiveMenu {
        id: albumContextMenu
        property string albumName: ""

        ThemedMenuItem {
            text: "Edit album tags"
            Accessible.name: text
            onTriggered: {
                App.selectAlbum(albumContextMenu.albumName)
                App.openAlbumTagEditor()
            }
        }
        ThemedMenuItem {
            text: App.discogs.busy ? "Fetching from Discogs…" : "Fetch album info from Discogs"
            Accessible.name: text
            enabled: !App.discogs.busy && !App.bulkDiscogsAlbumsActive
            onTriggered: {
                App.selectAlbum(albumContextMenu.albumName)
                App.openDiscogsForSelectedAlbum()
            }
        }
        ThemedMenuItem {
            text: App.metadataSearch.searching && App.titleFixOpen ? "Fixing titles…" : "Title Fix"
            Accessible.name: text
            enabled: !App.metadataSearch.searching || !App.titleFixOpen
            onTriggered: {
                App.selectAlbum(albumContextMenu.albumName)
                App.openTitleFix()
            }
        }
        MenuSeparator {}
        AdaptiveMenu {
            id: addAlbumToPlaylistMenu
            title: "Add album to playlist"

            Instantiator {
                model: App.playlistItems
                delegate: ThemedMenuItem {
                    text: model.name
                    Accessible.name: "Add album to " + model.name
                    onTriggered: {
                        App.selectAlbum(albumContextMenu.albumName)
                        App.addAlbumToPlaylist(model.name)
                    }
                }
                onObjectAdded: (index, object) => addAlbumToPlaylistMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => addAlbumToPlaylistMenu.removeItem(object)
            }
            ThemedMenuItem {
                text: "No playlists yet — create one in Playlists"
                Accessible.name: text
                enabled: false
                visible: App.playlistItems.count === 0
                height: visible ? implicitHeight : 0
            }
        }
        MenuSeparator {}
        ThemedMenuItem {
            text: App.lyrics.busy ? "Fetching lyrics…" : "Fetch lyrics for album"
            Accessible.name: text
            enabled: !App.lyrics.busy
            onTriggered: {
                App.selectAlbum(albumContextMenu.albumName)
                App.fetchLyricsForAlbum(App.selectedArtist, albumContextMenu.albumName)
            }
        }
    }

    AdaptiveMenu {
        id: trackContextMenu
        property int trackIndex: -1

        ThemedMenuItem {
            text: "Edit tags"
            Accessible.name: text
            enabled: App.multiSelectedTrackCount <= 1
            onTriggered: App.openTrackTagEditor(trackContextMenu.trackIndex)
        }
        MenuSeparator {}
        AdaptiveMenu {
            id: addTrackToPlaylistMenu
            title: App.multiSelectedTrackCount > 1
                   ? ("Add " + App.multiSelectedTrackCount + " tracks to playlist")
                   : "Add to playlist"

            Instantiator {
                model: App.playlistItems
                delegate: ThemedMenuItem {
                    text: model.name
                    Accessible.name: addTrackToPlaylistMenu.title + " " + model.name
                    onTriggered: App.addSelectedTracksToPlaylist(model.name)
                }
                onObjectAdded: (index, object) => addTrackToPlaylistMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => addTrackToPlaylistMenu.removeItem(object)
            }
            ThemedMenuItem {
                text: "No playlists yet — create one in Playlists"
                Accessible.name: text
                enabled: false
                visible: App.playlistItems.count === 0
                height: visible ? implicitHeight : 0
            }
        }
        MenuSeparator {}
        ThemedMenuItem {
            text: App.lyrics.busy ? "Fetching lyrics…" : "Fetch lyrics"
            Accessible.name: text
            enabled: !App.lyrics.busy && App.multiSelectedTrackCount <= 1
            onTriggered: App.fetchLyricsForTrackIndex(trackContextMenu.trackIndex)
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
        onResizingChanged: {
            if (!resizing)
                libraryView.persistLayout()
        }

        handle: SplitHandle {
            orientation: Qt.Horizontal
        }

            Item {
                id: artistsPane
                property real paneWidth: 220
                clip: true
                SplitView.preferredWidth: paneWidth
                SplitView.minimumWidth: 140
                SplitView.maximumWidth: 420

                HoverHandler {
                    onHoveredChanged: {
                        if (hovered) {
                            App.setLibrarySearchScope("artists")
                            App.setLibraryFocusColumn("artists")
                        }
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spaceSm

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.spaceMd
                        Layout.rightMargin: Theme.spaceMd
                        Layout.topMargin: Theme.spaceMd
                        spacing: 4

                        SectionLabel {
                            title: "Artists"
                            Layout.fillWidth: true
                        }

                        Label {
                            id: artistsHeaderDetail
                            text: {
                                if (App.selectedArtist.length > 0)
                                    return App.selectedArtist
                                if (artistList.count > 0)
                                    return artistList.count
                                           + (artistList.count === 1 ? " artist" : " artists")
                                return " "
                            }
                            font.pixelSize: App.selectedArtist.length > 0
                                            ? Theme.fontHeading : Theme.fontCaption
                            font.bold: App.selectedArtist.length > 0
                            opacity: App.selectedArtist.length > 0 ? 1.0 : 0.5
                            color: Theme.foreground
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            visible: !App.librarySearchOpen
                                     || App.librarySearchQuery.trimmed().length === 0
                                     || App.librarySearchScope !== "artists"
                            HoverHandler { id: artistsHeaderHover }
                            ElisionPopup {
                                visible: artistsHeaderHover.hovered
                                         && artistsHeaderDetail.truncated
                                         && App.selectedArtist.length > 0
                                text: artistsHeaderDetail.text
                            }
                        }

                        Label {
                            text: searchCountText()
                            font.pixelSize: Theme.fontCaption
                            opacity: 0.6
                            color: Theme.accent
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            visible: App.librarySearchOpen && App.librarySearchQuery.trimmed().length > 0
                                     && App.librarySearchScope === "artists"
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.topMargin: 2
                            height: 1
                            color: Theme.rgba(Theme.foreground, 0.08)
                        }
                    }

                    ProgressBar {
                        Layout.leftMargin: Theme.spaceMd
                        Layout.rightMargin: Theme.spaceMd
                        Layout.fillWidth: true
                        from: 0
                        to: Math.max(1, App.lyrics.total)
                        value: App.lyrics.progress
                        visible: App.lyrics.busy
                    }

                    ToolButton {
                        text: "Cancel lyrics fetch"
                        Layout.leftMargin: Theme.spaceMd
                        visible: App.lyrics.busy
                        onClicked: App.lyrics.cancel()
                    }

                    ListView {
                        id: artistList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        model: App.artists
                        visible: count > 0 || App.library.scanning

                        delegate: AppListDelegate {
                            width: artistList.width
                            itemRadius: Theme.radiusSm
                            text: model.name
                            searchHighlight: App.librarySearchOpen
                                             && App.librarySearchScope === "artists"
                            highlighted: App.selectedArtist === model.name
                            accented: App.libraryFocusColumn === "artists"
                                      && App.selectedArtist === model.name
                            onClicked: {
                                App.selectArtist(model.name)
                                if (App.librarySearchOpen)
                                    App.acceptLibrarySearch()
                            }

                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: {
                                    artistContextMenu.artistName = model.name
                                    artistContextMenu.popup()
                                }
                            }
                        }
                    }

                    EmptyState {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        iconName: "folder-music-symbolic"
                        title: "No music found"
                        subtitle: "Set your Music folder in Settings, then rescan."
                        actionText: "Open Settings"
                        loading: App.library.scanning
                        visible: artistList.count === 0 && !App.library.scanning
                        onActionClicked: App.showSettings()
                    }
                }
            }

            Item {
                id: albumsPane
                property real paneWidth: 0
                clip: true
                visible: App.selectedArtist.length > 0

                function applyWidth() {
                    paneWidth = App.selectedArtist.length > 0 ? App.config.layoutLibraryAlbumsWidth : 0
                }

                SplitView.preferredWidth: paneWidth
                SplitView.minimumWidth: App.selectedArtist.length > 0 ? 140 : 0
                SplitView.maximumWidth: App.selectedArtist.length > 0 ? 420 : 0

                HoverHandler {
                    onHoveredChanged: {
                        if (hovered) {
                            App.setLibrarySearchScope("albums")
                            App.setLibraryFocusColumn("albums")
                        }
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spaceSm
                    visible: App.selectedArtist.length > 0

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.spaceMd
                        Layout.rightMargin: Theme.spaceMd
                        Layout.topMargin: Theme.spaceMd
                        spacing: 4

                        SectionLabel {
                            title: "Albums"
                            Layout.fillWidth: true
                        }

                        Label {
                            id: selectedArtistLabel
                            text: App.selectedArtist
                            font.pixelSize: Theme.fontHeading
                            font.bold: true
                            color: Theme.foreground
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            HoverHandler { id: selectedArtistHover }
                            ElisionPopup {
                                visible: selectedArtistHover.hovered && selectedArtistLabel.truncated
                                text: selectedArtistLabel.text
                            }
                        }

                        Label {
                            text: searchCountText()
                            font.pixelSize: Theme.fontCaption
                            opacity: 0.6
                            color: Theme.accent
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            visible: App.librarySearchOpen && App.librarySearchQuery.trimmed().length > 0
                                     && App.librarySearchScope === "albums"
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.topMargin: 2
                            height: 1
                            color: Theme.rgba(Theme.foreground, 0.08)
                        }
                    }

                    ListView {
                        id: albumList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        model: App.albums

                        delegate: AppListDelegate {
                            width: albumList.width
                            itemRadius: Theme.radiusSm
                            text: modelData
                            searchHighlight: App.librarySearchOpen
                                             && App.librarySearchScope === "albums"
                            highlighted: App.selectedAlbum === modelData
                            accented: App.libraryFocusColumn === "albums"
                                      && App.selectedAlbum === modelData
                            onClicked: {
                                App.selectAlbum(modelData)
                                if (App.librarySearchOpen)
                                    App.acceptLibrarySearch()
                            }

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
                clip: true

                HoverHandler {
                    onHoveredChanged: {
                        if (hovered) {
                            App.setLibrarySearchScope("tracks")
                            App.setLibraryFocusColumn("tracks")
                        }
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spaceSm

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.spaceMd
                        Layout.leftMargin: Theme.spaceMd
                        Layout.rightMargin: Theme.spaceMd
                        spacing: 4

                        SectionLabel {
                            title: "Tracks"
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spaceSm
                            visible: App.selectedAlbum.length > 0
                                      || (App.librarySearchOpen && App.librarySearchScope === "tracks")

                            Image {
                                Layout.preferredWidth: 44
                                Layout.preferredHeight: 44
                                fillMode: Image.PreserveAspectCrop
                                source: App.selectedAlbumArtUrl
                                visible: source.length > 0
                                cache: true
                            }

                            Label {
                                id: selectedAlbumLabel
                                text: App.selectedAlbum.length > 0 ? App.selectedAlbum : "Tracks"
                                font.pixelSize: Theme.fontHeading
                                font.bold: true
                                color: Theme.foreground
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                HoverHandler { id: selectedAlbumHover }
                                ElisionPopup {
                                    visible: selectedAlbumHover.hovered && selectedAlbumLabel.truncated
                                    text: selectedAlbumLabel.text
                                }
                            }
                        }

                        Label {
                            text: searchCountText()
                            font.pixelSize: Theme.fontCaption
                            opacity: 0.6
                            color: Theme.accent
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            visible: App.librarySearchOpen && App.librarySearchQuery.trimmed().length > 0
                                     && App.librarySearchScope === "tracks"
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.topMargin: 2
                            height: 1
                            color: Theme.rgba(Theme.foreground, 0.08)
                            visible: App.selectedAlbum.length > 0
                                      || (App.librarySearchOpen && App.librarySearchScope === "tracks")
                        }
                    }

                    Label {
                        text: "Select an artist and album"
                        opacity: 0.45
                        color: Theme.foreground
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: Theme.spaceXl
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
                        currentIndex: App.selectedTrackIndex

                        onCountChanged: {
                            if (App.selectedTrackIndex >= 0)
                                positionViewAtIndex(App.selectedTrackIndex, ListView.Contain)
                        }

                        delegate: AppListDelegate {
                            id: trackDelegate
                            width: trackList.width
                            itemRadius: Theme.radiusSm
                            text: model.title
                            accessibleName: (model.trackNumber > 0 ? model.trackNumber + ". " : "")
                                             + model.title
                            searchHighlight: App.librarySearchOpen
                                             && App.librarySearchScope === "tracks"
                            highlighted: index === App.selectedTrackIndex
                            multiSelected: App.multiSelectedTrackIndices.indexOf(index) >= 0
                            accented: App.libraryFocusColumn === "tracks"
                                      && index === App.selectedTrackIndex

                            contentItem: RowLayout {
                                spacing: Theme.spaceSm

                                Label {
                                    text: model.trackNumber > 0 ? model.trackNumber : index + 1
                                    opacity: (App.playback.currentPath === model.path
                                              || index === App.selectedTrackIndex
                                              || trackDelegate.multiSelected) ? 1 : 0.55
                                    color: Theme.foreground
                                    Layout.preferredWidth: 28
                                }

                                Label {
                                    id: libraryTrackTitleLabel
                                    text: trackDelegate.searchHighlight
                                          ? App.highlightSearchMatch(model.title, Theme.accent)
                                          : model.title
                                    textFormat: trackDelegate.searchHighlight ? Text.RichText : Text.PlainText
                                    font: trackDelegate.font
                                    color: Theme.foreground
                                    opacity: trackDelegate.enabled
                                             ? (trackDelegate.highlighted || trackDelegate.multiSelected ? 1 : 0.92) : 0.45
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                    HoverHandler { id: libraryTrackTitleHover }
                                    ElisionPopup {
                                        visible: libraryTrackTitleHover.hovered && libraryTrackTitleLabel.truncated
                                        text: model.title
                                    }

                                }

                                Label {
                                    text: "▶"
                                    visible: App.playback.currentPath === model.path
                                             && App.playback.currentPath.length > 0
                                    color: Theme.accent
                                    font.pixelSize: Theme.fontSmall
                                }

                                Label {
                                    text: formatDuration(model.durationMs)
                                    opacity: 0.55
                                    color: Theme.foreground
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                onClicked: function(mouse) {
                                    App.handleTrackClick(index, mouse.modifiers)
                                    if (App.librarySearchOpen && mouse.modifiers === Qt.NoModifier)
                                        App.acceptLibrarySearch()
                                }
                            }

                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: {
                                    App.prepareTrackContextMenu(index)
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
        implicitHeight: searchRow.implicitHeight + Theme.spaceSm * 2
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
                anchors.margins: Theme.spaceSm
                spacing: Theme.spaceSm

                Rectangle {
                    Layout.preferredWidth: scopeBadge.implicitWidth + Theme.spaceMd
                    Layout.preferredHeight: scopeBadge.implicitHeight + Theme.spaceSm
                    radius: Theme.radiusSm
                    color: scopeMouse.containsMouse || scopeMouse.pressed
                           ? Theme.rgba(Theme.accent, 0.22)
                           : Theme.rgba(Theme.accent, 0.12)

                    Behavior on color { ColorAnimation { duration: 120 } }

                    Label {
                        id: scopeBadge
                        anchors.centerIn: parent
                        text: scopeHint() + " ▾"
                        font.pixelSize: Theme.fontCaption
                        font.bold: true
                        color: Theme.accent
                    }

                    ToolTip.visible: App.config.tooltipsEnabled && scopeMouse.containsMouse
                    ToolTip.text: "Click or press Tab to switch scope (" + scopeHint() + " → "
                                  + nextScopeHint() + ")"

                    MouseArea {
                        id: scopeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: App.cycleLibrarySearchScope()
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignVCenter
                    text: searchCountText()
                    font.pixelSize: Theme.fontCaption
                    color: Theme.accent
                    opacity: 0.85
                    visible: text.length > 0
                }

                TextField {
                    id: librarySearchField
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    placeholderText: searchPlaceholder()
                    selectByMouse: true
                    Accessible.name: "Library search"
                    Accessible.role: Accessible.EditableText
                    // Steal arrows/Enter before TextField uses them for the caret.
                    Keys.priority: Keys.BeforeItem
                    onTextChanged: {
                        if (App.librarySearchQuery !== text)
                            App.setLibrarySearchQuery(text)
                    }
                    Keys.onEscapePressed: function(event) {
                        event.accepted = true
                        App.closeLibrarySearch()
                    }
                    Keys.onTabPressed: function(event) {
                        event.accepted = true
                        App.cycleLibrarySearchScope()
                    }
                    Keys.onUpPressed: function(event) {
                        event.accepted = true
                        App.librarySearchMoveUp()
                    }
                    Keys.onDownPressed: function(event) {
                        event.accepted = true
                        App.librarySearchMoveDown()
                    }
                    Keys.onReturnPressed: function(event) {
                        event.accepted = true
                        App.acceptLibrarySearch()
                    }
                    Keys.onEnterPressed: function(event) {
                        event.accepted = true
                        App.acceptLibrarySearch()
                    }
                }

                ToolButton {
                    Layout.alignment: Qt.AlignVCenter
                    text: "Esc"
                    display: AbstractButton.TextOnly
                    Accessible.name: "Close search"
                    Accessible.role: Accessible.Button
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
        case "albums": return "Search albums…  (↑↓ select, Enter · Tab: tracks)"
        case "tracks": return "Search tracks…  (↑↓ select, Enter · Tab: artists)"
        default: return "Search artists…  (↑↓ select, Enter · Tab: albums)"
        }
    }

    function scopeHint() {
        switch (App.librarySearchScope) {
        case "albums": return "Albums"
        case "tracks": return "Tracks"
        default: return "Artists"
        }
    }

    function nextScopeHint() {
        switch (App.librarySearchScope) {
        case "artists": return "Albums"
        case "albums": return "Tracks"
        default: return "Artists"
        }
    }

    function searchCountText() {
        if (!App.librarySearchOpen || App.librarySearchQuery.trimmed().length === 0)
            return ""
        var n = 0
        if (App.librarySearchScope === "albums")
            n = albumList.count
        else if (App.librarySearchScope === "tracks")
            n = trackList.count
        else
            n = artistList.count
        return n === 1 ? "1 match" : n + " matches"
    }

    function formatDuration(ms) {
        if (!ms || ms <= 0) return "--:--"
        var totalSec = Math.floor(ms / 1000)
        var min = Math.floor(totalSec / 60)
        var sec = totalSec % 60
        return min + ":" + (sec < 10 ? "0" : "") + sec
    }
}
