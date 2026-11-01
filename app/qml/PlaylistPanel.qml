import QtQuick
import EchoPlay
import "Format.js" as Format

// The clips in the opened file's folder, only that folder (mpv's playlist from autocreate-playlist). Thumbnails,
// durations and track counts come from ClipInfo, for the rows on screen.
GlassPanel {
    id: panel

    required property var ui
    readonly property var glass: Theme.tokens.glass
    readonly property var strings: I18n.strings
    readonly property var entries: ui.window.playlist
    readonly property string folder: entries.length ? entries[0].filename.replace(/[\\/][^\\/]*$/, "") : ""

    objectName: "playlistPanel"
    backdrop: ui.backdrop

    function askVisibleRows() {
        const first = Math.max(0, list.indexAt(0, list.contentY))
        const last = list.indexAt(0, list.contentY + list.height - 1)
        const end = last < 0 ? entries.length - 1 : last
        const paths = []
        for (let i = first; i <= end && i < entries.length; ++i) paths.push(entries[i].filename)
        ClipInfo.request(paths)
    }
    function showPlaying() {
        const playing = entries.findIndex(e => e.playing || e.current)
        if (playing >= 0) list.positionViewAtIndex(playing, ListView.Contain)
        ask.restart()
    }
    onEntriesChanged: showPlaying()
    Component.onCompleted: showPlaying()

    MouseArea { // keeps clicks and the wheel from the video below
        anchors.fill: parent
        onWheel: wheel => wheel.accepted = true
    }
    Timer { // scrolling asks once, for where it stopped
        id: ask
        interval: 100
        onTriggered: panel.askVisibleRows()
    }
    Column {
        x: 16
        y: 14
        width: parent.width - 32 - 44
        spacing: 2
        Label {
            width: parent.width
            text: panel.strings.in_this_folder
            color: panel.glass.text
            font.pixelSize: 16
            font.weight: Font.Bold
        }
        Label {
            width: parent.width
            elide: Text.ElideMiddle
            text: panel.folder + " · " + (panel.entries.length === 1 ? panel.strings.clip_count_one : panel.strings.clips_count.arg(panel.entries.length))
            color: panel.glass.textMuted
            font.pixelSize: Theme.tokens.type.small
        }
    }
    IconButton {
        objectName: "playlistClose"
        anchors.right: parent.right
        anchors.rightMargin: 10
        y: 14
        implicitWidth: 36
        implicitHeight: 36
        icon: "close"
        iconSize: Theme.tokens.icon.sm
        background: panel.glass.controlSubtle
        hoverColor: panel.glass.control
        tip: panel.strings.close_playlist
        onClicked: panel.ui.playlistOpen = false
    }
    ListView {
        id: list
        x: 10
        y: 60
        width: parent.width - 20
        height: parent.height - y - 10
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: panel.entries
        onContentYChanged: ask.restart()
        onHeightChanged: ask.restart()
        delegate: Item {
            id: row
            required property var modelData
            required property int index
            readonly property string path: modelData.filename
            readonly property bool playing: !!(modelData.playing || modelData.current)
            readonly property var info: {
                ClipInfo.revision
                return ClipInfo.info(path)
            }
            objectName: "playlistRow-" + index
            width: list.width
            height: 70
            activeFocusOnTab: true
            Accessible.role: Accessible.ListItem
            Accessible.name: name.text
            Keys.onReturnPressed: panel.ui.mpv.command(["playlist-play-index", index])
            Rectangle {
                anchors.fill: parent
                radius: Theme.tokens.radius.md
                color: row.playing ? Qt.alpha(Theme.accent, 0.14) : panel.glass.controlSubtle
                opacity: row.playing || rowMouse.containsMouse ? 1 : 0
                border.width: row.playing ? 1 : 0
                border.color: Qt.alpha(Theme.accent, 0.45)
            }
            FocusRing { target: row; targetRadius: Theme.tokens.radius.md }
            Rectangle {
                id: frame
                x: 9
                y: 8
                width: 96
                height: 54
                radius: Theme.tokens.radius.sm
                color: "black"
                RoundedImage { // a clip that is not 16:9 keeps its shape, with black beside it
                    visible: !!row.info.thumbnail
                    anchors.centerIn: parent
                    width: Math.min(parent.width, parent.height * aspect)
                    height: Math.min(parent.height, parent.width / aspect)
                    source: row.info.thumbnail || ""
                }
                Rectangle {
                    visible: row.info.duration > 0
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 4
                    width: badge.implicitWidth + 10
                    height: badge.implicitHeight + 2
                    radius: 5
                    color: Qt.alpha(panel.glass.fill, 0.85)
                    Label {
                        id: badge
                        anchors.centerIn: parent
                        text: Format.time(row.info.duration || 0)
                        color: panel.glass.text
                        font.pixelSize: 11
                        font.weight: Font.Bold
                        font.features: {"tnum": 1}
                    }
                }
            }
            Column {
                x: frame.x + frame.width + 12
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - x - 9
                spacing: 3
                Label {
                    id: name
                    width: parent.width
                    wrapMode: Text.WrapAnywhere
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    lineHeight: 17
                    lineHeightMode: Text.FixedHeight
                    text: row.path.split(/[\\/]/).pop()
                    color: panel.glass.text
                    font.pixelSize: Theme.tokens.type.small
                    font.weight: Font.DemiBold
                }
                Row {
                    visible: row.playing || row.info.audioTracks !== undefined
                    spacing: 4
                    readonly property color tone: row.playing ? Qt.lighter(Theme.accent, 1.2) : panel.glass.textMuted
                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: row.playing ? "equalizer" : "graphic_eq"
                        filled: true
                        size: 15
                        color: parent.tone
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: row.playing ? panel.strings.playing
                             : row.info.audioTracks === 0 ? panel.strings.no_audio
                             : row.info.audioTracks === 1 ? panel.strings.track_count_one
                             : panel.strings.tracks_count.arg(row.info.audioTracks)
                        color: parent.tone
                        font.pixelSize: Theme.tokens.type.caption
                        font.weight: Font.DemiBold
                    }
                }
            }
            MouseArea {
                id: rowMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: panel.ui.mpv.command(["playlist-play-index", row.index])
            }
        }
    }
}
