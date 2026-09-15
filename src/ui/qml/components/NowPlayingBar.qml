import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import components 1.0

Pane {
    id: bar
    padding: Theme.spaceMd

    property var playback: App.playback
    property var media: App.nowPlaying

    background: Rectangle { color: Theme.chrome }

    component TransportButton: ToolButton {
        id: transport
        required property string iconName
        property int glyphSize: 16
        property real glyphOpacity: 1
        property bool playButton: false
        property int glyphNudgeX: 0
        property string iconColor: Theme.chromeIcon
        property string accessibleLabel: ""

        Accessible.name: accessibleLabel
        Accessible.role: Accessible.Button
        Layout.alignment: Qt.AlignVCenter

        padding: 0
        implicitWidth: playButton ? 36 : 32
        implicitHeight: playButton ? 36 : 32
        display: AbstractButton.IconOnly
        hoverEnabled: true
        scale: pressed ? 0.96 : 1

        Behavior on scale {
            NumberAnimation { duration: 80; easing.type: Easing.OutCubic }
        }

        background: Rectangle {
            radius: height / 2
            color: {
                if (transport.playButton)
                    return Theme.chromeIcon
                return transport.hovered ? Theme.rgba(Theme.chromeIcon, 0.12) : "transparent"
            }
        }

        contentItem: Item {
            implicitWidth: transport.implicitWidth
            implicitHeight: transport.implicitHeight
            Image {
                width: transport.glyphSize
                height: transport.glyphSize
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: transport.glyphNudgeX
                source: transport.iconName.length > 0
                        ? Theme.iconUrl(transport.iconName, transport.glyphSize, transport.iconColor)
                        : ""
                fillMode: Image.PreserveAspectFit
                opacity: transport.pressed ? transport.glyphOpacity * 0.65 : transport.glyphOpacity
                cache: true
            }
        }
    }

    component ClusterDivider: Rectangle {
        Layout.alignment: Qt.AlignVCenter
        Layout.preferredWidth: 1
        Layout.preferredHeight: 22
        Layout.leftMargin: Theme.spaceXs
        Layout.rightMargin: Theme.spaceXs
        color: Theme.rgba(Theme.border, 0.35)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spaceSm

        Item {
            Layout.fillWidth: true
            Layout.minimumHeight: 60
            Layout.preferredHeight: 60
            Layout.maximumHeight: 60

            RowLayout {
                id: leftCluster
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(80, transportCluster.x - Theme.spaceMd)
                spacing: Theme.spaceSm
                clip: true

                Rectangle {
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 56
                    Layout.alignment: Qt.AlignVCenter
                    Layout.minimumWidth: 56
                    radius: Theme.radiusSm
                    color: Theme.rgba(Theme.selection, 0.8)
                    clip: true
                    visible: playback.currentPath.length > 0

                    Image {
                        anchors.fill: parent
                        source: media.albumArtUrl
                        fillMode: Image.PreserveAspectCrop
                        visible: media.albumArtUrl.length > 0
                    }

                    Image {
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        source: Theme.iconUrl("audio-x-generic-symbolic", 18, Theme.chromeIcon)
                        fillMode: Image.PreserveAspectFit
                        opacity: 0.35
                        visible: media.albumArtUrl.length === 0
                        cache: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    Layout.minimumWidth: 0
                    spacing: 1

                    Label {
                        id: nowPlayingTitleLabel
                        text: playback.title.length > 0 ? playback.title : "Not playing"
                        font.bold: true
                        color: playback.playing && !playback.paused
                               ? Theme.accent : Theme.chromeIcon
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        HoverHandler { id: nowPlayingTitleHover }
                        ElisionPopup {
                            visible: nowPlayingTitleHover.hovered && nowPlayingTitleLabel.truncated
                            text: nowPlayingTitleLabel.text
                        }
                    }

                    Label {
                        id: nowPlayingArtistLabel
                        text: playback.artist.length > 0
                              ? playback.artist + " — " + playback.album
                              : (playback.error.length > 0 ? playback.error : "")
                        opacity: 0.65
                        color: playback.error.length > 0 ? Theme.error : Theme.chromeIcon
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        font.pixelSize: Theme.fontSmall
                        HoverHandler { id: nowPlayingArtistHover }
                        ElisionPopup {
                            visible: nowPlayingArtistHover.hovered && nowPlayingArtistLabel.truncated
                            text: nowPlayingArtistLabel.text
                        }
                    }

                    Label {
                        text: media.qualityLabel
                        visible: media.qualityLabel.length > 0
                        opacity: 0.55
                        color: Theme.chromeIcon
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }

            RowLayout {
                id: transportCluster
                z: 1
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                spacing: 0

                TransportButton {
                    glyphSize: 16
                    iconName: playback.repeatMode === 1
                              ? "media-playlist-repeat-song-symbolic"
                              : "media-playlist-repeat-symbolic"
                    glyphOpacity: playback.repeatMode === 0 ? 0.55 : 1
                    accessibleLabel: "Repeat mode"
                    onClicked: playback.setRepeatMode((playback.repeatMode + 1) % 3)
                }

                Item { Layout.preferredWidth: Theme.spaceLg }

                TransportButton {
                    iconName: "media-skip-backward-symbolic"
                    glyphSize: 18
                    accessibleLabel: "Previous track"
                    onClicked: playback.previous()
                }
                Item { Layout.preferredWidth: Theme.spaceSm }
                TransportButton {
                    playButton: true
                    glyphSize: 16
                    glyphNudgeX: playback.playing && !playback.paused ? 0 : 1
                    iconName: playback.playing && !playback.paused
                               ? "media-playback-pause-symbolic"
                               : "media-playback-start-symbolic"
                    iconColor: Theme.chrome
                    accessibleLabel: playback.playing && !playback.paused ? "Pause" : "Play"
                    onClicked: playback.togglePlayPause()
                }
                Item { Layout.preferredWidth: Theme.spaceSm }
                TransportButton {
                    iconName: "media-skip-forward-symbolic"
                    glyphSize: 18
                    accessibleLabel: "Next track"
                    onClicked: playback.next()
                }

                Item { Layout.preferredWidth: Theme.spaceLg }

                TransportButton {
                    glyphSize: 16
                    iconName: "media-playlist-shuffle-symbolic"
                    glyphOpacity: playback.shuffle ? 1 : 0.55
                    accessibleLabel: "Shuffle"
                    onClicked: playback.setShuffle(!playback.shuffle)
                }
            }

            RowLayout {
                id: rightCluster
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.spaceSm

                Label {
                    Layout.alignment: Qt.AlignVCenter
                    text: formatTime(playback.position) + " / " + formatTime(playback.duration)
                    font.family: "monospace"
                    font.pixelSize: Theme.fontCaption
                    opacity: 0.55
                    color: Theme.chromeIcon
                }

                ClusterDivider {}

                RowLayout {
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 0

                    TransportButton {
                        iconName: volumeIconName()
                        glyphOpacity: playback.dacPassthrough ? 0.55 : (playback.muted ? 1 : 0.85)
                        accessibleLabel: playback.muted ? "Unmute" : "Mute"
                        enabled: !playback.dacPassthrough
                        ToolTip.visible: App.config.tooltipsEnabled && hovered && playback.dacPassthrough
                        ToolTip.text: "Mute disabled in DAC passthrough — pause or use the DAC knob"
                        onClicked: playback.setMuted(!playback.muted)
                    }
                    Slider {
                        Layout.preferredWidth: 92
                        from: 0
                        to: 100
                        value: playback.volume
                        enabled: !playback.muted && !playback.dacPassthrough
                        opacity: playback.dacPassthrough ? 0.4 : 1
                        Accessible.name: "Volume"
                        Accessible.role: Accessible.Slider
                        palette.highlight: Theme.accent
                        palette.mid: Theme.chromeIcon
                        palette.button: Theme.chromeIcon
                        ToolTip.visible: App.config.tooltipsEnabled && hovered && playback.dacPassthrough
                        ToolTip.text: "Locked at 100% in DAC passthrough — use the DAC knob"
                        onMoved: playback.setVolume(value)
                    }
                }

                TransportButton {
                    id: outputButton
                    iconName: "audio-card-symbolic"
                    iconColor: playback.bitPerfectActive
                               ? Theme.success
                               : (playback.dacPassthrough ? Theme.accent : Theme.chromeIcon)
                    glyphOpacity: playback.dacPassthrough || outputMenu.visible ? 1 : 0.75
                    accessibleLabel: "Output device"
                    ToolTip.visible: App.config.tooltipsEnabled && hovered && !outputMenu.visible
                    ToolTip.text: playback.dacPassthrough
                                  ? (playback.bitPerfectActive
                                     ? "Bit-perfect active — choose output"
                                     : "Bit-perfect on — choose output")
                                  : "Choose output device"
                    onClicked: {
                        if (outputMenu.visible) {
                            outputMenu.close()
                            return
                        }
                        outputMenu.popup(outputButton)
                        Qt.callLater(outputMenu.repositionAboveButton)
                    }
                }
            }
        }

        Loader {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            Layout.minimumHeight: 28
            Layout.maximumHeight: 48
            Layout.bottomMargin: Theme.spaceSm
            Layout.fillHeight: false
            source: "qrc:/components/WaveformSeekBar.qml"
        }
    }

    AdaptiveMenu {
        id: outputMenu
        minimumMenuWidth: 260
        maximumMenuWidth: 380

        property var devices: []

        onAboutToShow: refreshDevices()

        function refreshDevices() {
            var items = []
            if (!playback.dacPassthrough)
                items.push({ "name": "", "description": "System default" })
            var list = playback.audioDeviceList()
            for (var i = 0; i < list.length; ++i)
                items.push(list[i])
            devices = items
        }

        function selectDevice(name) {
            playback.setAudioDevice(name)
            App.config.setAudioDevice(name)
        }

        function applyBitPerfect(enable) {
            // Close first — setDacPassthrough rebuilds the device family and an
            // Instantiator refresh while the menu is open corrupts its items.
            close()
            Qt.callLater(function () {
                App.setDacPassthrough(enable)
            })
        }

        function repositionAboveButton() {
            if (!visible || !Overlay.overlay)
                return
            resizeToContents()
            var overlay = Overlay.overlay
            var p = outputButton.mapToItem(overlay, 0, 0)
            var menuW = width > 0 ? width : minimumMenuWidth
            var menuH = height > 0 ? height : 120
            var x = p.x + outputButton.width - menuW
            x = Math.max(Theme.spaceSm, Math.min(x, overlay.width - menuW - Theme.spaceSm))
            var y = p.y - menuH - Theme.spaceXs
            if (y < Theme.spaceSm)
                y = p.y + outputButton.height + Theme.spaceXs
            outputMenu.x = x
            outputMenu.y = y
        }

        Instantiator {
            model: outputMenu.devices
            active: outputMenu.visible || outputMenu.devices.length > 0
            delegate: ThemedMenuItem {
                required property var modelData
                text: modelData.description || modelData.name || "System default"
                checkable: true
                autoExclusive: true
                checked: (modelData.name || "") === playback.audioDevice
                Accessible.name: text
                onTriggered: outputMenu.selectDevice(modelData.name || "")
            }
            onObjectAdded: (index, object) => outputMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => outputMenu.removeItem(object)
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: Theme.rgba(Theme.foreground, 0.12)
            }
        }

        ThemedMenuItem {
            text: "Bit-perfect"
            checkable: true
            checked: playback.dacPassthrough
            Accessible.name: "Bit-perfect output"
            onTriggered: outputMenu.applyBitPerfect(checked)
        }

        ThemedMenuItem {
            visible: playback.dacPassthrough && playback.bitPerfectStatus.length > 0
            enabled: false
            opacity: 0.65
            text: playback.bitPerfectStatus
            Accessible.name: playback.bitPerfectStatus
        }

        ThemedMenuItem {
            visible: playback.dacPassthrough && !playback.bitPerfectActive
            text: "Retry exclusive"
            Accessible.name: text
            onTriggered: {
                outputMenu.close()
                Qt.callLater(function () {
                    if (playback.retryExclusiveOutput() && playback.audioDevice.length > 0)
                        App.config.setAudioDevice(playback.audioDevice)
                })
            }
        }
    }

    function formatTime(secs) {
        if (!secs || secs < 0) return "0:00"
        var total = Math.floor(secs)
        var min = Math.floor(total / 60)
        var sec = total % 60
        return min + ":" + (sec < 10 ? "0" : "") + sec
    }

    function volumeIconName() {
        if (playback.muted || playback.volume === 0)
            return "audio-volume-muted-symbolic"
        if (playback.volume < 40)
            return "audio-volume-low-symbolic"
        if (playback.volume < 75)
            return "audio-volume-medium-symbolic"
        return "audio-volume-high-symbolic"
    }
}
