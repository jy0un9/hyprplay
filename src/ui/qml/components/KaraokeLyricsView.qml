import QtQuick
import QtQuick.Controls

Item {
    id: root
    anchors.fill: parent

    property var media: App.nowPlaying
    property var playback: App.playback

    readonly property int activeIndex: media ? media.currentLyricIndex : -1

    property real lastPosition: 0
    property bool jumped: false

    Connections {
        target: playback
        function onPositionChanged() {
            const pos = playback ? playback.position : 0
            root.jumped = Math.abs(pos - root.lastPosition) > 1.25
            root.lastPosition = pos
        }
    }

    Timer {
        interval: 250
        running: root.jumped
        onTriggered: root.jumped = false
    }

    Label {
        anchors.centerIn: parent
        width: parent.width - 24
        text: "No lyrics"
        opacity: 0.4
        font.pixelSize: 14
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        visible: !media || media.lyricLines.length === 0
    }

    ListView {
        id: lyricsList
        anchors.fill: parent
        visible: media && media.lyricLines.length > 0
        clip: true
        interactive: true
        boundsBehavior: Flickable.StopAtBounds
        model: media ? media.lyricLines : []
        spacing: 8
        highlightRangeMode: ListView.StrictlyEnforceRange
        preferredHighlightBegin: height * 0.42
        preferredHighlightEnd: height * 0.58
        highlightMoveDuration: root.jumped ? 0 : 120
        highlightFollowsCurrentItem: true

        delegate: Item {
            id: row
            width: lyricsList.width
            height: Math.max(lyricText.implicitHeight + 10, 30)

            readonly property bool isActive: root.activeIndex >= 0 && index === root.activeIndex
            readonly property int distance: root.activeIndex < 0 ? 99 : Math.abs(index - root.activeIndex)

            Text {
                id: lyricText
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 16
                text: modelData
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                color: row.isActive ? Theme.accent : Theme.foreground
                font.pixelSize: row.isActive ? 18 : Math.max(12, 14 - Math.min(row.distance, 2))
                font.bold: row.isActive
                opacity: {
                    if (row.isActive)
                        return 1.0
                    if (root.activeIndex < 0)
                        return 0.45
                    if (row.distance === 1)
                        return 0.5
                    if (row.distance === 2)
                        return 0.35
                    return 0.2
                }
            }
        }

        onCurrentIndexChanged: {
            if (currentIndex >= 0)
                positionViewAtIndex(currentIndex, ListView.Center)
        }
    }

    onActiveIndexChanged: {
        if (activeIndex >= 0) {
            lyricsList.currentIndex = activeIndex
            lyricsList.positionViewAtIndex(activeIndex, ListView.Center)
        }
    }

    Connections {
        target: media
        function onCurrentLyricChanged() {
            if (root.activeIndex >= 0) {
                lyricsList.currentIndex = root.activeIndex
                lyricsList.positionViewAtIndex(root.activeIndex, ListView.Center)
            }
        }
        function onMediaChanged() {
            if (root.activeIndex >= 0)
                lyricsList.positionViewAtIndex(root.activeIndex, ListView.Center)
        }
    }
}
