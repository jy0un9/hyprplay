import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: settingsView
    padding: 16

    background: Rectangle { color: Theme.background }

    ScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            width: settingsView.width - 32
            spacing: 16

            Label {
                text: "Settings"
                font.pixelSize: 22
                font.bold: true
                color: Theme.foreground
            }

            GroupBox {
                title: "Library"
                Layout.fillWidth: true

                GridLayout {
                    anchors.fill: parent
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 8

                    Label { text: "Import inbox"; color: Theme.foreground }
                    TextField {
                        id: inboxField
                        Layout.fillWidth: true
                        text: App.config.importInbox
                        placeholderText: "~/Music/inbox"
                    }

                    Label { text: "Lyrics dir"; color: Theme.foreground }
                    Label {
                        text: App.config.lyricsDir
                        opacity: 0.75
                        color: Theme.foreground
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }
            }

            GroupBox {
                title: "Beets"
                Layout.fillWidth: true

                GridLayout {
                    anchors.fill: parent
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 8

                    Label { text: "Binary"; color: Theme.foreground }
                    TextField {
                        id: beetsBinaryField
                        Layout.fillWidth: true
                        text: App.config.beetsBinary
                    }

                    Label { text: "No-move mode"; color: Theme.foreground }
                    CheckBox {
                        id: beetsNomoveField
                        checked: App.config.beetsNomove
                        text: "Use --nomove / --nocopy on import"
                    }

                    Label { text: "Status"; color: Theme.foreground }
                    Label {
                        text: App.beets.available ? ("beet OK — " + App.beets.version) : "beet not found"
                        opacity: 0.75
                        color: Theme.foreground
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                }
            }

            GroupBox {
                title: "Appearance"
                Layout.fillWidth: true

                GridLayout {
                    anchors.fill: parent
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 8

                    Label { text: "Font"; color: Theme.foreground }
                    TextField {
                        id: fontField
                        Layout.fillWidth: true
                        text: App.config.uiFontFamily
                        placeholderText: "JetBrainsMono Nerd Font"
                    }
                }
            }

            GroupBox {
                title: "Discogs"
                Layout.fillWidth: true

                GridLayout {
                    anchors.fill: parent
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 8

                    Label { text: "Token"; color: Theme.foreground }
                    TextField {
                        id: discogsTokenField
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        placeholderText: App.discogs.hasToken ? "Configured (leave blank to keep)"
                                                          : "Personal access token"
                    }

                    Label { text: "Status"; color: Theme.foreground }
                    Label {
                        text: App.discogs.status.length > 0 ? App.discogs.status
                            : (App.discogs.hasToken ? "Token configured" : "No token")
                        opacity: 0.75
                        color: Theme.foreground
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: "Save"
                    highlighted: true
                    onClicked: {
                        App.saveSettings(inboxField.text.trim(),
                                         beetsBinaryField.text.trim(),
                                         beetsNomoveField.checked,
                                         discogsTokenField.text.trim(),
                                         fontField.text.trim())
                    }
                }

                Label {
                    text: App.config.configPath
                    opacity: 0.45
                    font.pixelSize: 11
                    color: Theme.foreground
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                }
            }
        }
    }
}
