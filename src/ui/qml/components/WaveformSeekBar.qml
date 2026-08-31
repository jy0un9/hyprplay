import QtQuick
import QtQuick.Controls

Control {
    id: root
    anchors.fill: parent

    property var playback: App.playback
    property var media: App.nowPlaying
    property bool dragging: false

    readonly property real progressRatio: {
        if (!playback || playback.duration <= 0)
            return 0
        return Math.max(0, Math.min(1, playback.position / playback.duration))
    }

    readonly property color barPlayed: Theme.accent
    readonly property color barUnplayed: Theme.rgba(Theme.accent, 0.28)
    readonly property color trackBg: Theme.rgba(Theme.selection, 0.65)

    background: Rectangle {
        radius: Theme.radiusSm
        color: root.trackBg
        border.color: Theme.rgba(Theme.border, 0.35)
        border.width: 1
    }

    contentItem: Item {
        anchors.fill: parent
        anchors.margins: 3

        Canvas {
            id: canvas
            anchors.fill: parent

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()

            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                if (width < 2 || height < 2)
                    return

                const mid = height / 2
                const peaks = media ? media.waveformPeaks : []
                const count = peaks.length
                const playedX = width * root.progressRatio
                const maxAmp = height * 0.5 - 1

                if (count > 0) {
                    const columns = Math.min(count, Math.floor(width))
                    const colWidth = width / columns

                    for (let col = 0; col < columns; col++) {
                        const i = Math.min(count - 1, Math.floor(col * count / columns))
                        const amp = Math.max(1.5, peaks[i] * maxAmp)
                        const x = col * colWidth
                        const barW = Math.max(1, colWidth - 0.2)
                        const xCenter = x + barW / 2

                        ctx.fillStyle = xCenter <= playedX ? root.barPlayed : root.barUnplayed
                        ctx.fillRect(x, mid - amp, barW, amp * 2)
                    }
                } else {
                    ctx.fillStyle = root.barUnplayed
                    ctx.fillRect(0, mid - 1.5, width, 3)
                    ctx.fillStyle = root.barPlayed
                    ctx.fillRect(0, mid - 1.5, playedX, 3)
                }

                ctx.fillStyle = root.barPlayed
                ctx.fillRect(Math.max(0, playedX - 1), 0, 2, height)
            }
        }

        MouseArea {
            anchors.fill: parent
            onPressed: (mouse) => {
                dragging = true
                seekAt(mouse.x)
            }
            onPositionChanged: (mouse) => {
                if (dragging)
                    seekAt(mouse.x)
            }
            onReleased: (mouse) => {
                if (dragging) {
                    seekAt(mouse.x)
                    dragging = false
                }
            }
        }
    }

    function seekAt(x) {
        if (!playback || playback.duration <= 0 || canvas.width <= 0)
            return
        const ratio = Math.max(0, Math.min(1, x / canvas.width))
        playback.seek(ratio * playback.duration)
        canvas.requestPaint()
    }

    Connections {
        target: playback
        function onPositionChanged() {
            if (!root.dragging)
                canvas.requestPaint()
        }
        function onPlaybackChanged() { canvas.requestPaint() }
    }

    Connections {
        target: media
        function onMediaChanged() { canvas.requestPaint() }
        function onLoadingChanged() { canvas.requestPaint() }
    }

    Component.onCompleted: canvas.requestPaint()
}
