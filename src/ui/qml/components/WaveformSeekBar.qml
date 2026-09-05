import QtQuick
import QtQuick.Controls

Control {
    id: root
    anchors.fill: parent

    property var playback: App.playback
    property var media: App.nowPlaying
    property bool dragging: false
    property real hoverX: -1
    property real dragRatio: -1

    readonly property real displayRatio: root.dragRatio >= 0 ? root.dragRatio : root.progressRatio
    readonly property real hoverRatio: {
        if (hoverX < 0 || canvas.width <= 0)
            return -1
        return Math.max(0, Math.min(1, hoverX / canvas.width))
    }

    readonly property real progressRatio: {
        if (!playback || playback.duration <= 0)
            return 0
        return Math.max(0, Math.min(1, playback.position / playback.duration))
    }

    readonly property bool isPaused: playback ? playback.paused : false

    readonly property color barPlayed: isPaused ? Theme.muted : Theme.accent
    readonly property color barUnplayed: isPaused ? Theme.rgba(Theme.muted, 0.45) : Theme.rgba(Theme.accent, 0.28)
    readonly property color trackBg: isPaused ? Theme.rgba(Theme.selection, 0.5) : Theme.rgba(Theme.selection, 0.65)

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
                const playedX = width * root.displayRatio
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

                if (!root.dragging && root.hoverRatio >= 0) {
                    const hx = root.hoverRatio * width
                    ctx.fillStyle = Theme.foreground
                    ctx.globalAlpha = 0.6
                    ctx.fillRect(hx - 0.5, 0, 1, height)
                    ctx.globalAlpha = 1
                }
            }
        }

        MouseArea {
            id: seekMouse
            anchors.fill: parent
            hoverEnabled: true
            onPositionChanged: (mouse) => {
                root.hoverX = mouse.x
                if (dragging)
                    seekAt(mouse.x)
            }
            onExited: root.hoverX = -1
            onPressed: (mouse) => {
                dragging = true
                root.dragRatio = root.clampRatio(mouse.x / canvas.width)
                seekAt(mouse.x)
            }
            onReleased: (mouse) => {
                if (dragging) {
                    seekAt(mouse.x)
                    dragging = false
                    root.dragRatio = -1
                }
            }
        }

        Label {
            visible: seekMouse.containsMouse && !root.dragging && root.hoverRatio >= 0
                     && playback && playback.duration > 0
            text: root.formatTime(root.hoverRatio * (playback ? playback.duration : 0))
            color: Theme.foreground
            font.pixelSize: Theme.fontCaption
            background: null
            x: Math.max(0, Math.min(parent.width - width, root.hoverX - width / 2))
            y: -height - 2
        }
    }

    function clampRatio(ratio) {
        return Math.max(0, Math.min(1, ratio))
    }

    function seekAt(x) {
        if (!playback || playback.duration <= 0 || canvas.width <= 0)
            return
        root.dragRatio = root.clampRatio(x / canvas.width)
        playback.seek(root.dragRatio * playback.duration)
        canvas.requestPaint()
    }

    function formatTime(secs) {
        if (!secs || secs < 0) return "0:00"
        var total = Math.floor(secs)
        var min = Math.floor(total / 60)
        var sec = total % 60
        return min + ":" + (sec < 10 ? "0" : "") + sec
    }

    onHoverXChanged: canvas.requestPaint()
    onDragRatioChanged: canvas.requestPaint()
    onDisplayRatioChanged: canvas.requestPaint()

    Connections {
        target: playback
        function onPositionChanged() {
            if (!root.dragging)
                canvas.requestPaint()
        }
        function onPlaybackChanged() { canvas.requestPaint() }
    }

    onIsPausedChanged: canvas.requestPaint()

    Connections {
        target: media
        function onMediaChanged() { canvas.requestPaint() }
        function onLoadingChanged() { canvas.requestPaint() }
    }

    Component.onCompleted: canvas.requestPaint()
}
