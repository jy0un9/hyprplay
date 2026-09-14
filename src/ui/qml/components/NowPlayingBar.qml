import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import components 1.0

Pane {
    id: bar
    padding: Theme.spaceMd

    property var playback: App.playback
    property var media: App.nowPlaying

    background: Rectangle { color: Theme.darkBackground }

    component TransportButton: ToolButton {
        id: transport
        required property string iconName
        property int glyphSize: 16
        property real glyphOpacity: 1
        property color bgColor: "transparent"
        property color iconColor: Theme.foreground
        property string accessibleLabel: ""

        Accessible.name: accessibleLabel
        Accessible.role: Accessible.Button

        padding: 2
        implicitWidth: glyphSize + 6
        implicitHeight: glyphSize + 6
        display: AbstractButton.IconOnly

        background: Rectangle {
            radius: Theme.radiusSm
            color: transport.bgColor
            Behavior on color {
                ColorAnimation { duration: 120 }
            }
        }

        contentItem: Image {
            width: transport.glyphSize
            height: transport.glyphSize
            anchors.centerIn: parent
            source: transport.iconName.length > 0
                    ? Theme.iconUrl(transport.iconName, transport.glyphSize, transport.iconColor)
                    : ""
            fillMode: Image.PreserveAspectFit
            opacity: transport.pressed ? transport.glyphOpacity * 0.55 : transport.glyphOpacity
            cache: true
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spaceSm

        RowLayout {
            Layout.fillWidth: true
            Layout.minimumHeight: 60
            Layout.preferredHeight: 60
            Layout.maximumHeight: 60
            spacing: Theme.spaceSm

            Rectangle {
                Layout.preferredWidth: 56
                Layout.preferredHeight: 56
                Layout.alignment: Qt.AlignVCenter
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
                    source: Theme.iconUrl("audio-x-generic-symbolic", 18, Theme.foreground)
                    fillMode: Image.PreserveAspectFit
                    opacity: 0.35
                    visible: media.albumArtUrl.length === 0
                    cache: true
                }
            }

            ColumnLayout {
                Layout.preferredWidth: 180
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 1

                Label {
                    id: nowPlayingTitleLabel
                    text: playback.title.length > 0 ? playback.title : "Not playing"
                    font.bold: true
                    color: playback.playing && !playback.paused
                           ? Theme.accent : Theme.foreground
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
                    color: playback.error.length > 0 ? Theme.error : Theme.foreground
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
                    color: Theme.foreground
                    font.pixelSize: Theme.fontCaption
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            Label {
                Layout.alignment: Qt.AlignVCenter
                text: formatTime(playback.position) + " / " + formatTime(playback.duration)
                font.family: "monospace"
                font.pixelSize: Theme.fontCaption
                opacity: 0.75
                color: Theme.foreground
            }

            TransportButton {
                Layout.alignment: Qt.AlignVCenter
                iconName: "media-skip-backward-symbolic"
                accessibleLabel: "Previous track"
                onClicked: playback.previous()
            }
            TransportButton {
                Layout.alignment: Qt.AlignVCenter
                highlighted: true
                bgColor: Theme.surface
                glyphSize: 20
                iconName: playback.playing && !playback.paused
                           ? "media-playback-pause-symbolic"
                           : "media-playback-start-symbolic"
                accessibleLabel: playback.playing && !playback.paused ? "Pause" : "Play"
                onClicked: playback.togglePlayPause()
            }
            TransportButton {
                Layout.alignment: Qt.AlignVCenter
                iconName: "media-skip-forward-symbolic"
                accessibleLabel: "Next track"
                onClicked: playback.next()
            }
            TransportButton {
                Layout.alignment: Qt.AlignVCenter
                iconName: repeatIconName()
                glyphOpacity: playback.repeatMode === 0 ? 0.55 : 1
                accessibleLabel: "Repeat mode"
                onClicked: playback.setRepeatMode((playback.repeatMode + 1) % 3)
            }
            TransportButton {
                Layout.alignment: Qt.AlignVCenter
                iconName: playback.shuffle
                           ? "media-playlist-shuffle-symbolic"
                           : "media-playlist-consecutive-symbolic"
                glyphOpacity: playback.shuffle ? 1 : 0.55
                accessibleLabel: "Shuffle"
                onClicked: playback.setShuffle(!playback.shuffle)
            }

            TransportButton {
                Layout.alignment: Qt.AlignVCenter
                iconName: volumeIconName()
                glyphOpacity: playback.dacPassthrough ? 0.4 : (playback.muted ? 1 : 0.7)
                accessibleLabel: playback.muted ? "Unmute" : "Mute"
                enabled: !playback.dacPassthrough
                ToolTip.visible: App.config.tooltipsEnabled && hovered && playback.dacPassthrough
                ToolTip.text: "Mute disabled in DAC passthrough — pause or use the DAC knob"
                onClicked: playback.setMuted(!playback.muted)
            }

            Slider {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 120
                from: 0
                to: 100
                value: playback.volume
                enabled: !playback.muted && !playback.dacPassthrough
                opacity: playback.dacPassthrough ? 0.4 : 1
                Accessible.name: "Volume"
                Accessible.role: Accessible.Slider
                ToolTip.visible: App.config.tooltipsEnabled && hovered && playback.dacPassthrough
                ToolTip.text: "Locked at 100% in DAC passthrough — use the DAC knob"
                onMoved: playback.setVolume(value)
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

    function formatTime(secs) {
        if (!secs || secs < 0) return "0:00"
        var total = Math.floor(secs)
        var min = Math.floor(total / 60)
        var sec = total % 60
        return min + ":" + (sec < 10 ? "0" : "") + sec
    }

    function repeatIconName() {
        switch (playback.repeatMode) {
        case 1: return "media-playlist-repeat-song-symbolic"
        case 2: return "media-playlist-repeat-symbolic"
        default: return "media-playlist-repeat-symbolic"
        }
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
