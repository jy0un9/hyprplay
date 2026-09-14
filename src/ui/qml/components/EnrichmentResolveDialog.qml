import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: dialog
    title: dialog.headerTitle()
    modal: true
    Overlay.modal: ThemedModalScrim {}
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(1100, parent ? parent.width * 0.96 : 1100)
    height: Math.min(860, parent ? parent.height * 0.96 : 860)
    padding: Theme.spaceSm
    topPadding: Theme.spaceSm
    bottomPadding: Theme.spaceSm
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Material.theme: Theme.dark ? Material.Dark : Material.Light

    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.surface
        border.color: Theme.rgba(Theme.border, 0.35)
        border.width: 1
    }

    // Escape / scrim close skips the current item and continues the run.
    onClosed: {
        if (App.enrichment.paused)
            App.enrichment.skipCurrent()
    }

    function headerTitle() {
        var kind = App.enrichment.resolveKind
        if (kind === "artist")
            return "Choose Discogs artist"
        if (kind === "album")
            return "Choose Discogs album"
        if (kind === "titleFix")
            return "Review title fix"
        return "Library enrichment"
    }

    function metaLine(row) {
        var parts = []
        if (row.detail)
            parts.push(row.detail)
        if (row.year)
            parts.push(row.year)
        if (row.format)
            parts.push(row.format)
        if (row.label)
            parts.push(row.label)
        if (row.country)
            parts.push(row.country)
        if (row.source)
            parts.push(row.source)
        return parts.join(" · ")
    }

    function badgeText(row) {
        if (row.badge && row.badge.length > 0)
            return row.badge
        if (row.trackCount > 0)
            return row.trackCount + " tr"
        if (row.mediaCount > 0)
            return row.mediaCount + "×"
        return ""
    }

    function trackDeltaText(row) {
        var local = App.enrichment.localTrackCount
        if (!(row.trackCount > 0) || local <= 0)
            return ""
        var delta = row.trackCount - local
        if (delta === 0)
            return "exact"
        return (delta > 0 ? "+" : "") + delta
    }

    function primaryActionLabel() {
        if (App.enrichment.resolveKind === "titleFix")
            return "Apply title changes"
        return "Use selected"
    }

    Connections {
        target: App.enrichment
        function onStateChanged() {
            if (App.enrichment.paused) {
                if (!dialog.visible)
                    dialog.open()
            } else if (dialog.visible) {
                dialog.close()
            }
        }
    }

    footer: DialogButtonBox {
        alignment: Qt.AlignRight
        Label {
            text: "Skip leaves this item unchanged and continues."
            opacity: 0.55
            font.pixelSize: Theme.fontCaption
            anchors.verticalCenter: parent.verticalCenter
            leftPadding: Theme.spaceSm
        }
        Button {
            text: "Cancel run"
            DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole
            onClicked: {
                App.enrichment.cancel()
                dialog.close()
            }
        }
        Button {
            text: "Skip"
            onClicked: App.enrichment.skipCurrent()
        }
        PrimaryButton {
            text: dialog.primaryActionLabel()
            enabled: App.enrichment.resolveKind === "titleFix"
                     || App.enrichment.selectedIndex >= 0
            onClicked: App.enrichment.chooseSelected()
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
                    text: App.enrichment.currentAlbum.length > 0
                          ? (App.enrichment.currentArtist + " — " + App.enrichment.currentAlbum)
                          : App.enrichment.currentArtist
                    font.bold: true
                    elide: Text.ElideRight
                }
                Label {
                    width: parent.width
                    visible: App.enrichment.localTrackCount > 0
                             && (App.enrichment.resolveKind === "album"
                                 || App.enrichment.resolveKind === "titleFix")
                    text: "Your library copy: " + App.enrichment.localTrackCount + " tracks"
                    opacity: 0.7
                    font.pixelSize: Theme.fontCaption
                    elide: Text.ElideRight
                }
                Label {
                    width: parent.width
                    text: App.enrichment.status
                    opacity: 0.65
                    font.pixelSize: Theme.fontCaption
                    elide: Text.ElideRight
                    visible: text.length > 0
                }
            }
        }

        // Artist / album: ranked candidate list only
        ListView {
            id: candidateList
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: App.enrichment.resolveKind !== "titleFix"
            clip: true
            model: App.enrichment.candidates
            currentIndex: App.enrichment.selectedIndex
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: ItemDelegate {
                id: del
                width: candidateList.width
                height: 54
                highlighted: App.enrichment.selectedIndex === index
                onClicked: App.enrichment.selectedIndex = index

                background: Rectangle {
                    color: del.highlighted ? Theme.rgba(Theme.accent, 0.18)
                                           : (del.hovered ? Theme.rgba(Theme.foreground, 0.05)
                                                          : "transparent")
                    radius: Theme.radiusSm
                    border.width: del.highlighted ? 1 : 0
                    border.color: Theme.rgba(Theme.accent, 0.35)
                }

                contentItem: Item {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8

                    Column {
                        anchors.left: parent.left
                        anchors.right: badgeBox.left
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
                        id: badgeBox
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        visible: dialog.badgeText(modelData).length > 0
                        radius: 8
                        color: Theme.rgba(Theme.accent, 0.15)
                        width: badgeLabel.implicitWidth + 12
                        height: 22
                        Label {
                            id: badgeLabel
                            anchors.centerIn: parent
                            text: dialog.badgeText(modelData)
                            font.pixelSize: Theme.fontCaption
                            color: Theme.accent
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: candidateList.count === 0
                opacity: 0.55
                text: "No candidates"
                width: parent.width - 24
                horizontalAlignment: Text.AlignHCenter
            }
        }

        // Title Fix: releases above, proposals + unmatched below
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: App.enrichment.resolveKind === "titleFix"
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
                    id: titleCandidateList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: App.enrichment.candidates
                    currentIndex: App.enrichment.selectedIndex
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: ItemDelegate {
                        id: candDel
                        width: titleCandidateList.width
                        height: 54
                        highlighted: App.enrichment.selectedIndex === index
                        onClicked: App.enrichment.selectedIndex = index

                        background: Rectangle {
                            color: candDel.highlighted ? Theme.rgba(Theme.accent, 0.18)
                                                       : (candDel.hovered ? Theme.rgba(Theme.foreground, 0.05)
                                                                          : "transparent")
                            radius: Theme.radiusSm
                            border.width: candDel.highlighted ? 1 : 0
                            border.color: Theme.rgba(Theme.accent, 0.35)
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
                                spacing: 2
                                Label {
                                    width: parent.width
                                    text: modelData.title || ""
                                    font.bold: candDel.highlighted
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

                            Column {
                                id: badgeCol
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2
                                Label {
                                    anchors.right: parent.right
                                    text: dialog.trackDeltaText(modelData)
                                    visible: text.length > 0
                                    font.pixelSize: Theme.fontCaption
                                    color: text === "exact" ? Theme.accent : Theme.foreground
                                }
                                Rectangle {
                                    anchors.right: parent.right
                                    visible: dialog.badgeText(modelData).length > 0
                                    radius: 8
                                    color: Theme.rgba(Theme.accent, 0.15)
                                    width: titleBadgeLabel.implicitWidth + 12
                                    height: 22
                                    Label {
                                        id: titleBadgeLabel
                                        anchors.centerIn: parent
                                        text: dialog.badgeText(modelData)
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
                    text: "Title changes"
                    font.bold: true
                    Layout.fillWidth: true
                }

                ListView {
                    id: proposalList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: App.enrichment.titleFixProposals
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    spacing: 6

                    delegate: Rectangle {
                        id: proposalBox
                        width: proposalList.width
                        radius: Theme.radiusSm
                        property bool checked: modelData.checked === true
                        color: checked ? Theme.rgba(Theme.accent, 0.10)
                                       : Theme.rgba(Theme.foreground, 0.04)
                        height: Math.max(64, proposalInner.implicitHeight + 16)

                        RowLayout {
                            id: proposalInner
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8

                            CheckBox {
                                checked: proposalBox.checked
                                onClicked: App.enrichment.setTitleFixChecked(index, checked)
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    Layout.fillWidth: true
                                    text: (modelData.trackNumber > 0
                                           ? ("#" + modelData.trackNumber + "  ") : "")
                                          + (modelData.current || "")
                                    opacity: 0.7
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: Theme.fontCaption
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: "→  " + (modelData.proposed || "")
                                    font.bold: true
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            anchors.leftMargin: 36
                            onClicked: App.enrichment.setTitleFixChecked(index, !proposalBox.checked)
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: proposalList.count === 0
                        opacity: 0.55
                        text: App.enrichment.titleFixUnmatched.length > 0
                              ? "No automatic title changes — unmatched tracks below"
                              : "No title changes needed"
                        width: parent.width - 24
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }

                Label {
                    visible: App.enrichment.titleFixUnmatched.length > 0
                    text: "Unmatched local tracks"
                    font.bold: true
                    Layout.fillWidth: true
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(120, count * 28)
                    visible: App.enrichment.titleFixUnmatched.length > 0
                    clip: true
                    model: App.enrichment.titleFixUnmatched
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    delegate: Label {
                        width: ListView.view.width
                        text: "Unmatched: "
                              + (modelData.trackNumber > 0 ? ("#" + modelData.trackNumber + "  ") : "")
                              + (modelData.title || "")
                        font.pixelSize: Theme.fontCaption
                        opacity: 0.75
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
