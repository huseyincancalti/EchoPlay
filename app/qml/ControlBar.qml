import QtQuick
import EchoPlay
import "Format.js" as Format

// The control bar: a floating island or edge to edge (Theme.barStyle). Below ~820 px the volume slider and
// the mixer chip's text collapse.
GlassPanel {
    id: bar

    required property var ui
    readonly property bool flush: Theme.barStyle === "full"
    readonly property bool narrow: width < 820
    readonly property bool hovered: hover.hovered
    readonly property real chipCenter: x + rightGroup.x + chip.x + chip.width / 2
    readonly property real gearCenter: x + rightGroup.x + gear.x + gear.width / 2
    readonly property real subtitlesCenter: x + rightGroup.x + subtitles.x + subtitles.width / 2
    readonly property var glass: Theme.tokens.glass
    readonly property bool boosted: ui.volume > 100
    readonly property color volumeColor: !boosted ? glass.text : ui.volume >= 200 ? glass.danger : Theme.accent
    readonly property bool noSound: ui.noSound

    objectName: "controlBar"
    backdrop: ui.backdrop
    implicitHeight: 92
    fillOpacity: Theme.barOpacity
    radius: flush ? 0 : Theme.tokens.radius.lg
    shadow: !flush

    HoverHandler { id: hover }
    MouseArea { // clicks and double-clicks on the bar's empty parts do nothing
        anchors.fill: parent
        onDoubleClicked: {}
    }
    SeekBar {
        id: seekBar
        x: 16
        y: 11
        width: parent.width - 32
        position: bar.ui.position
        duration: bar.ui.duration
        hasVideo: bar.ui.hasVideo
        loopA: bar.ui.loopA
        loopB: bar.ui.loopB
        bounds: bar.parent
        onSeekRequested: (seconds, exact) => bar.ui.seekTo(seconds, exact)
    }
    Row {
        id: leftGroup
        x: 16
        y: 35
        height: 44
        spacing: 2
        IconButton {
            objectName: "playButton"
            icon: bar.ui.paused ? "play_arrow" : "pause"
            filled: true
            iconSize: Theme.tokens.icon.lg
            tip: bar.ui.paused ? I18n.strings.play : I18n.strings.pause
            onClicked: bar.ui.mpv.command(["cycle", "pause"])
        }
        Item { width: 4; height: 1 }
        IconButton {
            objectName: "muteButton"
            icon: bar.ui.muted ? "volume_off" : "volume_up"
            filled: bar.boosted
            color: bar.volumeColor
            tip: I18n.strings.mute
            onClicked: bar.ui.mpv.command(["cycle", "mute"])
        }
        VolumeSlider {
            visible: !bar.narrow
            anchors.verticalCenter: parent.verticalCenter
            volume: bar.ui.muted ? 0 : bar.ui.volume
            maxVolume: PlayerSettings.volumeMax
            color: bar.volumeColor
            onMoved: volume => {
                if (bar.ui.muted) bar.ui.mpv.setProperty("mute", false)
                bar.ui.mpv.setProperty("volume", volume)
            }
        }
        Item { visible: !bar.narrow; width: 6; height: 1 }
        Label {
            visible: !bar.narrow
            width: 44
            anchors.verticalCenter: parent.verticalCenter
            text: Math.round(bar.ui.volume) + "%"
            color: bar.volumeColor
            font.weight: Font.Bold
            font.features: {"tnum": 1}
        }
        Item { width: 6; height: 1 }
        Item { // a click: what has played, or what is left
            objectName: "timeDisplay"
            anchors.verticalCenter: parent.verticalCenter
            width: times.width
            height: times.height
            Row {
                id: times
                Label {
                    objectName: "timeText"
                    text: PlayerSettings.showRemaining ? "-" + Format.time(Math.max(0, bar.ui.duration - bar.ui.position)) : Format.time(bar.ui.position)
                    color: bar.glass.text
                    font.weight: Font.DemiBold
                    font.features: {"tnum": 1}
                }
                Label {
                    text: " / " + Format.time(bar.ui.duration)
                    color: bar.glass.textMuted
                    font.weight: Font.Medium
                    font.features: {"tnum": 1}
                }
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: PlayerSettings.showRemaining = !PlayerSettings.showRemaining
            }
        }
    }
    Row {
        id: rightGroup
        anchors.right: parent.right
        anchors.rightMargin: 16
        y: 35
        height: 44
        spacing: 2
        Item { // the mixer chip: the reason EchoPlay exists, always in sight
            id: chip
            objectName: "mixerChip"
            readonly property bool open: bar.ui.popover === "mixer"
            readonly property color tone: bar.noSound ? bar.glass.warning : Theme.accent
            anchors.verticalCenter: parent.verticalCenter
            width: bar.narrow ? 44 : 164
            height: 36
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: I18n.strings.audio_tracks
            Keys.onReturnPressed: bar.ui.togglePopover("mixer")
            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: chip.open ? Theme.accent : Qt.alpha(chip.tone, chipMouse.containsMouse ? 0.26 : 0.18)
                border.width: 1
                border.color: chip.open ? Theme.accent : Qt.alpha(chip.tone, bar.noSound ? 0.70 : 0.60)
            }
            Rectangle { // the first-run hint's ring around the chip
                visible: bar.ui.hintShown
                anchors.fill: parent
                anchors.margins: -5
                radius: height / 2
                color: "transparent"
                border.width: 2
                border.color: Theme.accent
            }
            FocusRing { target: chip; targetRadius: chip.height / 2 }
            Row {
                anchors.centerIn: parent
                spacing: 8
                width: Math.min(implicitWidth, chip.width - 16)
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: bar.noSound ? "volume_off" : "graphic_eq"
                    filled: true
                    size: 20
                    color: chip.open ? Theme.onAccent : chip.tone
                }
                Label {
                    visible: !bar.narrow
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, chip.width - 20 - 20 - 32)
                    text: Format.chip(bar.ui.tracks, I18n.strings)
                    color: chip.open ? Theme.onAccent : bar.glass.text
                    font.weight: Font.Bold
                }
                Icon {
                    visible: !bar.narrow
                    anchors.verticalCenter: parent.verticalCenter
                    name: "expand_less"
                    size: 20
                    color: chip.open ? Theme.onAccent : bar.glass.text
                }
            }
            MouseArea {
                id: chipMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: bar.ui.togglePopover("mixer")
            }
        }
        Item { width: 4; height: 1 }
        IconButton {
            id: subtitles
            objectName: "subtitlesButton"
            icon: "subtitles"
            filled: bar.ui.subtitleTrack > 0
            enabledLook: bar.ui.subtitleTracks.length > 0
            tip: I18n.strings.subtitles
            onClicked: bar.ui.togglePopover("subtitles")
        }
        IconButton {
            objectName: "playlistButton"
            icon: "playlist_play"
            background: bar.ui.playlistOpen ? bar.glass.control : "transparent"
            tip: I18n.strings.playlist
            onClicked: {
                bar.ui.playlistOpen = !bar.ui.playlistOpen
                if (bar.ui.playlistOpen) bar.ui.popover = "" // both live at the bar's right end
            }
        }
        IconButton {
            id: gear
            objectName: "gearButton"
            icon: "settings"
            filled: bar.ui.popover === "quick"
            background: bar.ui.popover === "quick" ? bar.glass.control : "transparent"
            tip: I18n.strings.quick_settings
            onClicked: bar.ui.togglePopover("quick")
        }
        IconButton {
            objectName: "fullscreenButton"
            icon: bar.ui.fullScreen ? "fullscreen_exit" : "fullscreen"
            iconSize: 26
            tip: I18n.strings.full_screen
            onClicked: bar.ui.toggleFullScreen()
        }
    }
}
