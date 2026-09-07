import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import components 1.0

Pane {
    id: settingsView
    padding: 0
    background: Rectangle { color: Theme.background }

    property bool dirty: false
    property bool savedFlash: false

    function markClean() {
        settingsView.dirty = false
        settingsView.savedFlash = true
        savedTimer.restart()
    }

    Timer {
        id: savedTimer
        interval: 2500
        onTriggered: settingsView.savedFlash = false
    }

    ScrollView {
        id: settingsScroll
        anchors.fill: parent
        anchors.margins: Theme.spaceXl
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: settingsScroll.availableWidth
            spacing: Theme.spaceLg

            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: (event) => {
                    const flick = settingsScroll.contentItem
                    if (!flick)
                        return
                    const dy = event.pixelDelta.y !== 0
                               ? event.pixelDelta.y * 2
                               : event.angleDelta.y * 2.5
                    if (dy === 0)
                        return
                    const maxY = Math.max(0, flick.contentHeight - flick.height)
                    flick.contentY = Math.max(0, Math.min(maxY, flick.contentY - dy))
                    event.accepted = true
                }
            }

            Item {
                Layout.fillWidth: true
                implicitHeight: headerColumn.implicitHeight

                ColumnLayout {
                    id: headerColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: Theme.spaceXs

                    Label {
                        text: "Settings"
                        color: Theme.foreground
                        font.pixelSize: Theme.fontDisplay
                        font.bold: true
                    }

                    Label {
                        text: "Tune your library, imports, appearance, and integrations."
                        color: Theme.muted
                        font.pixelSize: Theme.fontBody
                        Layout.fillWidth: true
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: libraryCard.implicitHeight + Theme.spaceXl * 2
                radius: Theme.radiusLg
                color: Theme.surface
                border.color: Theme.rgba(Theme.border, 0.35)

                ColumnLayout {
                    id: libraryCard
                    anchors.fill: parent
                    anchors.margins: Theme.spaceXl
                    spacing: Theme.spaceMd

                    Label {
                        text: "Library"
                        color: Theme.foreground
                        font.pixelSize: Theme.fontTitle
                        font.bold: true
                    }
                    Label {
                        text: "Choose where Qt Music finds your files."
                        color: Theme.muted
                        font.pixelSize: Theme.fontSmall
                    }

                    Label { text: "Music folders (comma separated)"; color: Theme.foreground; font.bold: true }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceSm
                        TextField {
                            id: libraryPathsField
                            Layout.fillWidth: true
                            text: App.config.libraryPaths.join(", ")
                            placeholderText: "~/Music"
                            onTextChanged: settingsView.dirty = settingsView.isDirty()
                        }
                        Button {
                            text: "Browse…"
                            onClicked: libraryFolderDialog.open()
                        }
                    }

                    Label { text: "Import inbox"; color: Theme.foreground; font.bold: true }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceSm
                        TextField {
                            id: inboxField
                            Layout.fillWidth: true
                            text: App.config.importInbox
                            placeholderText: "~/Music/inbox"
                            onTextChanged: settingsView.dirty = settingsView.isDirty()
                        }
                        Button {
                            text: "Browse…"
                            onClicked: inboxFolderDialog.open()
                        }
                    }

                    Label { text: "Lyrics directory"; color: Theme.foreground; font.bold: true }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceSm
                        TextField {
                            id: lyricsDirField
                            Layout.fillWidth: true
                            text: App.config.lyricsDir
                            placeholderText: "~/Music/Lyrics"
                            onTextChanged: settingsView.dirty = settingsView.isDirty()
                        }
                        Button {
                            text: "Browse…"
                            onClicked: lyricsFolderDialog.open()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceMd
                        PrimaryButton {
                            text: "Rescan library"
                            onClicked: App.rescanLibrary()
                        }
                        Label {
                            text: App.scanStatus.length > 0 ? App.scanStatus : "Library is up to date"
                            color: Theme.muted
                            opacity: 0.7
                            font.pixelSize: Theme.fontCaption
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                    CheckBox {
                        id: scanOnLaunchField
                        text: "Scan library on launch"
                        checked: App.config.scanOnLaunch
                        onToggled: settingsView.dirty = settingsView.isDirty()
                    }
                    CheckBox {
                        id: libraryWatchField
                        text: "Watch library for changes"
                        checked: App.config.libraryWatchEnabled
                        onToggled: settingsView.dirty = settingsView.isDirty()
                    }
                    Label {
                        text: "When enabled, new or removed tracks under your library paths are picked up automatically."
                        color: Theme.muted
                        font.pixelSize: Theme.fontCaption
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceLg

                Rectangle {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    implicitHeight: toolsCard.implicitHeight + Theme.spaceXl * 2
                    radius: Theme.radiusLg
                    color: Theme.surface
                    border.color: Theme.rgba(Theme.border, 0.35)

                    ColumnLayout {
                        id: toolsCard
                        anchors.fill: parent
                        anchors.margins: Theme.spaceXl
                        spacing: Theme.spaceMd

                        Label { text: "Beets"; color: Theme.foreground; font.pixelSize: Theme.fontTitle; font.bold: true }
                        Label { text: "Optional import automation."; color: Theme.muted; font.pixelSize: Theme.fontSmall }
                        Label { text: "Binary"; color: Theme.foreground; font.bold: true }
                        TextField {
                            id: beetsBinaryField
                            Layout.fillWidth: true
                            text: App.config.beetsBinary
                            placeholderText: "beet"
                            onTextChanged: settingsView.dirty = settingsView.isDirty()
                        }
                        CheckBox {
                            id: beetsNomoveField
                            text: "Use no-move mode"
                            checked: App.config.beetsNomove
                            onToggled: settingsView.dirty = settingsView.isDirty()
                        }
                        Label {
                            text: App.beets.available ? "●  beet OK — " + App.beets.version : "●  beet not found"
                            color: App.beets.available ? Theme.success : Theme.warning
                            font.pixelSize: Theme.fontSmall
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    implicitHeight: appearanceCard.implicitHeight + Theme.spaceXl * 2
                    radius: Theme.radiusLg
                    color: Theme.surface
                    border.color: Theme.rgba(Theme.border, 0.35)

                    ColumnLayout {
                        id: appearanceCard
                        anchors.fill: parent
                        anchors.margins: Theme.spaceXl
                        spacing: Theme.spaceMd

                        Label { text: "Appearance"; color: Theme.foreground; font.pixelSize: Theme.fontTitle; font.bold: true }
                        Label { text: "Personalize the reading experience."; color: Theme.muted; font.pixelSize: Theme.fontSmall }
                        Label { text: "Font family"; color: Theme.foreground; font.bold: true }
                        TextField {
                            id: fontField
                            Layout.fillWidth: true
                            text: App.config.uiFontFamily
                            placeholderText: "JetBrainsMono Nerd Font"
                            onTextChanged: settingsView.dirty = settingsView.isDirty()
                        }
                        Label { text: "Font size"; color: Theme.foreground; font.bold: true }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spaceSm
                            SpinBox {
                                id: fontSizeField
                                from: 9
                                to: 24
                                value: App.config.uiFontSize
                                editable: true
                                onValueChanged: settingsView.dirty = settingsView.isDirty()
                            }
                            Label {
                                text: fontSizeField.value + " px"
                                color: Theme.muted
                                font.pixelSize: Theme.fontSmall
                            }
                            Item { Layout.fillWidth: true }
                        }
                        CheckBox {
                            id: wasdField
                            text: "WASD navigation (mirror arrow keys)"
                            checked: App.config.wasdNavigation
                            onToggled: settingsView.dirty = settingsView.isDirty()
                        }
                        CheckBox {
                            id: tooltipsField
                            text: "Show tooltips"
                            checked: App.config.tooltipsEnabled
                            onToggled: settingsView.dirty = settingsView.isDirty()
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: discogsCard.implicitHeight + Theme.spaceXl * 2
                radius: Theme.radiusLg
                color: Theme.surface
                border.color: Theme.rgba(Theme.border, 0.35)

                ColumnLayout {
                    id: discogsCard
                    anchors.fill: parent
                    anchors.margins: Theme.spaceXl
                    spacing: Theme.spaceMd

                    Label { text: "Discogs"; color: Theme.foreground; font.pixelSize: Theme.fontTitle; font.bold: true }
                    Label {
                        text: "Use a personal token to fetch album notes and artwork."
                        color: Theme.muted
                        font.pixelSize: Theme.fontSmall
                        Layout.fillWidth: true
                    }
                    Label {
                        text: App.config.secretsStorageDescription()
                        color: Theme.muted
                        font.pixelSize: Theme.fontCaption
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    TextField {
                        id: discogsTokenField
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        placeholderText: App.discogs.hasToken ? "Token configured — leave blank to keep" : "Personal access token"
                        onTextChanged: settingsView.dirty = settingsView.isDirty()
                    }
                    Label {
                        text: App.discogs.status.length > 0 ? App.discogs.status
                            : (App.discogs.hasToken ? "●  Token configured" : "●  No token configured")
                        color: App.discogs.hasToken ? Theme.success : Theme.muted
                        font.pixelSize: Theme.fontSmall
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: lyricsProvidersCard.implicitHeight + Theme.spaceXl * 2
                radius: Theme.radiusLg
                color: Theme.surface
                border.color: Theme.rgba(Theme.border, 0.35)

                ColumnLayout {
                    id: lyricsProvidersCard
                    anchors.fill: parent
                    anchors.margins: Theme.spaceXl
                    spacing: Theme.spaceMd

                    Label { text: "Lyrics Providers"; color: Theme.foreground; font.pixelSize: Theme.fontTitle; font.bold: true }
                    Label {
                        text: "LRCLIB is always tried first. Fallbacks below only run when it finds nothing, one request at a time."
                        color: Theme.muted
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    CheckBox {
                        id: neteaseField
                        text: "NetEase fallback (synced)"
                        checked: App.config.lyricsNeteaseEnabled
                        onToggled: settingsView.dirty = settingsView.isDirty()
                    }
                    CheckBox {
                        id: plainField
                        text: "Plain-text fallback (unsynced .txt)"
                        checked: App.config.lyricsPlainEnabled
                        onToggled: settingsView.dirty = settingsView.isDirty()
                    }
                    Label {
                        text: "Genius token (for plain-text fallback, needs a free token from genius.com/api-clients)"
                        color: Theme.muted
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceSm
                        TextField {
                            id: geniusTokenField
                            Layout.fillWidth: true
                            echoMode: TextInput.Password
                            placeholderText: App.lyrics.geniusTokenSet ? "Token configured — enter a new one to replace" : "Genius access token"
                        }
                        PrimaryButton {
                            text: "Save"
                            enabled: geniusTokenField.text.trim().length > 0
                            onClicked: {
                                App.lyrics.setGeniusToken(geniusTokenField.text.trim())
                                geniusTokenField.text = ""
                            }
                        }
                    }
                    Label {
                        text: App.lyrics.geniusTokenSet ? "●  Genius token configured" : "○  No Genius token — Genius skipped"
                        color: App.lyrics.geniusTokenSet ? Theme.success : Theme.muted
                        font.pixelSize: Theme.fontSmall
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: dacCard.implicitHeight + Theme.spaceXl * 2
                radius: Theme.radiusLg
                color: Theme.surface
                border.color: Theme.rgba(Theme.border, 0.35)

                ColumnLayout {
                    id: dacCard
                    anchors.fill: parent
                    anchors.margins: Theme.spaceXl
                    spacing: Theme.spaceMd

                    Label { text: "Audio Output"; color: Theme.foreground; font.pixelSize: Theme.fontTitle; font.bold: true }
                    Label {
                        text: App.playback.audioBackend.length > 0
                              ? "●  Backend: " + App.playback.audioBackend
                              : "○  Backend: idle (start playback to see the active output)"
                        color: App.playback.audioBackend.length > 0 ? Theme.success : Theme.muted
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Label { text: "DAC Passthrough"; color: Theme.foreground; font.pixelSize: Theme.fontTitle; font.bold: true }
                    Label {
                        text: "Bit-perfect output for a USB DAC: follows the file's sample rate/format, disables software volume and ReplayGain, gapless on."
                        color: Theme.muted
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Switch {
                        id: dacSwitch
                        text: "Enable DAC passthrough"
                        checked: App.playback.dacPassthrough
                        onToggled: App.setDacPassthrough(checked)
                    }
                    Label {
                        text: App.playback.dacPassthrough
                              ? "●  Active — volume locked at 100%, use the DAC knob. Engine restarts on toggle. Note: direct hardware output is not visible to per-app mixers."
                              : "○  Off — normal PipeWire output with software volume and per-app mixer entry."
                        color: App.playback.dacPassthrough ? Theme.success : Theme.muted
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceMd

                PrimaryButton {
                    text: settingsView.savedFlash ? "Saved ✓" : "Save changes"
                    enabled: settingsView.dirty
                    onClicked: {
                        App.saveSettings(libraryPathsField.text.trim(),
                                         inboxField.text.trim(),
                                         lyricsDirField.text.trim(),
                                         beetsBinaryField.text.trim(),
                                         beetsNomoveField.checked,
                                         discogsTokenField.text.trim(),
                                         fontField.text.trim(),
                                         fontSizeField.value,
                                         scanOnLaunchField.checked,
                                         libraryWatchField.checked,
                                         wasdField.checked,
                                         tooltipsField.checked,
                                         neteaseField.checked,
                                         plainField.checked)
                        discogsTokenField.text = ""
                        settingsView.markClean()
                    }
                }
                Label {
                    text: settingsView.dirty ? "Unsaved changes"
                          : settingsView.savedFlash ? "All changes saved"
                          : App.config.configPath
                    color: settingsView.dirty ? Theme.warning
                           : settingsView.savedFlash ? Theme.success : Theme.muted
                    opacity: 0.85
                    font.pixelSize: Theme.fontCaption
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
            }
        }
    }

    function isDirty() {
        if (!libraryPathsField || !inboxField || !lyricsDirField
            || !beetsBinaryField || !beetsNomoveField || !fontField
            || !fontSizeField || !discogsTokenField || !scanOnLaunchField
            || !libraryWatchField
            || !wasdField || !tooltipsField || !neteaseField || !plainField)
            return settingsView.dirty
        return libraryPathsField.text.trim() !== App.config.libraryPaths.join(", ")
            || inboxField.text.trim() !== App.config.importInbox
            || lyricsDirField.text.trim() !== App.config.lyricsDir
            || beetsBinaryField.text.trim() !== App.config.beetsBinary
            || beetsNomoveField.checked !== App.config.beetsNomove
            || fontField.text.trim() !== App.config.uiFontFamily
            || fontSizeField.value !== App.config.uiFontSize
            || scanOnLaunchField.checked !== App.config.scanOnLaunch
            || libraryWatchField.checked !== App.config.libraryWatchEnabled
            || wasdField.checked !== App.config.wasdNavigation
            || tooltipsField.checked !== App.config.tooltipsEnabled
            || neteaseField.checked !== App.config.lyricsNeteaseEnabled
            || plainField.checked !== App.config.lyricsPlainEnabled
            || discogsTokenField.text.trim().length > 0
    }

    FolderDialog {
        id: libraryFolderDialog
        title: "Choose music folder"
        currentFolder: App.config.expandPath(App.config.libraryPaths.length > 0
                                             ? App.config.libraryPaths[0] : "~/Music")
        onAccepted: {
            libraryPathsField.text = selectedFolder.toString().replace("file://", "")
            settingsView.dirty = settingsView.isDirty()
        }
    }

    FolderDialog {
        id: inboxFolderDialog
        title: "Choose import inbox folder"
        currentFolder: App.config.expandPath(App.config.importInbox.length > 0
                                             ? App.config.importInbox : "~/Music")
        onAccepted: {
            inboxField.text = selectedFolder.toString().replace("file://", "")
            settingsView.dirty = settingsView.isDirty()
        }
    }

    FolderDialog {
        id: lyricsFolderDialog
        title: "Choose lyrics folder"
        currentFolder: App.config.expandPath(App.config.lyricsDir.length > 0
                                             ? App.config.lyricsDir : "~/Music")
        onAccepted: {
            lyricsDirField.text = selectedFolder.toString().replace("file://", "")
            settingsView.dirty = settingsView.isDirty()
        }
    }
}
