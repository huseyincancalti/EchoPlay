import QtQuick
import EchoPlay
import "Format.js" as Format

// The seek bar: played part, what the pointer is over, and a preview of that moment above it.
Item {
    id: seek

    required property real position
    required property real duration
    property bool hasVideo: true
    property real loopA: -1 // the A-B loop's two ends in seconds, -1 while not set
    property real loopB: -1
    property var chapters: [] // mpv's chapter-list: {title, time}
    property Item bounds: parent // the preview stays inside this item
    signal seekRequested(real seconds, bool exact)
    readonly property bool active: hover.hovered || mouse.pressed

    implicitHeight: 20
    objectName: "seekBar"

    readonly property real pointerFraction: Math.min(1, Math.max(0, hover.point.position.x / Math.max(1, width)))
    property real dragFraction: 0
    readonly property real shownFraction: mouse.pressed ? dragFraction : duration > 0 ? Math.min(1, position / duration) : 0
    readonly property real targetFraction: mouse.pressed ? dragFraction : pointerFraction
    readonly property real targetSeconds: targetFraction * duration
    readonly property string targetChapter: {
        let title = ""
        for (let i = 0; i < chapters.length && chapters[i].time <= targetSeconds; ++i)
            title = chapters[i].title || I18n.strings.chapter_n.arg(i + 1)
        return title
    }

    onTargetSecondsChanged: if (active && hasVideo && duration > 0) Thumbnailer.request(targetSeconds)

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: seek.active ? 6 : 4
        radius: height / 2
        color: Theme.tokens.glass.track
        Rectangle { // up to the pointer
            visible: seek.active
            width: parent.width * seek.targetFraction
            height: parent.height
            radius: height / 2
            color: Theme.tokens.glass.trackHover
        }
        Rectangle { // played
            width: parent.width * seek.shownFraction
            height: parent.height
            radius: height / 2
            color: Theme.accent
        }
        Rectangle { // the A-B loop, over what has played
            visible: seek.loopA >= 0 && seek.loopB >= 0 && seek.duration > 0
            x: parent.width * Math.min(seek.loopA, seek.loopB) / Math.max(1, seek.duration)
            width: parent.width * Math.abs(seek.loopB - seek.loopA) / Math.max(1, seek.duration)
            height: parent.height
            color: Qt.alpha(Theme.tokens.glass.text, 0.45)
        }
        Repeater { // each chapter's start: a gap in the track, as on YouTube
            model: seek.chapters
            Rectangle {
                required property var modelData
                required property int index
                objectName: "chapterMark-" + index
                visible: modelData.time > 0 && modelData.time < seek.duration
                x: parent.width * modelData.time / Math.max(1, seek.duration) - 1
                width: 2
                height: parent.height
                color: Theme.tokens.glass.fill
            }
        }
    }
    Repeater { // its ends
        model: [{name: "loopStartMark", at: seek.loopA}, {name: "loopEndMark", at: seek.loopB}]
        Rectangle {
            required property var modelData
            objectName: modelData.name
            visible: modelData.at >= 0 && seek.duration > 0
            anchors.verticalCenter: parent.verticalCenter
            x: seek.width * modelData.at / Math.max(1, seek.duration) - 1
            width: 2
            height: 14
            radius: 1
            color: Theme.tokens.glass.text
        }
    }
    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        x: seek.width * seek.shownFraction - width / 2
        width: 14
        height: 14
        radius: 7
        color: "#FFFFFF"
    }
    HoverHandler { id: hover }
    MouseArea {
        id: mouse
        anchors.fill: parent
        preventStealing: true
        function fraction() { return Math.min(1, Math.max(0, mouseX / Math.max(1, width))) }
        onPressed: {
            seek.dragFraction = fraction()
            seek.seekRequested(seek.dragFraction * seek.duration, false)
        }
        onPositionChanged: {
            if (!pressed) return
            seek.dragFraction = fraction()
            seek.seekRequested(seek.dragFraction * seek.duration, false)
        }
        onReleased: seek.seekRequested(seek.dragFraction * seek.duration, true)
    }

    // The preview, 30 px above the bar. Its width follows the video's shape at a fixed height.
    Column {
        id: preview
        visible: seek.active && seek.duration > 0
        spacing: 6
        width: Math.max(picture.visible ? picture.width : 0, timeTag.width)
        y: -height - 10
        x: {
            const wanted = seek.width * seek.targetFraction - width / 2
            const offset = seek.mapToItem(seek.bounds, 0, 0).x
            return Math.min(seek.bounds.width - width - 8 - offset, Math.max(8 - offset, wanted))
        }
        RoundedImage {
            id: picture
            anchors.horizontalCenter: parent.horizontalCenter
            visible: seek.hasVideo && Thumbnailer.source.toString() !== ""
            height: 119
            width: Math.round(height * aspect)
            source: visible ? Thumbnailer.source : ""
            radius: 10
            borderWidth: 2
            borderColor: Theme.tokens.glass.line
        }
        Rectangle {
            id: timeTag
            anchors.horizontalCenter: parent.horizontalCenter
            width: tag.implicitWidth + 20
            height: tag.implicitHeight + 6
            radius: height / 2
            color: Qt.alpha(Theme.tokens.glass.fill, 0.86)
            Row { // the time, then the chapter it falls in
                id: tag
                anchors.centerIn: parent
                spacing: 8
                Label {
                    text: Format.time(seek.targetSeconds)
                    color: Theme.tokens.glass.text
                    font.pixelSize: Theme.tokens.type.small
                    font.weight: Font.Bold
                    font.features: {"tnum": 1}
                }
                Label {
                    objectName: "seekChapter"
                    visible: text !== ""
                    width: Math.min(implicitWidth, 240)
                    text: seek.targetChapter
                    color: Theme.tokens.glass.textMuted
                    font.pixelSize: Theme.tokens.type.small
                    font.weight: Font.DemiBold
                }
            }
        }
    }
}
