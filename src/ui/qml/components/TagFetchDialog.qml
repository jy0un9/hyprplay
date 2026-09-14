import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: tagFetchDialog
    title: "Lookup metadata"
    modal: true
    Overlay.modal: ThemedModalScrim {}
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(1100, parent ? parent.width * 0.98 : 1100)
    height: Math.min(860, parent ? parent.height * 0.96 : 860)
    padding: Theme.spaceSm
    topPadding: Theme.spaceSm
    bottomPadding: Theme.spaceSm

    Material.theme: Theme.dark ? Material.Dark : Material.Light

    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.surface
        border.color: Theme.rgba(Theme.border, 0.35)
        border.width: 1
    }

    onClosed: App.closeTagFetch()

    property string selectedEdition: ""
    property bool filtersExpanded: true

    function syncSearchFields() {
        const fields = App.metadataSearch.currentFields || {}
        artistField.text = fields.artist || App.selectedArtist || ""
        albumField.text = fields.album || App.selectedAlbum || ""
        formatCd.checked = true
        formatVinyl.checked = false
        formatCassette.checked = false
        formatDigital.checked = true
        selectedEdition = ""
        filtersExpanded = true
    }

    function selectedFormats() {
        var list = []
        if (formatCd.checked) list.push("CD")
        if (formatVinyl.checked) list.push("Vinyl")
        if (formatCassette.checked) list.push("Cassette")
        if (formatDigital.checked) list.push("Digital")
        return list
    }

    function toggleEdition(label) {
        selectedEdition = (selectedEdition === label) ? "" : label
    }

    function runSearch() {
        App.metadataSearch.searchRelease(artistField.text, albumField.text, "",
                                         tagFetchDialog.selectedFormats(),
                                         tagFetchDialog.selectedEdition)
    }

    function trackBadgeText(row) {
        if (row.trackCount > 0)
            return row.trackCount + " tr"
        if (row.mediaCount > 0)
            return row.mediaCount + "×"
        return ""
    }

    function trackDeltaText(row) {
        var local = App.metadataSearch.localTrackCount
        if (!(row.trackCount > 0) || local <= 0)
            return ""
        var delta = row.trackCount - local
        if (delta === 0)
            return "exact"
        return (delta > 0 ? "+" : "") + delta
    }

    Connections {
        target: App
        function onTagFetchChanged() {
            if (App.tagFetchOpen) {
                if (!tagFetchDialog.visible)
                    tagFetchDialog.syncSearchFields()
                tagFetchDialog.open()
            } else {
                tagFetchDialog.close()
            }
        }
    }

    Connections {
        target: App.metadataSearch
        function onFieldChoicesChanged() {
            if (App.metadataSearch.fieldChoices.length > 0)
                tagFetchDialog.filtersExpanded = false
        }
    }

    footer: DialogButtonBox {
        Button {
            text: "Cancel"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: tagFetchDialog.reject()
        }
        PrimaryButton {
            text: "Apply selected tags"
            enabled: App.metadataSearch.fieldChoices.length > 0 && !App.metadataSearch.searching
            onClicked: App.applyTagFetch()
        }
    }

    contentItem: ColumnLayout {
        spacing: 6
        clip: true

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Label {
                    Layout.fillWidth: true
                    text: (artistField.text || App.selectedArtist || "Artist")
                          + " — " + (albumField.text || App.selectedAlbum || "Album")
                    font.bold: true
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    text: App.metadataSearch.localTrackCount > 0
                          ? (App.metadataSearch.localTrackCount + " tracks in library · closest track counts rank first")
                          : "Search MusicBrainz + Discogs, then review tags below"
                    opacity: 0.65
                    font.pixelSize: Theme.fontCaption
                    elide: Text.ElideRight
                }
            }

            Button {
                text: tagFetchDialog.filtersExpanded ? "Hide filters" : "Show filters"
                flat: true
                onClicked: tagFetchDialog.filtersExpanded = !tagFetchDialog.filtersExpanded
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: tagFetchDialog.filtersExpanded

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TextField {
                    id: artistField
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    placeholderText: "Artist"
                    onAccepted: tagFetchDialog.runSearch()
                }
                TextField {
                    id: albumField
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1.4
                    placeholderText: "Album / release title"
                    onAccepted: tagFetchDialog.runSearch()
                }
                Button {
                    text: "Search"
                    enabled: !App.metadataSearch.searching
                             && (artistField.text.trim().length > 0
                                 || albumField.text.trim().length > 0
                                 || tagFetchDialog.selectedEdition.length > 0)
                    onClicked: tagFetchDialog.runSearch()
                }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8
                CheckBox { id: formatCd; text: "CD"; checked: true }
                CheckBox { id: formatVinyl; text: "Vinyl" }
                CheckBox { id: formatCassette; text: "Tape" }
                CheckBox { id: formatDigital; text: "Digital"; checked: true }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: ["Deluxe", "Expanded", "Anniversary", "Remaster", "Special Edition", "Explicit", "Bonus"]
                    delegate: Button {
                        text: modelData
                        checkable: true
                        checked: tagFetchDialog.selectedEdition === modelData
                        flat: true
                        font.pixelSize: Theme.fontCaption
                        implicitHeight: 28
                        onClicked: tagFetchDialog.toggleEdition(modelData)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                Layout.fillWidth: true
                text: App.metadataSearch.status
                opacity: 0.7
                elide: Text.ElideRight
                font.pixelSize: Theme.fontCaption
            }
            Button {
                text: "Search again"
                flat: true
                visible: !tagFetchDialog.filtersExpanded
                enabled: !App.metadataSearch.searching
                onClicked: {
                    tagFetchDialog.filtersExpanded = true
                    tagFetchDialog.runSearch()
                }
            }
            BusyIndicator {
                running: App.metadataSearch.searching
                visible: running
                implicitWidth: 20
                implicitHeight: 20
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical
            handle: Rectangle {
                implicitHeight: 8
                color: Theme.rgba(Theme.border, 0.35)
            }

            ColumnLayout {
                SplitView.preferredHeight: 200
                SplitView.minimumHeight: 120
                SplitView.maximumHeight: parent.height * 0.45
                spacing: 4

                Label {
                    text: "Matching releases"
                    font.bold: true
                    Layout.fillWidth: true
                }

                ListView {
                    id: candidateList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: App.metadataSearch.candidates
                    currentIndex: App.metadataSearch.selectedIndex
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: ItemDelegate {
                        id: candDel
                        width: candidateList.width
                        height: 48
                        highlighted: App.metadataSearch.selectedIndex === index
                        onClicked: App.metadataSearch.setSelectedIndex(index)

                        background: Rectangle {
                            color: candDel.highlighted ? Theme.rgba(Theme.accent, 0.18)
                                                       : (candDel.hovered ? Theme.rgba(Theme.foreground, 0.05)
                                                                          : "transparent")
                            radius: Theme.radiusSm
                        }

                        contentItem: Item {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8

                            Column {
                                anchors.left: parent.left
                                anchors.right: badgeCol.left
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 1

                                Label {
                                    width: parent.width
                                    text: (modelData.source || "") + " · " + (modelData.title || "")
                                    font.bold: candDel.highlighted
                                    elide: Text.ElideRight
                                }
                                Label {
                                    width: parent.width
                                    text: modelData.detail || ""
                                    opacity: 0.6
                                    font.pixelSize: Theme.fontCaption
                                    elide: Text.ElideRight
                                }
                            }

                            Row {
                                id: badgeCol
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 6

                                Label {
                                    visible: tagFetchDialog.trackDeltaText(modelData).length > 0
                                    text: tagFetchDialog.trackDeltaText(modelData)
                                    font.pixelSize: Theme.fontCaption
                                    opacity: 0.75
                                    color: tagFetchDialog.trackDeltaText(modelData) === "exact"
                                           ? Theme.accent : Theme.foreground
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Rectangle {
                                    visible: tagFetchDialog.trackBadgeText(modelData).length > 0
                                    radius: 8
                                    color: Theme.rgba(Theme.accent, 0.15)
                                    width: Math.max(40, trackLbl.implicitWidth + 12)
                                    height: 22
                                    Label {
                                        id: trackLbl
                                        anchors.centerIn: parent
                                        text: tagFetchDialog.trackBadgeText(modelData)
                                        font.pixelSize: Theme.fontCaption
                                        color: Theme.accent
                                    }
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                SplitView.fillHeight: true
                SplitView.minimumHeight: 260
                spacing: 6

                Label {
                    text: App.metadataSearch.fieldChoices.length > 0
                          ? ("Tags to apply (" + App.metadataSearch.fieldChoices.length + ")")
                          : "Tags to apply"
                    font.bold: true
                    font.pixelSize: Theme.fontSmall + 1
                    Layout.fillWidth: true
                }

                ListView {
                    id: fieldList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: App.metadataSearch.fieldChoices
                    visible: count > 0
                    boundsBehavior: Flickable.StopAtBounds
                    spacing: 4
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AlwaysOn
                        width: 10
                    }

                    delegate: Rectangle {
                        id: fieldRow
                        width: fieldList.width - 12
                        height: Math.max(64, fieldInner.implicitHeight + 16)
                        radius: Theme.radiusSm
                        color: fieldChoice.checked ? Theme.rgba(Theme.accent, 0.10)
                                                   : Theme.rgba(Theme.foreground, 0.04)
                        border.color: Theme.rgba(Theme.border, 0.25)
                        border.width: 1

                        CheckBox {
                            id: fieldChoice
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: 6
                            width: 36
                            property string fieldKey: modelData.key || ""
                            checked: modelData.checked === true
                            onClicked: App.metadataSearch.setFieldChecked(fieldKey, checked)
                        }

                        ColumnLayout {
                            id: fieldInner
                            anchors.left: fieldChoice.right
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 4
                            anchors.rightMargin: 10
                            spacing: 4

                            Label {
                                Layout.fillWidth: true
                                text: modelData.label || fieldChoice.fieldKey
                                font.bold: true
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                text: "\"" + (modelData.current || "") + "\""
                                opacity: 0.6
                                wrapMode: Text.Wrap
                                font.pixelSize: Theme.fontSmall
                            }
                            Label {
                                Layout.fillWidth: true
                                text: "→  \"" + (modelData.proposed || "") + "\""
                                wrapMode: Text.Wrap
                                font.pixelSize: Theme.fontSmall + 1
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            anchors.leftMargin: 42
                            acceptedButtons: Qt.LeftButton
                            onClicked: App.metadataSearch.setFieldChecked(
                                           fieldChoice.fieldKey, !fieldChoice.checked)
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: "Select a release above to compare tags in this large pane."
                    opacity: 0.5
                    visible: App.metadataSearch.fieldChoices.length === 0 && !App.metadataSearch.searching
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
