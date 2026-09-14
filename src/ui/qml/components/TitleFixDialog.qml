import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: titleFixDialog
    title: "Title Fix"
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

    onClosed: App.closeTitleFix()

    property string selectedEdition: ""
    property bool filtersExpanded: true

    function syncSearchFields() {
        artistField.text = App.selectedArtist
        albumField.text = App.selectedAlbum
        formatCd.checked = true
        formatVinyl.checked = false
        formatCassette.checked = false
        formatDigital.checked = true
        selectedEdition = ""
        // Start with filters open; collapse once track changes appear so the list can breathe.
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

    function runSearch() {
        App.metadataSearch.searchRelease(artistField.text, albumField.text, "",
                                         titleFixDialog.selectedFormats(),
                                         titleFixDialog.selectedEdition)
    }

    function toggleEdition(label) {
        selectedEdition = (selectedEdition === label) ? "" : label
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
        function onTitleFixChanged() {
            if (App.titleFixOpen) {
                if (!titleFixDialog.visible)
                    titleFixDialog.syncSearchFields()
                titleFixDialog.open()
            } else {
                titleFixDialog.close()
            }
        }
    }

    Connections {
        target: App.metadataSearch
        function onTitleFixProposalsChanged() {
            if (App.metadataSearch.titleFixProposals.length > 0)
                titleFixDialog.filtersExpanded = false
        }
    }

    footer: DialogButtonBox {
        Button {
            text: "Cancel"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: titleFixDialog.reject()
        }
        PrimaryButton {
            text: "Apply title fixes"
            enabled: App.metadataSearch.titleFixProposals.length > 0 && !App.metadataSearch.searching
            onClicked: App.applyTitleFix()
        }
    }

    contentItem: ColumnLayout {
        spacing: 6
        clip: true

        // Compact album + search row
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Label {
                    Layout.fillWidth: true
                    text: App.selectedArtist + " — " + App.selectedAlbum
                    font.bold: true
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    text: App.metadataSearch.localTrackCount > 0
                          ? (App.metadataSearch.localTrackCount + " tracks in library · closest track counts rank first")
                          : "Pick a release, then review title changes below"
                    opacity: 0.65
                    font.pixelSize: Theme.fontCaption
                    elide: Text.ElideRight
                }
            }

            Button {
                text: titleFixDialog.filtersExpanded ? "Hide filters" : "Show filters"
                flat: true
                onClicked: titleFixDialog.filtersExpanded = !titleFixDialog.filtersExpanded
            }
        }

        // Collapsible filters — frees vertical space for track changes
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: titleFixDialog.filtersExpanded

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TextField {
                    id: artistField
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    placeholderText: "Artist"
                    onAccepted: titleFixDialog.runSearch()
                }
                TextField {
                    id: albumField
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1.4
                    placeholderText: "Album / release title"
                    onAccepted: titleFixDialog.runSearch()
                }
                Button {
                    text: "Search"
                    enabled: !App.metadataSearch.searching
                             && (artistField.text.trim().length > 0
                                 || albumField.text.trim().length > 0
                                 || titleFixDialog.selectedEdition.length > 0)
                    onClicked: titleFixDialog.runSearch()
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
                        checked: titleFixDialog.selectedEdition === modelData
                        flat: true
                        font.pixelSize: Theme.fontCaption
                        implicitHeight: 28
                        onClicked: titleFixDialog.toggleEdition(modelData)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: !titleFixDialog.filtersExpanded
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
                enabled: !App.metadataSearch.searching
                onClicked: {
                    titleFixDialog.filtersExpanded = true
                    titleFixDialog.runSearch()
                }
            }
            BusyIndicator {
                running: App.metadataSearch.searching
                visible: running
                implicitWidth: 20
                implicitHeight: 20
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: titleFixDialog.filtersExpanded
            Label {
                Layout.fillWidth: true
                text: App.metadataSearch.status
                opacity: 0.7
                elide: Text.ElideRight
                font.pixelSize: Theme.fontCaption
            }
            BusyIndicator {
                running: App.metadataSearch.searching
                visible: running
                implicitWidth: 20
                implicitHeight: 20
            }
        }

        // Vertical split: compact release picker on top, big track-changes pane below
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
                                    visible: titleFixDialog.trackDeltaText(modelData).length > 0
                                    text: titleFixDialog.trackDeltaText(modelData)
                                    font.pixelSize: Theme.fontCaption
                                    opacity: 0.75
                                    color: titleFixDialog.trackDeltaText(modelData) === "exact"
                                           ? Theme.accent : Theme.foreground
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Rectangle {
                                    visible: titleFixDialog.trackBadgeText(modelData).length > 0
                                    radius: 8
                                    color: Theme.rgba(Theme.accent, 0.15)
                                    width: Math.max(40, trackLbl.implicitWidth + 12)
                                    height: 22
                                    Label {
                                        id: trackLbl
                                        anchors.centerIn: parent
                                        text: titleFixDialog.trackBadgeText(modelData)
                                        font.pixelSize: Theme.fontCaption
                                        color: Theme.accent
                                    }
                                }
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: !App.metadataSearch.searching && candidateList.count === 0
                        opacity: 0.55
                        width: parent.width - 24
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: "No matching releases — show filters and try Digital + Deluxe/Expanded"
                    }
                }
            }

            ColumnLayout {
                SplitView.fillHeight: true
                SplitView.minimumHeight: 260
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: App.metadataSearch.titleFixProposals.length > 0
                              ? ("Title changes (" + App.metadataSearch.titleFixProposals.length + ")")
                              : "Title changes"
                        font.bold: true
                        font.pixelSize: Theme.fontSmall + 1
                        Layout.fillWidth: true
                    }
                    Label {
                        visible: App.metadataSearch.titleFixUnmatched.length > 0
                        text: App.metadataSearch.titleFixUnmatched.length + " unmatched"
                        opacity: 0.6
                        font.pixelSize: Theme.fontCaption
                    }
                }

                ListView {
                    id: proposalList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: App.metadataSearch.titleFixProposals
                    visible: count > 0
                    boundsBehavior: Flickable.StopAtBounds
                    spacing: 4
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AlwaysOn
                        width: 10
                    }

                    delegate: Rectangle {
                        id: proposalRow
                        width: proposalList.width - 12
                        height: Math.max(64, proposalInner.implicitHeight + 16)
                        radius: Theme.radiusSm
                        color: proposalBox.checked ? Theme.rgba(Theme.accent, 0.10)
                                                   : Theme.rgba(Theme.foreground, 0.04)
                        border.color: Theme.rgba(Theme.border, 0.25)
                        border.width: 1

                        CheckBox {
                            id: proposalBox
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: 6
                            width: 36
                            checked: modelData.checked === true
                            onClicked: App.metadataSearch.setTitleFixChecked(index, checked)
                        }

                        ColumnLayout {
                            id: proposalInner
                            anchors.left: proposalBox.right
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 4
                            anchors.rightMargin: 10
                            spacing: 4

                            Label {
                                Layout.fillWidth: true
                                text: (modelData.trackNumber > 0 ? ("#" + modelData.trackNumber + "  ") : "")
                                      + (modelData.current || "")
                                opacity: 0.65
                                wrapMode: Text.Wrap
                                font.pixelSize: Theme.fontSmall
                            }
                            Label {
                                Layout.fillWidth: true
                                text: "→  " + (modelData.proposed || "")
                                wrapMode: Text.Wrap
                                font.bold: true
                                font.pixelSize: Theme.fontSmall + 1
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            anchors.leftMargin: 42
                            acceptedButtons: Qt.LeftButton
                            onClicked: App.metadataSearch.setTitleFixChecked(index, !proposalBox.checked)
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: App.metadataSearch.titleFixProposals.length === 0
                    opacity: 0.55
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: App.metadataSearch.searching
                          ? "Searching…"
                          : (App.metadataSearch.candidates.length === 0
                             ? "Search for a release to begin"
                             : "Select a release above with a matching track count.\nTitle changes will fill this large pane.")
                }

                ListView {
                    id: unmatchedList
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(72, contentHeight)
                    clip: true
                    visible: App.metadataSearch.titleFixUnmatched.length > 0
                    model: App.metadataSearch.titleFixUnmatched
                    spacing: 2
                    delegate: Label {
                        width: unmatchedList.width
                        height: 22
                        opacity: 0.65
                        text: "Unmatched: "
                              + (modelData.trackNumber > 0 ? ("#" + modelData.trackNumber + "  ") : "")
                              + (modelData.title || "")
                        elide: Text.ElideRight
                        font.pixelSize: Theme.fontCaption
                    }
                }
            }
        }
    }
}
