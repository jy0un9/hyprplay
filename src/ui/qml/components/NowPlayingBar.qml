import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: bar
    padding: 10

    property var playback: App.playback
    property var media: App.nowPlaying

    background: Rectangle { color: Theme.darkBackground }

    component TransportButton: ToolButton {
        id: transport
        required property string iconName
        property int glyphSize: 16
        property real glyphOpacity: 1

        padding: 2
        implicitWidth: glyphSize + 6
        implicitHeight: glyphSize + 6
        display: AbstractButton.IconOnly

        contentItem: Image {
            width: transport.glyphSize
            height: transport.glyphSize
            anchors.centerIn: parent
            source: transport.iconName.length > 0
                    ? "image://themeicon/" + transport.iconName + "?" + transport.glyphSize
                    : ""
            fillMode: Image.PreserveAspectFit
            opacity: transport.glyphOpacity
            cache: true
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Layout.minimumHeight: 52
            Layout.preferredHeight: 52
            Layout.maximumHeight: 52
            spacing: 8

            Rectangle {
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
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
                    source: "image://themeicon/audio-x-generic-symbolic?18"
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
                    text: playback.title.length > 0 ? playback.title : "Not playing"
                    font.bold: true
                    color: Theme.foreground
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Label {
                    text: playback.artist.length > 0
                          ? playback.artist + " — " + playback.album
                          : (playback.error.length > 0 ? playback.error : "")
                    opacity: 0.65
                    color: playback.error.length > 0 ? Theme.error : Theme.foreground
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    font.pixelSize: 12
                }

                Label {
                    text: media.qualityLabel
                    visible: media.qualityLabel.length > 0
                    opacity: 0.55
                    color: Theme.foreground
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            Label {
                Layout.alignment: Qt.AlignVCenter
                text: formatTime(playback.position) + " / " + formatTime(playback.duration)
                font.family: "monospace"
                font.pixelSize: 11
                opacity: 0.75
                color: Theme.foreground
            }

            TransportButton {
                Layout.alignment: Qt.AlignVCenter
                iconName: "media-skip-backward-symbolic"
                onClicked: playback.previous()
            }
            TransportButton {
                Layout.alignment: Qt.AlignVCenter
                highlighted: true
                glyphSize: 18
                iconName: playback.playing && !playback.paused
                           ? "media-playback-pause-symbolic"
                           : "media-playback-start-symbolic"
                onClicked: playback.togglePlayPause()
            }
            TransportButton {
                Layout.alignment: Qt.AlignVCenter
                iconName: "media-skip-forward-symbolic"
                onClicked: playback.next()
            }
            TransportButton {
                Layout.alignment: Qt.AlignVCenter
                iconName: repeatIconName()
                glyphOpacity: playback.repeatMode === 0 ? 0.55 : 1
                onClicked: playback.setRepeatMode((playback.repeatMode + 1) % 3)
            }
            TransportButton {
                Layout.alignment: Qt.AlignVCenter
                iconName: playback.shuffle
                           ? "media-playlist-shuffle-symbolic"
                           : "media-playlist-consecutive-symbolic"
                glyphOpacity: playback.shuffle ? 1 : 0.55
                onClicked: playback.setShuffle(!playback.shuffle)
            }

            Slider {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 80
                from: 0
                to: 100
                value: playback.volume
                onMoved: playback.setVolume(value)
            }
        }

        Loader {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            Layout.minimumHeight: 28
            Layout.maximumHeight: 48
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
}
