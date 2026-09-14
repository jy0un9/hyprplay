import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import components 1.0

Pane {
    id: settingsView
    padding: 0

    property string section: "library"
    property string flashText: ""

    readonly property var sections: [
        { key: "library", label: "Library", icon: "folder-music-symbolic", group: "prefs",
          subtitle: "Where your music lives and how it is scanned." },
        { key: "audio", label: "Audio", icon: "audio-card-symbolic", group: "prefs",
          subtitle: "Output device and bit-perfect playback." },
        { key: "lyrics", label: "Lyrics", icon: "document-edit-symbolic", group: "prefs",
          subtitle: "Sidecar location, providers, and bulk fetching." },
        { key: "appearance", label: "Appearance", icon: "preferences-desktop-theme-symbolic", group: "prefs",
          subtitle: "Typography and input, on top of your Omarchy colors." },
        { key: "integrations", label: "Integrations", icon: "network-server-symbolic", group: "prefs",
          subtitle: "Metadata services and stored credentials." },
        { key: "import", label: "Import", icon: "folder-download-symbolic", group: "tools",
          subtitle: "Copy albums from an inbox folder into your library." },
        { key: "convert", label: "Convert", icon: "media-playlist-repeat-symbolic", group: "tools",
          subtitle: "Re-encode FLAC albums already in your library to Opus." },
        { key: "enrich", label: "Enrich library", icon: "starred-symbolic", group: "tools",
          subtitle: "One pass over the whole library for artwork, notes, titles, and lyrics." }
    ]

    function sectionIndex(key) {
        for (let i = 0; i < sections.length; i++) {
            if (sections[i].key === key)
                return i
        }
        return 0
    }

    function showSection(key) {
        settingsView.section = key
        if (key === "import")
            App.importInbox.scanInbox()
        else if (key === "convert")
            App.convert.scanLibrary()
    }

    function flash(text) {
        settingsView.flashText = text
        flashTimer.restart()
    }

    // Enrichment runs these phases in order; report each one's state for the pipeline list.
    function phaseState(id) {
        const order = ["artist", "album", "titleFix", "lyrics"]
        if (App.enrichment.phase === "done")
            return "done"
        if (!App.enrichment.active)
            return "idle"
        const current = order.indexOf(App.enrichment.phase)
        const mine = order.indexOf(id)
        if (current === mine)
            return App.enrichment.paused ? "paused" : "active"
        return current > mine ? "done" : "idle"
    }

    function phaseLabel(id) {
        const state = phaseState(id)
        return state === "active" ? "Running"
             : state === "paused" ? "Needs input"
             : state === "done" ? "Done" : "Waiting"
    }

    function phaseColor(id) {
        const state = phaseState(id)
        return state === "active" ? Theme.accent
             : state === "paused" ? Theme.warning
             : state === "done" ? Theme.success : Theme.muted
    }

    background: Rectangle { color: Theme.background }

    Timer {
        id: flashTimer
        interval: 2200
        onTriggered: settingsView.flashText = ""
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Navigation ─────────────────────────────────────────────────────
        Rectangle {
            Layout.preferredWidth: 208
            Layout.minimumWidth: 176
            Layout.fillHeight: true
            color: Theme.darkBackground

            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: Theme.rgba(Theme.border, 0.3)
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: Theme.spaceMd
                anchors.bottomMargin: Theme.spaceMd
                spacing: 0

                SectionLabel {
                    title: "Preferences"
                    Layout.leftMargin: Theme.spaceMd
                    Layout.bottomMargin: Theme.spaceXs
                }

                Repeater {
                    model: settingsView.sections.filter(s => s.group === "prefs")
                    delegate: SettingsNavItem {
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.label
                        iconName: modelData.icon
                        selected: settingsView.section === modelData.key
                        onClicked: settingsView.showSection(modelData.key)
                    }
                }

                SectionLabel {
                    title: "Tools"
                    Layout.leftMargin: Theme.spaceMd
                    Layout.topMargin: Theme.spaceLg
                    Layout.bottomMargin: Theme.spaceXs
                }

                Repeater {
                    model: settingsView.sections.filter(s => s.group === "tools")
                    delegate: SettingsNavItem {
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.label
                        iconName: modelData.icon
                        selected: settingsView.section === modelData.key
                        onClicked: settingsView.showSection(modelData.key)
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        // ── Content ────────────────────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                color: Theme.darkBackground

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spaceXl
                    anchors.rightMargin: Theme.spaceXl
                    spacing: Theme.spaceLg

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        Label {
                            text: settingsView.sections[settingsView.sectionIndex(settingsView.section)].label
                            color: Theme.foreground
                            font.pixelSize: Theme.fontHeading
                            font.bold: true
                        }

                        Label {
                            text: settingsView.sections[settingsView.sectionIndex(settingsView.section)].subtitle
                            color: Theme.muted
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    Label {
                        text: settingsView.flashText.length > 0
                              ? settingsView.flashText : App.config.configPath
                        color: settingsView.flashText.length > 0 ? Theme.success : Theme.muted
                        opacity: settingsView.flashText.length > 0 ? 1 : 0.55
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideMiddle
                        visible: settingsView.width > 860
                        Layout.maximumWidth: 320
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.rgba(Theme.border, 0.3)
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: settingsView.sectionIndex(settingsView.section)

                // ── Library ────────────────────────────────────────────────
                ScrollView {
                    id: libraryPage
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        x: Theme.spaceXl
                        width: Math.min(820, libraryPage.availableWidth - Theme.spaceXl * 2)
                        spacing: Theme.spaceLg

                        Item { Layout.preferredHeight: Theme.spaceXs }

                        SettingsGroup {
                            title: "Locations"

                            SettingsRow {
                                title: "Music folders"
                                description: "Scanned recursively. Separate several with commas, e.g. ~/Music, /mnt/media."
                                wideControl: true

                                TextField {
                                    id: libraryPathsField
                                    Layout.fillWidth: true
                                    text: App.config.libraryPaths.join(", ")
                                    onEditingFinished: {
                                        App.applyLibraryPaths(text.trim())
                                        settingsView.flash("Music folders saved")
                                    }
                                }

                                Button {
                                    text: "Browse…"
                                    onClicked: libraryFolderDialog.open()
                                }
                            }

                            SettingsRow {
                                title: "Playlists folder"
                                description: App.config.playlistsDir

                                Label {
                                    text: App.playlists.playlistCount + " playlists"
                                    color: Theme.muted
                                    font.pixelSize: Theme.fontSmall
                                }
                            }
                        }

                        SettingsGroup {
                            title: "Scanning"

                            SettingsRow {
                                title: "Scan on launch"
                                description: "Look for new files every time Hyprplay starts."

                                Switch {
                                    id: scanOnLaunchField
                                    checked: App.config.scanOnLaunch
                                    onToggled: {
                                        App.applyScanning(checked, libraryWatchField.checked)
                                        settingsView.flash("Scanning saved")
                                    }
                                }
                            }

                            SettingsRow {
                                title: "Watch for changes"
                                description: "Pick up added and removed tracks while the app is open."

                                Switch {
                                    id: libraryWatchField
                                    checked: App.config.libraryWatchEnabled
                                    onToggled: {
                                        App.applyScanning(scanOnLaunchField.checked, checked)
                                        settingsView.flash("Scanning saved")
                                    }
                                }
                            }

                            SettingsRow {
                                title: "Rescan now"
                                description: App.scanStatus.length > 0
                                             ? App.scanStatus
                                             : (App.library.trackCount + " tracks indexed.")

                                PrimaryButton {
                                    text: App.library.scanning ? "Scanning…" : "Rescan"
                                    enabled: !App.library.scanning
                                    onClicked: App.rescanLibrary()
                                }
                            }
                        }

                        Item { Layout.preferredHeight: Theme.spaceXs }
                    }
                }

                // ── Audio ──────────────────────────────────────────────────
                ScrollView {
                    id: audioPage
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        x: Theme.spaceXl
                        width: Math.min(820, audioPage.availableWidth - Theme.spaceXl * 2)
                        spacing: Theme.spaceLg

                        Item { Layout.preferredHeight: Theme.spaceXs }

                        SettingsGroup {
                            title: "Output"

                            SettingsRow {
                                title: "Backend"
                                description: "The audio path the engine is currently using."

                                Label {
                                    text: App.playback.audioBackend.length > 0
                                          ? App.playback.audioBackend : "idle"
                                    color: App.playback.audioBackend.length > 0
                                           ? Theme.success : Theme.muted
                                    font.pixelSize: Theme.fontSmall
                                }
                            }

                            SettingsRow {
                                title: "Device"
                                visible: audioDevicePicker.count > 1
                                controlWidth: 280

                                ComboBox {
                                    id: audioDevicePicker
                                    Layout.fillWidth: true
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
                                    displayText: currentIndex < 0 && App.playback.audioDevice.length > 0
                                                 ? App.playback.audioDevice : currentText
                                    onActivated: (index) => {
                                        var entry = model[index]
                                        var name = entry ? entry.name : ""
                                        App.playback.setAudioDevice(name)
                                        App.config.setAudioDevice(name)
                                        settingsView.flash("Output device saved")
                                    }
                                }
                            }
                        }

                        SettingsGroup {
                            title: "DAC passthrough"

                            SettingsRow {
                                title: "Bit-perfect output"
                                description: "Locks volume at 100%, disables mute and ReplayGain, forces gapless."
                                value: App.playback.dacPassthrough
                                       ? "Active — use the DAC's own volume control."
                                       : "Off — PipeWire handles volume and per-app mixing."
                                valueColor: App.playback.dacPassthrough ? Theme.success : Theme.muted

                                Switch {
                                    id: dacSwitch
                                    checked: App.playback.dacPassthrough
                                    onToggled: App.setDacPassthrough(checked)
                                }
                            }

                            SettingsNote {
                                kind: "warning"
                                text: "Passthrough is requested but the output is PipeWire/Pulse, which is the same mixer path as normal mode. Only an alsa backend is bit-perfect."
                                visible: App.playback.dacPassthrough
                                         && (App.playback.audioBackend.indexOf("pipewire") === 0
                                             || App.playback.audioBackend.indexOf("pulse") === 0
                                             || (App.playback.audioBackend.length === 0
                                                 && App.playback.playing))
                            }
                        }

                        Item { Layout.preferredHeight: Theme.spaceXs }
                    }
                }

                // ── Lyrics ─────────────────────────────────────────────────
                ScrollView {
                    id: lyricsPage
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        x: Theme.spaceXl
                        width: Math.min(820, lyricsPage.availableWidth - Theme.spaceXl * 2)
                        spacing: Theme.spaceLg

                        Item { Layout.preferredHeight: Theme.spaceXs }

                        SettingsGroup {
                            title: "Location"

                            SettingsRow {
                                title: "Lyrics folder"
                                description: "Fetched lyrics land here. Sidecars next to tracks are still used."
                                wideControl: true

                                TextField {
                                    id: lyricsDirField
                                    Layout.fillWidth: true
                                    text: App.config.lyricsDir
                                    onEditingFinished: {
                                        App.applyLyricsDir(text.trim())
                                        settingsView.flash("Lyrics folder saved")
                                    }
                                }

                                Button {
                                    text: "Browse…"
                                    onClicked: lyricsFolderDialog.open()
                                }
                            }
                        }

                        SettingsGroup {
                            title: "Providers"

                            SettingsRow {
                                title: "LRCLIB"
                                description: "Synced lyrics. Always tried first."

                                Label {
                                    text: "Always on"
                                    color: Theme.success
                                    font.pixelSize: Theme.fontSmall
                                }
                            }

                            SettingsRow {
                                title: "NetEase"
                                description: "Synced fallback when LRCLIB has no match."

                                Switch {
                                    id: neteaseField
                                    checked: App.config.lyricsNeteaseEnabled
                                    onToggled: {
                                        App.applyLyricsProviders(checked, plainField.checked)
                                        settingsView.flash("Providers saved")
                                    }
                                }
                            }

                            SettingsRow {
                                title: "Plain text"
                                description: "Unsynced .txt from lyrics.ovh, as a last resort."

                                Switch {
                                    id: plainField
                                    checked: App.config.lyricsPlainEnabled
                                    onToggled: {
                                        App.applyLyricsProviders(neteaseField.checked, checked)
                                        settingsView.flash("Providers saved")
                                    }
                                }
                            }

                            SettingsNote {
                                text: "Fallbacks run one request at a time so providers are not hammered."
                            }
                        }

                        SettingsGroup {
                            title: "Fetch everything"

                            SettingsRow {
                                title: "Whole library"
                                description: "Queues every track. Tracks that already have a sidecar are skipped."
                                value: App.lyrics.busy
                                       ? ("Lyrics " + App.lyrics.progress + " / " + App.lyrics.total
                                          + (App.lyrics.status.length > 0 ? (" · " + App.lyrics.status) : ""))
                                       : App.lyrics.status
                                valueColor: App.lyrics.busy ? Theme.accent : Theme.muted

                                PrimaryButton {
                                    text: App.lyrics.busy ? "Fetching…" : "Fetch all"
                                    enabled: !App.lyrics.busy && !App.enrichment.active
                                             && App.library.trackCount > 0
                                    onClicked: App.fetchLyricsForLibrary()
                                }

                                Button {
                                    text: "Cancel"
                                    enabled: App.lyrics.busy && !App.enrichment.active
                                    onClicked: App.lyrics.cancel()
                                }
                            }

                            SettingsRow {
                                visible: App.lyrics.busy || App.lyrics.total > 0
                                wideControl: true

                                ProgressBar {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: Math.max(1, App.lyrics.total)
                                    value: App.lyrics.progress
                                }
                            }
                        }

                        Item { Layout.preferredHeight: Theme.spaceXs }
                    }
                }

                // ── Appearance ─────────────────────────────────────────────
                ScrollView {
                    id: appearancePage
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        x: Theme.spaceXl
                        width: Math.min(820, appearancePage.availableWidth - Theme.spaceXl * 2)
                        spacing: Theme.spaceLg

                        Item { Layout.preferredHeight: Theme.spaceXs }

                        SettingsGroup {
                            title: "Typography"

                            SettingsRow {
                                title: "Font family"
                                description: "A Nerd Font keeps the player's glyphs aligned, e.g. JetBrainsMono Nerd Font."
                                wideControl: true

                                TextField {
                                    id: fontField
                                    Layout.fillWidth: true
                                    text: App.config.uiFontFamily
                                    onEditingFinished: {
                                        App.applyUiFontSettings(text.trim(), fontSizeField.value)
                                        settingsView.flash("Font saved")
                                    }
                                }
                            }

                            SettingsRow {
                                title: "Font size"
                                description: "Applies to the whole interface, in pixels."

                                SpinBox {
                                    id: fontSizeField
                                    from: 9
                                    to: 24
                                    value: App.config.uiFontSize
                                    editable: true
                                    onValueModified: {
                                        App.applyUiFontSettings(fontField.text, value)
                                        settingsView.flash("Font saved")
                                    }
                                }
                            }
                        }

                        SettingsGroup {
                            title: "Interaction"

                            SettingsRow {
                                title: "WASD navigation"
                                description: "Mirror the arrow keys onto W, A, S and D."

                                Switch {
                                    id: wasdField
                                    checked: App.config.wasdNavigation
                                    onToggled: {
                                        App.applyInteraction(checked, tooltipsField.checked)
                                        settingsView.flash("Interaction saved")
                                    }
                                }
                            }

                            SettingsRow {
                                title: "Tooltips"
                                description: "Show hover hints on toolbar and player controls."

                                Switch {
                                    id: tooltipsField
                                    checked: App.config.tooltipsEnabled
                                    onToggled: {
                                        App.applyInteraction(wasdField.checked, checked)
                                        settingsView.flash("Interaction saved")
                                    }
                                }
                            }
                        }

                        SettingsGroup {
                            title: "Colors"

                            SettingsRow {
                                title: "Active theme"
                                description: "Hyprplay follows your Omarchy theme and reloads when you switch."

                                Label {
                                    text: Theme.themeName
                                    color: Theme.accent
                                    font.pixelSize: Theme.fontSmall
                                    font.weight: Font.DemiBold
                                }
                            }

                            SettingsRow {
                                title: "Palette"

                                Repeater {
                                    model: [Theme.accent, Theme.foreground, Theme.muted,
                                            Theme.success, Theme.warning, Theme.error, Theme.magenta]
                                    delegate: Rectangle {
                                        required property var modelData
                                        implicitWidth: 18
                                        implicitHeight: 18
                                        radius: 3
                                        color: modelData
                                        border.width: 1
                                        border.color: Theme.rgba(Theme.border, 0.45)
                                    }
                                }
                            }
                        }

                        Item { Layout.preferredHeight: Theme.spaceXs }
                    }
                }

                // ── Integrations ───────────────────────────────────────────
                ScrollView {
                    id: integrationsPage
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        x: Theme.spaceXl
                        width: Math.min(820, integrationsPage.availableWidth - Theme.spaceXl * 2)
                        spacing: Theme.spaceLg

                        Item { Layout.preferredHeight: Theme.spaceXs }

                        SettingsGroup {
                            title: "Discogs"

                            SettingsRow {
                                title: "Connection"
                                description: "Artist profiles, album notes, release details, and artwork."
                                value: App.discogs.status

                                Label {
                                    text: App.discogs.hasToken ? "Connected" : "Not configured"
                                    color: App.discogs.hasToken ? Theme.success : Theme.muted
                                    font.pixelSize: Theme.fontSmall
                                    font.weight: Font.DemiBold
                                }
                            }

                            SettingsRow {
                                title: "Personal access token"
                                description: "Press Enter to save. The field never shows a stored token."
                                wideControl: true

                                TextField {
                                    id: discogsTokenField
                                    Layout.fillWidth: true
                                    echoMode: TextInput.Password
                                    placeholderText: App.discogs.hasToken
                                                     ? "Token stored — type a new one to replace it"
                                                     : "Enter Discogs token"
                                    onEditingFinished: {
                                        if (text.trim().length === 0)
                                            return
                                        App.applyDiscogsToken(text.trim())
                                        text = ""
                                        settingsView.flash("Discogs token saved")
                                    }
                                }

                                Button {
                                    text: "Clear"
                                    enabled: App.discogs.hasToken
                                    onClicked: {
                                        discogsTokenField.text = ""
                                        App.applyDiscogsToken("")
                                    }
                                }
                            }

                            SettingsNote {
                                kind: "accent"
                                text: App.config.secretsStorageDescription()
                            }
                        }

                        Item { Layout.preferredHeight: Theme.spaceXs }
                    }
                }

                // ── Import ─────────────────────────────────────────────────
                Item {
                    id: importPage

                    ColumnLayout {
                        x: Theme.spaceXl
                        y: Theme.spaceLg
                        width: Math.min(820, importPage.width - Theme.spaceXl * 2)
                        height: importPage.height - Theme.spaceLg * 2
                        spacing: Theme.spaceLg

                        SettingsGroup {
                            title: "Inbox"

                            SettingsRow {
                                title: "Inbox folder"
                                description: "Albums here are copied into the library untouched — FLAC stays FLAC. e.g. ~/Music/inbox"
                                wideControl: true

                                TextField {
                                    id: inboxField
                                    Layout.fillWidth: true
                                    text: App.config.importInbox
                                    onEditingFinished: {
                                        App.applyImportInbox(text.trim())
                                        settingsView.flash("Inbox folder saved")
                                    }
                                }

                                Button {
                                    text: "Browse…"
                                    onClicked: inboxFolderDialog.open()
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 180
                            radius: Theme.radiusSm
                            color: Theme.rgba(Theme.surface, 0.5)
                            border.width: 1
                            border.color: Theme.rgba(Theme.border, 0.3)
                            clip: true

                            Loader {
                                id: embeddedImportLoader
                                anchors.fill: parent
                                anchors.margins: Theme.spaceMd
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
                                    inboxField.forceActiveFocus()
                                }
                            }
                        }
                    }
                }

                // ── Convert ────────────────────────────────────────────────
                Item {
                    id: convertPage

                    ColumnLayout {
                        x: Theme.spaceXl
                        y: Theme.spaceLg
                        width: Math.min(820, convertPage.width - Theme.spaceXl * 2)
                        height: convertPage.height - Theme.spaceLg * 2
                        spacing: Theme.spaceLg

                        SettingsGroup {
                            title: "Encoding"

                            SettingsRow {
                                title: "Opus bitrate"
                                description: "256 kbps is transparent for almost every listener."
                                controlWidth: 120

                                ComboBox {
                                    id: opusBitrateField
                                    Layout.fillWidth: true
                                    model: [128, 160, 192, 256, 320]
                                    currentIndex: {
                                        const i = model.indexOf(App.config.opusBitrateKbps)
                                        return i >= 0 ? i : model.indexOf(256)
                                    }
                                    onActivated: {
                                        App.applyConvertOptions(model[currentIndex],
                                                                deleteSourceField.checked)
                                        settingsView.flash("Convert options saved")
                                    }
                                }
                            }

                            SettingsRow {
                                title: "Delete the FLAC afterwards"
                                description: "Only once the Opus file has been written successfully."

                                Switch {
                                    id: deleteSourceField
                                    checked: App.config.convertDeleteSource
                                    onToggled: {
                                        App.applyConvertOptions(
                                            opusBitrateField.model[opusBitrateField.currentIndex],
                                            checked)
                                        settingsView.flash("Convert options saved")
                                    }
                                }
                            }

                            SettingsNote {
                                text: "Needs opusenc (opus-tools) or ffmpeg. Import never converts — this is the only place that re-encodes."
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 180
                            radius: Theme.radiusSm
                            color: Theme.rgba(Theme.surface, 0.5)
                            border.width: 1
                            border.color: Theme.rgba(Theme.border, 0.3)
                            clip: true

                            Loader {
                                id: embeddedConvertLoader
                                anchors.fill: parent
                                anchors.margins: Theme.spaceMd
                                source: "qrc:/views/ConvertView.qml"
                                onLoaded: if (item) item.embedded = true
                                onStatusChanged: {
                                    if (status === Loader.Error)
                                        console.error("Embedded ConvertView failed to load")
                                }
                            }
                        }
                    }
                }


                // ── Enrich library ─────────────────────────────────────────
                ScrollView {
                    id: enrichmentPage
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        x: Theme.spaceXl
                        width: Math.min(820, enrichmentPage.availableWidth - Theme.spaceXl * 2)
                        spacing: Theme.spaceLg

                        Item { Layout.preferredHeight: Theme.spaceXs }

                        SettingsGroup {
                            title: "Pipeline"

                            SettingsRow {
                                title: "1 · Artist info"
                                description: "Discogs profile and artist artwork."

                                Label {
                                    text: settingsView.phaseLabel("artist")
                                    color: settingsView.phaseColor("artist")
                                    font.pixelSize: Theme.fontCaption
                                }
                            }

                            SettingsRow {
                                title: "2 · Album info"
                                description: "Release notes and cover art."

                                Label {
                                    text: settingsView.phaseLabel("album")
                                    color: settingsView.phaseColor("album")
                                    font.pixelSize: Theme.fontCaption
                                }
                            }

                            SettingsRow {
                                title: "3 · Title fix"
                                description: "Match track titles against an official release."

                                Label {
                                    text: settingsView.phaseLabel("titleFix")
                                    color: settingsView.phaseColor("titleFix")
                                    font.pixelSize: Theme.fontCaption
                                }
                            }

                            SettingsRow {
                                title: "4 · Lyrics"
                                description: "Fetch lyrics for every track that is still missing them."

                                Label {
                                    text: settingsView.phaseLabel("lyrics")
                                    color: settingsView.phaseColor("lyrics")
                                    font.pixelSize: Theme.fontCaption
                                }
                            }

                            SettingsNote {
                                text: "Exact single matches apply on their own; anything ambiguous pauses and asks you. Browse, playback, and playlists all work without ever running this."
                            }
                        }

                        SettingsGroup {
                            title: "Requirements"

                            SettingsRow {
                                title: "Discogs token"
                                description: "Artist, album, and title-fix steps need it. Lyrics do not."

                                Label {
                                    text: App.discogs.hasToken ? "Connected" : "Not configured"
                                    color: App.discogs.hasToken ? Theme.success : Theme.warning
                                    font.pixelSize: Theme.fontSmall
                                    font.weight: Font.DemiBold
                                }

                                Button {
                                    text: "Set up"
                                    visible: !App.discogs.hasToken
                                    onClicked: {
                                        settingsView.showSection("integrations")
                                        discogsTokenField.forceActiveFocus()
                                    }
                                }
                            }

                            SettingsNote {
                                kind: "warning"
                                text: "Start stays disabled until a Discogs token is saved."
                                visible: !App.discogs.hasToken
                            }
                        }

                        SettingsGroup {
                            title: "Run"

                            SettingsRow {
                                title: "Full pass"
                                description: App.enrichment.currentArtist.length > 0
                                             ? (App.enrichment.currentAlbum.length > 0
                                                ? (App.enrichment.currentArtist + " — "
                                                   + App.enrichment.currentAlbum)
                                                : App.enrichment.currentArtist)
                                             : App.enrichment.status
                                value: "Auto " + App.enrichment.autoApplied
                                       + " · Resolved " + App.enrichment.resolved
                                       + " · Skipped " + App.enrichment.skipped
                                       + " · Failed " + App.enrichment.failed
                                       + (App.enrichment.total > 0
                                          ? (" · " + App.enrichment.progress + "/"
                                             + App.enrichment.total)
                                          : "")

                                PrimaryButton {
                                    text: App.enrichment.active ? "Running…" : "Start"
                                    enabled: !App.enrichment.active && App.discogs.hasToken
                                    onClicked: App.enrichment.start()
                                }

                                Button {
                                    text: "Cancel"
                                    enabled: App.enrichment.active
                                    onClicked: App.enrichment.cancel()
                                }
                            }

                            SettingsRow {
                                visible: App.enrichment.active || App.enrichment.phase === "done"
                                wideControl: true

                                ProgressBar {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: Math.max(1, App.enrichment.total)
                                    value: App.enrichment.progress
                                }
                            }
                        }

                        Item { Layout.preferredHeight: Theme.spaceXs }
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
            App.applyLibraryPaths(libraryPathsField.text)
            settingsView.flash("Music folders saved")
        }
    }

    FolderDialog {
        id: inboxFolderDialog
        title: "Choose import inbox folder"
        currentFolder: App.config.expandPath(App.config.importInbox.length > 0
                                             ? App.config.importInbox : "~/Music")
        onAccepted: {
            inboxField.text = selectedFolder.toString().replace("file://", "")
            App.applyImportInbox(inboxField.text)
            settingsView.flash("Inbox folder saved")
        }
    }

    FolderDialog {
        id: lyricsFolderDialog
        title: "Choose lyrics folder"
        currentFolder: App.config.expandPath(App.config.lyricsDir.length > 0
                                             ? App.config.lyricsDir : "~/Music")
        onAccepted: {
            lyricsDirField.text = selectedFolder.toString().replace("file://", "")
            App.applyLyricsDir(lyricsDirField.text)
            settingsView.flash("Lyrics folder saved")
        }
    }
}
