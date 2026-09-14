import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import components 1.0

Pane {
    id: settingsView
    padding: 0

    property int currentSection: 0
    property bool dirty: false
    property bool savedFlash: false

    background: Rectangle { color: Theme.background }

    function selectSection(index) {
        currentSection = Math.max(0, Math.min(6, index))
        if (currentSection === 1)
            App.importInbox.scanInbox()
    }

    function sectionTitle() {
        return ["Library", "Import", "Audio", "Lyrics", "Appearance", "Integrations", "Enrich library"][currentSection]
    }

    function sectionSubtitle() {
        return [
            "Music locations, scanning, and library health.",
            "Review FLAC albums and bring clean Opus copies into your library.",
            "Playback backend and bit-perfect output controls.",
            "Lyrics locations, providers, and credentials.",
            "Typography and interaction preferences.",
            "Metadata services and securely stored credentials.",
            "Fetch artist & album info, fix titles, then lyrics — pauses when a match is unclear."
        ][currentSection]
    }

    function markClean() {
        dirty = false
        savedFlash = true
        savedTimer.restart()
    }

    function saveChanges() {
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
                         plainField.checked,
                         opusBitrateField.model[opusBitrateField.currentIndex],
                         importModeField.currentValue)
        discogsTokenField.text = ""
        markClean()
    }

    function isDirty() {
        if (!libraryPathsField || !inboxField || !lyricsDirField
            || !beetsBinaryField || !beetsNomoveField || !fontField
            || !fontSizeField || !discogsTokenField || !scanOnLaunchField
            || !libraryWatchField || !opusBitrateField || !importModeField
            || !wasdField || !tooltipsField || !neteaseField || !plainField)
            return dirty
        return libraryPathsField.text.trim() !== App.config.libraryPaths.join(", ")
            || inboxField.text.trim() !== App.config.importInbox
            || lyricsDirField.text.trim() !== App.config.lyricsDir
            || beetsBinaryField.text.trim() !== App.config.beetsBinary
            || beetsNomoveField.checked !== App.config.beetsNomove
            || importModeField.currentValue !== App.config.importMode
            || opusBitrateField.model[opusBitrateField.currentIndex] !== App.config.opusBitrateKbps
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

    Timer {
        id: savedTimer
        interval: 2500
        onTriggered: settingsView.savedFlash = false
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 232
            Layout.minimumWidth: 220
            Layout.fillHeight: true
            color: Theme.darkBackground
            border.width: 1
            border.color: Theme.rgba(Theme.border, 0.35)

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spaceLg
                spacing: Theme.spaceXs

                RowLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: Theme.spaceLg
                    spacing: Theme.spaceMd

                    Rectangle {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        radius: Theme.radiusMd
                        color: Theme.rgba(Theme.accent, 0.15)
                        border.width: 1
                        border.color: Theme.rgba(Theme.accent, 0.42)

                        AppIcon {
                            anchors.centerIn: parent
                            name: "preferences-system-symbolic"
                            size: 20
                            iconColor: Theme.accent
                        }
                    }

                    ColumnLayout {
                        spacing: 0
                        Label {
                            text: "Settings"
                            color: Theme.foreground
                            font.pixelSize: Theme.fontHeading
                            font.bold: true
                        }
                        Label {
                            text: "Hyprplay"
                            color: Theme.muted
                            font.pixelSize: Theme.fontCaption
                        }
                    }
                }

                SettingsNavItem {
                    Layout.fillWidth: true
                    text: "Library"
                    iconName: "folder-music-symbolic"
                    selected: currentSection === 0
                    onClicked: settingsView.selectSection(0)
                }
                SettingsNavItem {
                    Layout.fillWidth: true
                    text: "Import"
                    iconName: "folder-download-symbolic"
                    selected: currentSection === 1
                    onClicked: settingsView.selectSection(1)
                }
                SettingsNavItem {
                    Layout.fillWidth: true
                    text: "Audio"
                    iconName: "audio-card-symbolic"
                    selected: currentSection === 2
                    onClicked: settingsView.selectSection(2)
                }
                SettingsNavItem {
                    Layout.fillWidth: true
                    text: "Lyrics"
                    iconName: "document-edit-symbolic"
                    selected: currentSection === 3
                    onClicked: settingsView.selectSection(3)
                }
                SettingsNavItem {
                    Layout.fillWidth: true
                    text: "Appearance"
                    iconName: "preferences-desktop-theme-symbolic"
                    selected: currentSection === 4
                    onClicked: settingsView.selectSection(4)
                }
                SettingsNavItem {
                    Layout.fillWidth: true
                    text: "Integrations"
                    iconName: "network-server-symbolic"
                    selected: currentSection === 5
                    onClicked: settingsView.selectSection(5)
                }
                SettingsNavItem {
                    Layout.fillWidth: true
                    text: "Enrich library"
                    iconName: "folder-download-symbolic"
                    selected: currentSection === 6
                    onClicked: settingsView.selectSection(6)
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: themeInfo.implicitHeight + Theme.spaceMd * 2
                    radius: Theme.radiusMd
                    color: Theme.rgba(Theme.surface, 0.65)
                    border.width: 1
                    border.color: Theme.rgba(Theme.border, 0.28)

                    ColumnLayout {
                        id: themeInfo
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: Theme.spaceMd
                        spacing: 2
                        Label {
                            text: "OMARCHY THEME"
                            color: Theme.muted
                            font.pixelSize: Theme.fontCaption
                            font.bold: true
                        }
                        Label {
                            text: Theme.themeName
                            color: Theme.accent
                            font.pixelSize: Theme.fontSmall
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 88

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Theme.spaceXl
                    anchors.rightMargin: Theme.spaceXl
                    spacing: 3

                    Label {
                        text: settingsView.sectionTitle()
                        color: Theme.foreground
                        font.pixelSize: Theme.fontDisplay
                        font.bold: true
                    }
                    Label {
                        text: settingsView.sectionSubtitle()
                        color: Theme.muted
                        font.pixelSize: Theme.fontBody
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.rgba(Theme.border, 0.28)
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: settingsView.currentSection

                ScrollView {
                    id: libraryPage
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        width: Math.min(860, libraryPage.availableWidth - Theme.spaceXl * 2)
                        x: Math.max(Theme.spaceXl, (libraryPage.availableWidth - width) / 2)
                        spacing: Theme.spaceLg
                        Item { Layout.preferredHeight: Theme.spaceSm }

                        AppCard {
                            Layout.fillWidth: true
                            cardPadding: Theme.spaceXl
                            elevated: true
                            implicitHeight: libraryLocations.implicitHeight + Theme.spaceXl * 2
                            ColumnLayout {
                                id: libraryLocations
                                Layout.fillWidth: true
                                spacing: Theme.spaceMd
                                Label { text: "Music locations"; color: Theme.foreground; font.pixelSize: Theme.fontTitle; font.bold: true }
                                Label { text: "Choose the folders that make up your local collection."; color: Theme.muted; font.pixelSize: Theme.fontSmall }
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
                                    Button { text: "Browse…"; onClicked: libraryFolderDialog.open() }
                                }
                            }
                        }

                        AppCard {
                            Layout.fillWidth: true
                            cardPadding: Theme.spaceXl
                            implicitHeight: scanningContent.implicitHeight + Theme.spaceXl * 2
                            ColumnLayout {
                                id: scanningContent
                                Layout.fillWidth: true
                                spacing: Theme.spaceMd
                                Label { text: "Scanning"; color: Theme.foreground; font.pixelSize: Theme.fontTitle; font.bold: true }
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
                                    text: "New and removed tracks are detected automatically when watching is enabled."
                                    color: Theme.muted
                                    font.pixelSize: Theme.fontCaption
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spaceMd
                                    PrimaryButton { text: "Rescan library"; onClicked: App.rescanLibrary() }
                                    Label {
                                        text: App.scanStatus.length > 0 ? App.scanStatus : "Library is up to date"
                                        color: App.library.scanning ? Theme.accent : Theme.muted
                                        font.pixelSize: Theme.fontCaption
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                        Item { Layout.preferredHeight: Theme.spaceSm }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spaceXl
                        anchors.rightMargin: Theme.spaceXl
                        anchors.topMargin: Theme.spaceLg
                        anchors.bottomMargin: Theme.spaceLg
                        spacing: Theme.spaceMd

                        AppCard {
                            Layout.fillWidth: true
                            cardPadding: Theme.spaceLg
                            implicitHeight: importSetup.implicitHeight + Theme.spaceLg * 2
                            ColumnLayout {
                                id: importSetup
                                Layout.fillWidth: true
                                spacing: Theme.spaceMd
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: "Import setup"; color: Theme.foreground; font.pixelSize: Theme.fontTitle; font.bold: true; Layout.fillWidth: true }
                                    Label {
                                        text: App.beets.available ? "beet ready" : "beet optional"
                                        color: App.beets.available ? Theme.success : Theme.muted
                                        font.pixelSize: Theme.fontCaption
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spaceMd
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.spaceXs
                                        Label { text: "Inbox folder"; color: Theme.foreground; font.bold: true }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            TextField {
                                                id: inboxField
                                                Layout.fillWidth: true
                                                text: App.config.importInbox
                                                placeholderText: "~/Music/inbox"
                                                onTextChanged: settingsView.dirty = settingsView.isDirty()
                                            }
                                            Button { text: "Browse…"; onClicked: inboxFolderDialog.open() }
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.preferredWidth: 200
                                        spacing: Theme.spaceXs
                                        Label { text: "Import mode"; color: Theme.foreground; font.bold: true }
                                        ComboBox {
                                            id: importModeField
                                            Layout.fillWidth: true
                                            textRole: "label"
                                            valueRole: "value"
                                            model: [
                                                { label: "Copy as-is", value: "copy" },
                                                { label: "Convert FLAC→Opus", value: "convert_opus" }
                                            ]
                                            currentIndex: {
                                                const mode = App.config.importMode
                                                for (let i = 0; i < model.length; ++i) {
                                                    if (model[i].value === mode)
                                                        return i
                                                }
                                                return 0
                                            }
                                            onActivated: settingsView.dirty = settingsView.isDirty()
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.preferredWidth: 140
                                        spacing: Theme.spaceXs
                                        Label {
                                            text: "Opus bitrate"
                                            color: Theme.foreground
                                            font.bold: true
                                            opacity: importModeField.currentValue === "convert_opus" ? 1 : 0.45
                                        }
                                        ComboBox {
                                            id: opusBitrateField
                                            Layout.fillWidth: true
                                            enabled: importModeField.currentValue === "convert_opus"
                                            model: [128, 160, 192, 256, 320]
                                            currentIndex: {
                                                const i = model.indexOf(App.config.opusBitrateKbps)
                                                return i >= 0 ? i : model.indexOf(256)
                                            }
                                            onActivated: settingsView.dirty = settingsView.isDirty()
                                        }
                                    }
                                }
                                Label {
                                    text: importModeField.currentValue === "convert_opus"
                                          ? "FLAC tracks are converted to Opus; other formats are copied. Requires opusenc or ffmpeg."
                                          : "Tracks are copied into the library preserving format (FLAC, Opus, MP3, M4A)."
                                    color: Theme.muted
                                    font.pixelSize: Theme.fontCaption
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spaceMd
                                    TextField {
                                        id: beetsBinaryField
                                        Layout.fillWidth: true
                                        text: App.config.beetsBinary
                                        placeholderText: "beet binary"
                                        onTextChanged: settingsView.dirty = settingsView.isDirty()
                                    }
                                    CheckBox {
                                        id: beetsNomoveField
                                        text: "Beets no-move mode"
                                        checked: App.config.beetsNomove
                                        onToggled: settingsView.dirty = settingsView.isDirty()
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 260
                            radius: Theme.radiusLg
                            color: Theme.surface
                            border.width: 1
                            border.color: Theme.rgba(Theme.border, 0.35)
                            Loader {
                                id: embeddedImportLoader
                                anchors.fill: parent
                                anchors.margins: Theme.spaceLg
                                source: "qrc:/views/ImportView.qml"
                                onLoaded: if (item) item.embedded = true
                                onStatusChanged: {
                                    if (status === Loader.Error)
                                        console.error("Embedded ImportView failed to load")
                                }
                            }
                            Connections {
                                target: embeddedImportLoader.item
                                function onConfigureRequested() {
                                    settingsView.selectSection(1)
                                    inboxField.forceActiveFocus()
                                }
                            }
                        }
                    }
                }

                ScrollView {
                    id: audioPage
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    ColumnLayout {
                        width: Math.min(860, audioPage.availableWidth - Theme.spaceXl * 2)
                        x: Math.max(Theme.spaceXl, (audioPage.availableWidth - width) / 2)
                        spacing: Theme.spaceLg
                        Item { Layout.preferredHeight: Theme.spaceSm }
                        AppCard {
                            Layout.fillWidth: true
                            cardPadding: Theme.spaceXl
                            elevated: true
                            implicitHeight: audioContent.implicitHeight + Theme.spaceXl * 2
                            ColumnLayout {
                                id: audioContent
                                Layout.fillWidth: true
                                spacing: Theme.spaceMd
                                Label { text: "Audio output"; color: Theme.foreground; font.pixelSize: Theme.fontTitle; font.bold: true }
                                Label {
                                    text: App.playback.audioBackend.length > 0
                                          ? "Backend: " + App.playback.audioBackend
                                          : "Backend idle — start playback to see the active output"
                                    color: App.playback.audioBackend.length > 0 ? Theme.success : Theme.muted
                                    font.pixelSize: Theme.fontSmall
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                Label {
                                    visible: App.playback.dacPassthrough
                                             && (App.playback.audioBackend.indexOf("pipewire") === 0
                                                 || App.playback.audioBackend.indexOf("pulse") === 0
                                                 || (App.playback.audioBackend.length === 0 && App.playback.playing))
                                    text: "⚠ Passthrough requested but output is PipeWire/Pulse — same mixer path as normal mode."
                                    color: Theme.warning
                                    font.pixelSize: Theme.fontSmall
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                Label {
                                    visible: audioDevicePicker.count > 1
                                    text: "Output device"
                                    color: Theme.foreground
                                    font.pixelSize: Theme.fontSubtitle
                                    font.bold: true
                                }
                                ComboBox {
                                    id: audioDevicePicker
                                    Layout.fillWidth: true
                                    visible: count > 1
                                    textRole: "description"
                                    model: {
                                        // Depend on the backend string so the list refreshes after engine restarts.
                                        App.playback.audioBackend
                                        var items = [{ "name": "", "description": "System default" }]
                                        var devices = App.playback.audioDeviceList()
                                        for (var i = 0; i < devices.length; ++i) {
                                            items.push(devices[i])
                                        }
                                        return items
                                    }
                                    currentIndex: {
                                        var target = App.playback.audioDevice
                                        for (var i = 0; i < model.length; ++i) {
                                            if (model[i].name === target) {
                                                return i
                                            }
                                        }
                                        return target.length > 0 ? -1 : 0
                                    }
                                    displayText: currentIndex < 0 && App.playback.audioDevice.length > 0 ? App.playback.audioDevice : currentText
                                    onActivated: (index) => {
                                        var entry = model[index]
                                        var name = entry ? entry.name : ""
                                        App.playback.setAudioDevice(name)
                                        App.config.setAudioDevice(name)
                                    }
                                }
                                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.rgba(Theme.border, 0.3) }
                                Label { text: "DAC passthrough"; color: Theme.foreground; font.pixelSize: Theme.fontSubtitle; font.bold: true }
                                Label {
                                    text: "Bit-perfect only when Backend shows alsa; volume locked at 100%, mute disabled, ReplayGain off, gapless on."
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
                                          ? "Active — volume locked at 100%; use the DAC knob."
                                          : "Off — PipeWire software volume and per-app mixing."
                                    color: App.playback.dacPassthrough ? Theme.success : Theme.muted
                                    font.pixelSize: Theme.fontSmall
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                            }
                        }
                        Item { Layout.preferredHeight: Theme.spaceSm }
                    }
                }

                ScrollView {
                    id: lyricsPage
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    ColumnLayout {
                        width: Math.min(860, lyricsPage.availableWidth - Theme.spaceXl * 2)
                        x: Math.max(Theme.spaceXl, (lyricsPage.availableWidth - width) / 2)
                        spacing: Theme.spaceLg
                        Item { Layout.preferredHeight: Theme.spaceSm }
                        AppCard {
                            Layout.fillWidth: true
                            cardPadding: Theme.spaceXl
                            implicitHeight: lyricsLocationContent.implicitHeight + Theme.spaceXl * 2
                            ColumnLayout {
                                id: lyricsLocationContent
                                Layout.fillWidth: true
                                spacing: Theme.spaceMd
                                Label { text: "Lyrics location"; color: Theme.foreground; font.pixelSize: Theme.fontTitle; font.bold: true }
                                Label { text: "Sidecar lyrics are also searched here."; color: Theme.muted; font.pixelSize: Theme.fontSmall }
                                RowLayout {
                                    Layout.fillWidth: true
                                    TextField {
                                        id: lyricsDirField
                                        Layout.fillWidth: true
                                        text: App.config.lyricsDir
                                        placeholderText: "~/Music/Lyrics"
                                        onTextChanged: settingsView.dirty = settingsView.isDirty()
                                    }
                                    Button { text: "Browse…"; onClicked: lyricsFolderDialog.open() }
                                }
                            }
                        }
                        AppCard {
                            Layout.fillWidth: true
                            cardPadding: Theme.spaceXl
                            elevated: true
                            implicitHeight: lyricsProvidersContent.implicitHeight + Theme.spaceXl * 2
                            ColumnLayout {
                                id: lyricsProvidersContent
                                Layout.fillWidth: true
                                spacing: Theme.spaceMd
                                Label { text: "Providers"; color: Theme.foreground; font.pixelSize: Theme.fontTitle; font.bold: true }
                                Label {
                                    text: "LRCLIB is always tried first. Enabled fallbacks run one request at a time."
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
                                Label { text: "Genius access token"; color: Theme.foreground; font.bold: true }
                                RowLayout {
                                    Layout.fillWidth: true
                                    TextField {
                                        id: geniusTokenField
                                        Layout.fillWidth: true
                                        echoMode: TextInput.Password
                                        placeholderText: App.lyrics.geniusTokenSet
                                                         ? "Token configured — enter a new one to replace"
                                                         : "Token from genius.com/api-clients"
                                    }
                                    PrimaryButton {
                                        text: "Save token"
                                        enabled: geniusTokenField.text.trim().length > 0
                                        onClicked: {
                                            App.lyrics.setGeniusToken(geniusTokenField.text.trim())
                                            geniusTokenField.text = ""
                                        }
                                    }
                                }
                                Label {
                                    text: App.lyrics.geniusTokenSet ? "Genius token set" : "Genius not configured"
                                    color: App.lyrics.geniusTokenSet ? Theme.success : Theme.muted
                                    font.pixelSize: Theme.fontSmall
                                }
                            }
                        }
                        AppCard {
                            Layout.fillWidth: true
                            cardPadding: Theme.spaceXl
                            elevated: true
                            implicitHeight: lyricsFetchContent.implicitHeight + Theme.spaceXl * 2
                            ColumnLayout {
                                id: lyricsFetchContent
                                Layout.fillWidth: true
                                spacing: Theme.spaceMd
                                Label {
                                    text: "Fetch all lyrics"
                                    color: Theme.foreground
                                    font.pixelSize: Theme.fontTitle
                                    font.bold: true
                                }
                                Label {
                                    text: "Queue every library track for lyrics. Tracks that already have sidecars are skipped. Cancel stops the remaining queue."
                                    color: Theme.muted
                                    font.pixelSize: Theme.fontSmall
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spaceMd
                                    PrimaryButton {
                                        text: App.lyrics.busy ? "Fetching…" : "Fetch all lyrics"
                                        enabled: !App.lyrics.busy
                                                 && !App.enrichment.active
                                                 && App.library.trackCount > 0
                                        onClicked: App.fetchLyricsForLibrary()
                                    }
                                    Button {
                                        text: "Cancel"
                                        enabled: App.lyrics.busy && !App.enrichment.active
                                        onClicked: App.lyrics.cancel()
                                    }
                                    Item { Layout.fillWidth: true }
                                    Label {
                                        text: App.lyrics.busy ? "Running" : "Idle"
                                        color: App.lyrics.busy ? Theme.accent : Theme.muted
                                        font.pixelSize: Theme.fontSmall
                                        font.bold: true
                                    }
                                }
                                ProgressBar {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: Math.max(1, App.lyrics.total)
                                    value: App.lyrics.progress
                                    visible: App.lyrics.busy || App.lyrics.total > 0
                                }
                                Label {
                                    Layout.fillWidth: true
                                    visible: App.lyrics.status.length > 0
                                    text: App.lyrics.busy
                                          ? ("Lyrics " + App.lyrics.progress + "/" + App.lyrics.total
                                             + (App.lyrics.status.length > 0
                                                ? (" · " + App.lyrics.status) : ""))
                                          : App.lyrics.status
                                    color: Theme.muted
                                    font.pixelSize: Theme.fontSmall
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                        Item { Layout.preferredHeight: Theme.spaceSm }
                    }
                }

                ScrollView {
                    id: appearancePage
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    ColumnLayout {
                        width: Math.min(860, appearancePage.availableWidth - Theme.spaceXl * 2)
                        x: Math.max(Theme.spaceXl, (appearancePage.availableWidth - width) / 2)
                        spacing: Theme.spaceLg
                        Item { Layout.preferredHeight: Theme.spaceSm }
                        AppCard {
                            Layout.fillWidth: true
                            cardPadding: Theme.spaceXl
                            elevated: true
                            implicitHeight: appearanceContent.implicitHeight + Theme.spaceXl * 2
                            ColumnLayout {
                                id: appearanceContent
                                Layout.fillWidth: true
                                spacing: Theme.spaceMd
                                Label { text: "Typography"; color: Theme.foreground; font.pixelSize: Theme.fontTitle; font.bold: true }
                                Label { text: "Omarchy supplies colors; choose how text and controls feel."; color: Theme.muted; font.pixelSize: Theme.fontSmall }
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
                                    SpinBox {
                                        id: fontSizeField
                                        from: 9
                                        to: 24
                                        value: App.config.uiFontSize
                                        editable: true
                                        onValueChanged: settingsView.dirty = settingsView.isDirty()
                                    }
                                    Label { text: fontSizeField.value + " px"; color: Theme.muted; font.pixelSize: Theme.fontSmall }
                                    Item { Layout.fillWidth: true }
                                }
                                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.rgba(Theme.border, 0.3) }
                                Label { text: "Interaction"; color: Theme.foreground; font.pixelSize: Theme.fontSubtitle; font.bold: true }
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
                        Item { Layout.preferredHeight: Theme.spaceSm }
                    }
                }

                ScrollView {
                    id: integrationsPage
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    ColumnLayout {
                        width: Math.min(860, integrationsPage.availableWidth - Theme.spaceXl * 2)
                        x: Math.max(Theme.spaceXl, (integrationsPage.availableWidth - width) / 2)
                        spacing: Theme.spaceLg
                        Item { Layout.preferredHeight: Theme.spaceSm }
                        AppCard {
                            Layout.fillWidth: true
                            cardPadding: Theme.spaceXl
                            elevated: true
                            implicitHeight: discogsContent.implicitHeight + Theme.spaceXl * 2
                            ColumnLayout {
                                id: discogsContent
                                Layout.fillWidth: true
                                spacing: Theme.spaceMd
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: "Discogs"; color: Theme.foreground; font.pixelSize: Theme.fontTitle; font.bold: true; Layout.fillWidth: true }
                                    Label {
                                        text: App.discogs.hasToken ? "Connected" : "Not configured"
                                        color: App.discogs.hasToken ? Theme.success : Theme.muted
                                        font.pixelSize: Theme.fontSmall
                                    }
                                }
                                Label {
                                    text: "Fetch artist profiles, album notes, release details, and artwork."
                                    color: Theme.muted
                                    font.pixelSize: Theme.fontSmall
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: secretInfo.implicitHeight + Theme.spaceMd * 2
                                    radius: Theme.radiusMd
                                    color: Theme.rgba(Theme.darkBackground, 0.55)
                                    ColumnLayout {
                                        id: secretInfo
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.margins: Theme.spaceMd
                                        Label { text: "SECURE STORAGE"; color: Theme.accent; font.pixelSize: Theme.fontCaption; font.bold: true }
                                        Label {
                                            text: App.config.secretsStorageDescription()
                                            color: Theme.muted
                                            font.pixelSize: Theme.fontCaption
                                            wrapMode: Text.WordWrap
                                            Layout.fillWidth: true
                                        }
                                    }
                                }
                                Label { text: "Personal access token"; color: Theme.foreground; font.bold: true }
                                TextField {
                                    id: discogsTokenField
                                    Layout.fillWidth: true
                                    echoMode: TextInput.Password
                                    placeholderText: App.discogs.hasToken
                                                     ? "Token configured — leave blank to keep"
                                                     : "Enter Discogs token"
                                    onTextChanged: settingsView.dirty = settingsView.isDirty()
                                }
                                Label {
                                    text: App.discogs.status.length > 0
                                          ? App.discogs.status
                                          : "Used by artist and album right-click actions."
                                    color: Theme.muted
                                    font.pixelSize: Theme.fontSmall
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                        Item { Layout.preferredHeight: Theme.spaceSm }
                    }
                }

                ScrollView {
                    id: enrichmentPage
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    ColumnLayout {
                        width: Math.min(860, enrichmentPage.availableWidth - Theme.spaceXl * 2)
                        x: Math.max(Theme.spaceXl, (enrichmentPage.availableWidth - width) / 2)
                        spacing: Theme.spaceLg
                        Item { Layout.preferredHeight: Theme.spaceSm }

                        AppCard {
                            Layout.fillWidth: true
                            cardPadding: Theme.spaceXl
                            elevated: true
                            implicitHeight: enrichHowContent.implicitHeight + Theme.spaceXl * 2
                            ColumnLayout {
                                id: enrichHowContent
                                Layout.fillWidth: true
                                spacing: Theme.spaceMd
                                Label {
                                    text: "How it works"
                                    color: Theme.foreground
                                    font.pixelSize: Theme.fontTitle
                                    font.bold: true
                                }
                                Label {
                                    text: "Runs a whole-library pass in order. Exact single matches apply automatically; anything unclear pauses for a choice."
                                    color: Theme.muted
                                    font.pixelSize: Theme.fontSmall
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spaceSm
                                    Repeater {
                                        model: [
                                            "1. Artist info — Discogs profile & artwork",
                                            "2. Album info — Discogs notes & cover",
                                            "3. Title Fix — match track titles to an official release",
                                            "4. Lyrics — fetch lyrics for every track"
                                        ]
                                        delegate: Label {
                                            text: modelData
                                            color: Theme.foreground
                                            font.pixelSize: Theme.fontSmall
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }
                            }
                        }

                        AppCard {
                            Layout.fillWidth: true
                            cardPadding: Theme.spaceXl
                            elevated: true
                            implicitHeight: enrichPrereqContent.implicitHeight + Theme.spaceXl * 2
                            ColumnLayout {
                                id: enrichPrereqContent
                                Layout.fillWidth: true
                                spacing: Theme.spaceMd
                                Label {
                                    text: "Prerequisites"
                                    color: Theme.foreground
                                    font.pixelSize: Theme.fontTitle
                                    font.bold: true
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        text: "Discogs"
                                        color: Theme.foreground
                                        font.bold: true
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        text: App.discogs.hasToken ? "Connected" : "Not configured"
                                        color: App.discogs.hasToken ? Theme.success : Theme.muted
                                        font.pixelSize: Theme.fontSmall
                                    }
                                }
                                Button {
                                    text: "Configure in Integrations"
                                    flat: true
                                    onClicked: settingsView.selectSection(5)
                                }
                                Label {
                                    text: "Genius token is optional — configure it under Lyrics if you want that provider in the final lyrics pass."
                                    color: Theme.muted
                                    font.pixelSize: Theme.fontCaption
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        AppCard {
                            Layout.fillWidth: true
                            cardPadding: Theme.spaceXl
                            elevated: true
                            implicitHeight: enrichRunContent.implicitHeight + Theme.spaceXl * 2
                            ColumnLayout {
                                id: enrichRunContent
                                Layout.fillWidth: true
                                spacing: Theme.spaceMd
                                Label {
                                    text: "Run"
                                    color: Theme.foreground
                                    font.pixelSize: Theme.fontTitle
                                    font.bold: true
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spaceMd
                                    PrimaryButton {
                                        text: "Start"
                                        enabled: !App.enrichment.active && App.discogs.hasToken
                                        onClicked: App.enrichment.start()
                                    }
                                    Button {
                                        text: "Cancel"
                                        enabled: App.enrichment.active
                                        onClicked: App.enrichment.cancel()
                                    }
                                    Item { Layout.fillWidth: true }
                                    Label {
                                        text: App.enrichment.active
                                              ? (App.enrichment.paused ? "Paused" : "Running")
                                              : (App.enrichment.phase === "done" ? "Done" : "Idle")
                                        color: App.enrichment.paused ? Theme.warning
                                               : App.enrichment.active ? Theme.accent : Theme.muted
                                        font.pixelSize: Theme.fontSmall
                                        font.bold: true
                                    }
                                }
                                ProgressBar {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: Math.max(1, App.enrichment.total)
                                    value: App.enrichment.progress
                                    visible: App.enrichment.active || App.enrichment.phase === "done"
                                }
                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Repeater {
                                        model: [
                                            { id: "artist", label: "Artist" },
                                            { id: "album", label: "Album" },
                                            { id: "titleFix", label: "Title Fix" },
                                            { id: "lyrics", label: "Lyrics" }
                                        ]
                                        delegate: Rectangle {
                                            radius: 8
                                            color: App.enrichment.phase === modelData.id
                                                   ? Theme.rgba(Theme.accent, 0.18)
                                                   : Theme.rgba(Theme.foreground, 0.05)
                                            border.width: App.enrichment.phase === modelData.id ? 1 : 0
                                            border.color: Theme.rgba(Theme.accent, 0.35)
                                            width: chipLabel.implicitWidth + 16
                                            height: 26
                                            Label {
                                                id: chipLabel
                                                anchors.centerIn: parent
                                                text: modelData.label
                                                font.pixelSize: Theme.fontCaption
                                                color: App.enrichment.phase === modelData.id
                                                       ? Theme.accent : Theme.muted
                                            }
                                        }
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: App.enrichment.status
                                    color: Theme.muted
                                    font.pixelSize: Theme.fontSmall
                                    wrapMode: Text.WordWrap
                                    visible: text.length > 0
                                }
                                Label {
                                    Layout.fillWidth: true
                                    visible: App.enrichment.currentArtist.length > 0
                                    text: App.enrichment.currentAlbum.length > 0
                                          ? (App.enrichment.currentArtist + " — " + App.enrichment.currentAlbum)
                                          : App.enrichment.currentArtist
                                    color: Theme.foreground
                                    font.pixelSize: Theme.fontSmall
                                    elide: Text.ElideRight
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: "Auto " + App.enrichment.autoApplied
                                          + " · Resolved " + App.enrichment.resolved
                                          + " · Skipped " + App.enrichment.skipped
                                          + " · Failed " + App.enrichment.failed
                                          + (App.enrichment.total > 0
                                             ? (" · " + App.enrichment.progress + "/" + App.enrichment.total)
                                             : "")
                                    color: Theme.muted
                                    font.pixelSize: Theme.fontCaption
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                        Item { Layout.preferredHeight: Theme.spaceSm }
                    }
                }

            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 68
                color: Theme.darkBackground
                border.width: 1
                border.color: Theme.rgba(Theme.border, 0.35)
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spaceXl
                    anchors.rightMargin: Theme.spaceXl
                    spacing: Theme.spaceMd
                    Rectangle {
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: settingsView.dirty ? Theme.warning
                               : settingsView.savedFlash ? Theme.success : Theme.muted
                        opacity: settingsView.dirty || settingsView.savedFlash ? 1 : 0.45
                    }
                    Label {
                        text: settingsView.dirty ? "Unsaved changes"
                              : settingsView.savedFlash ? "All changes saved"
                              : App.config.configPath
                        color: settingsView.dirty ? Theme.warning
                               : settingsView.savedFlash ? Theme.success : Theme.muted
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                    PrimaryButton {
                        text: settingsView.savedFlash ? "Saved ✓" : "Save changes"
                        enabled: settingsView.dirty
                        onClicked: settingsView.saveChanges()
                    }
                }
            }
        }
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
