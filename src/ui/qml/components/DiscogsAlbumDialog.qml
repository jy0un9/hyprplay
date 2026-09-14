import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    title: "Discogs album lookup"
    modal: true
    Overlay.modal: ThemedModalScrim {}
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(860, parent ? parent.width * 0.94 : 860)
    height: Math.min(700, parent ? parent.height * 0.92 : 700)
    padding: Theme.spaceSm
    topPadding: Theme.spaceSm
    bottomPadding: Theme.spaceSm

    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.surface
        border.color: Theme.rgba(Theme.border, 0.35)
        border.width: 1
    }

    onClosed: App.closeDiscogsForSelectedAlbum()

    property string selectedEdition: ""

    function syncSearchFields() {
        artistField.text = App.selectedArtist
        albumField.text = App.selectedAlbum
        formatCd.checked = true
        formatVinyl.checked = false
        formatCassette.checked = false
        formatDigital.checked = false
        selectedEdition = ""
    }

    function selectedFormats() {
        var list = []
        if (formatCd.checked) list.push("CD")
        if (formatVinyl.checked) list.push("Vinyl")
        if (formatCassette.checked) list.push("Cassette")
        if (formatDigital.checked) list.push("Digital")
        return list
    }

    function runSearch() {
        App.searchAlbumDiscogs(artistField.text, albumField.text,
                               dialog.selectedFormats(), dialog.selectedEdition)
    }

    function toggleEdition(label) {
        selectedEdition = (selectedEdition === label) ? "" : label
    }

    function metaLine(row) {
        var parts = []
        if (row.year) parts.push(row.year)
        if (row.format) parts.push(row.format)
        if (row.mediaCount) parts.push(row.mediaCount + "× media")
        if (row.label) parts.push(row.label)
        if (row.country) parts.push(row.country)
        return parts.join(" · ")
    }

    Connections {
        target: App
        function onAlbumDiscogsChanged() {
            if (App.albumDiscogsOpen) {
                if (!dialog.visible)
                    dialog.syncSearchFields()
                dialog.open()
            } else {
                dialog.close()
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.spaceSm
        clip: true

        Rectangle {
            Layout.fillWidth: true
            radius: Theme.radiusSm
            color: Theme.rgba(Theme.accent, 0.08)
            border.color: Theme.rgba(Theme.accent, 0.25)
            border.width: 1
            implicitHeight: headerCol.implicitHeight + 16

            Column {
                id: headerCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 8
                spacing: 2

                Label {
                    width: parent.width
                    text: App.selectedArtist + " — " + App.selectedAlbum
                    font.bold: true
                    elide: Text.ElideRight
                }
                Label {
                    width: parent.width
                    text: App.selectedAlbumTrackCount > 0
                          ? ("Your library copy: " + App.selectedAlbumTrackCount + " tracks")
                          : "Search Discogs for artwork and album notes"
                    opacity: 0.7
                    font.pixelSize: Theme.fontCaption
                    elide: Text.ElideRight
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Theme.spaceSm
            rowSpacing: 6

            Label { text: "Artist"; opacity: 0.7; font.pixelSize: Theme.fontCaption }
            TextField {
                id: artistField
                Layout.fillWidth: true
                placeholderText: "Artist or leave blank"
                onAccepted: dialog.runSearch()
            }

            Label { text: "Album"; opacity: 0.7; font.pixelSize: Theme.fontCaption }
            TextField {
                id: albumField
                Layout.fillWidth: true
                placeholderText: "Album / release title"
                onAccepted: dialog.runSearch()
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 8
            CheckBox { id: formatCd; text: "CD"; checked: true }
            CheckBox { id: formatVinyl; text: "Vinyl" }
            CheckBox { id: formatCassette; text: "Tape" }
            CheckBox { id: formatDigital; text: "Digital" }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: ["Deluxe", "Expanded", "Anniversary", "Remaster", "Special Edition", "Explicit", "Bonus"]
                delegate: Button {
                    text: modelData
                    checkable: true
                    checked: dialog.selectedEdition === modelData
                    flat: true
                    font.pixelSize: Theme.fontCaption
                    implicitHeight: 28
                    onClicked: dialog.toggleEdition(modelData)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Button {
                text: "Search"
                enabled: !App.discogs.busy
                         && (artistField.text.trim().length > 0
                             || albumField.text.trim().length > 0
                             || dialog.selectedEdition.length > 0)
                onClicked: dialog.runSearch()
            }
            Label {
                Layout.fillWidth: true
                text: App.discogs.status
                opacity: 0.7
                elide: Text.ElideRight
                font.pixelSize: Theme.fontCaption
                visible: text.length > 0
            }
            BusyIndicator {
                running: App.discogs.busy
                visible: running
                implicitWidth: 22
                implicitHeight: 22
            }
        }

        ListView {
            id: candidates
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: App.albumDiscogsCandidates
            currentIndex: App.albumDiscogsSelectedIndex
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: ItemDelegate {
                id: del
                width: candidates.width
                height: 54
                highlighted: App.albumDiscogsSelectedIndex === index
                onClicked: App.setAlbumDiscogsSelectedIndex(index)

                background: Rectangle {
                    color: del.highlighted ? Theme.rgba(Theme.accent, 0.18)
                                           : (del.hovered ? Theme.rgba(Theme.foreground, 0.05)
                                                          : "transparent")
                    radius: Theme.radiusSm
                }

                contentItem: Item {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8

                    Column {
                        anchors.left: parent.left
                        anchors.right: trackBadge.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2

                        Label {
                            width: parent.width
                            text: modelData.title || ""
                            font.bold: del.highlighted
                            elide: Text.ElideRight
                        }
                        Label {
                            width: parent.width
                            text: dialog.metaLine(modelData)
                            opacity: 0.6
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideRight
                        }
                    }

                    Rectangle {
                        id: trackBadge
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        visible: modelData.mediaCount > 0
                        radius: 8
                        color: Theme.rgba(Theme.accent, 0.15)
                        width: badgeLabel.implicitWidth + 12
                        height: 22
                        Label {
                            id: badgeLabel
                            anchors.centerIn: parent
                            text: modelData.mediaCount + "×"
                            font.pixelSize: Theme.fontCaption
                            color: Theme.accent
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: !App.discogs.busy && candidates.count === 0
                opacity: 0.55
                text: "No releases matched — try another format or edition"
                width: parent.width - 24
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }

        Label {
            Layout.fillWidth: true
            visible: candidates.count > 0
            text: "Showing " + candidates.count + " release(s)"
                  + (App.selectedAlbumTrackCount > 0
                     ? (" · pick the closest match to your "
                        + App.selectedAlbumTrackCount + " tracks")
                     : "")
            opacity: 0.55
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideRight
        }
    }

    footer: DialogButtonBox {
        Button {
            text: "Cancel"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: dialog.reject()
        }
        Button {
            text: "Fetch selected"
            enabled: App.albumDiscogsSelectedIndex >= 0 && !App.discogs.busy
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            onClicked: App.fetchSelectedDiscogsAlbum()
        }
    }
}
