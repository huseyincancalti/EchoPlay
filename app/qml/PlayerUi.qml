import QtQuick
import EchoPlay

// Everything over the video. Main.qml keeps the startup sequence and mpv's setup; this reads mpv only through
// observed values (a synchronous get can stall the render loop) and polls the position on a timer, never per frame.
Item {
    id: ui

    required property MpvItem mpv
    required property var window
    required property WindowChrome chrome
    required property string mediaTitle
    property bool forceChrome: false // bench: a bar and a popover stay open, the worst case for the video

    anchors.fill: parent

    readonly property var player: ui // for loaded parts: inside them, "ui" is their own property
    readonly property alias backdrop: glassBackdrop
    readonly property alias bar: controlBar

    // mpv's state, as observed.
    property bool paused: false
    property bool muted: false
    property real volume: 100
    property real speed: 1
    property real duration: 0
    property real position: 0
    property var trackList: []
    property var subtitleTrack: false

    readonly property var tracks: MixerController.tracks
    readonly property bool hasVideo: trackList.some(t => t.type === "video")
    readonly property var subtitleTracks: trackList.filter(t => t.type === "sub")
    readonly property bool fullScreen: window.visibility === Window.FullScreen

    // "" or the open popover: "mixer", "quick", "subtitles".
    property string popover: forceChrome ? "mixer" : ""
    property bool playlistOpen: false
    property bool settingsOpen: false
    // Once ever, and never in a bench run. Opening the mixer counts as seeing it.
    readonly property bool hintShown: !forceChrome && !PlayerSettings.firstRunHintDone && tracks.length >= 2
    readonly property bool noSound: tracks.length > 0 && !tracks.some(t => t.enabled)
    readonly property string subtitleName: {
        const selected = subtitleTracks.find(t => t.id === subtitleTrack)
        return selected ? trackLabel(selected) : I18n.strings.off
    }

    function togglePopover(name) {
        popover = popover === name ? "" : name
        if (popover !== "") playlistOpen = false // both live at the bar's right end
        if (name === "mixer") PlayerSettings.firstRunHintDone = true
    }
    function toggleFullScreen() { chrome.toggleFullScreen() }
    function trackLabel(track) { return track.title || track.lang || I18n.strings.track_n.arg(track.id) }
    function openSettings() {
        popover = ""
        settingsOpen = true
    }
    function installUpdate() {
        if (UpdateCheck.setupUrl) Updater.install(UpdateCheck.setupUrl, UpdateCheck.setupSha256, UpdateCheck.latestVersion)
        else Qt.openUrlExternally(UpdateCheck.releasesUrl) // the release has no setup EchoPlay can use
    }
    function downloadText() {
        if (Updater.state === "installing") return I18n.strings.update_restart
        const mb = bytes => Math.round(bytes / 1048576)
        return I18n.strings.download_progress.arg(mb(Updater.received)).arg(mb(Updater.total))
    }
    function seekTo(seconds, exact) {
        position = seconds
        mpv.command(["seek", seconds, exact ? "absolute+exact" : "absolute+keyframes"])
    }
    // The top thing closes first. False: nothing was open.
    function closeTop() {
        if (settings.item) return settings.item.closeTop()
        if (popover !== "") { popover = ""; return true }
        if (playlistOpen) { playlistOpen = false; return true }
        return false
    }
    function flashVolume() { flash.request() }
    function sweepArrows(forward) { sweep.play(forward) }

    function observe(name, value) {
        switch (name) {
        case "pause": paused = value === true; break
        case "mute": muted = value === true; break
        case "volume": volume = value || 0; break
        case "speed": speed = value || 1; break
        case "duration": duration = value || 0; break
        case "track-list": trackList = value || []; break
        case "sid": subtitleTrack = value; break
        }
    }
    Component.onCompleted: {
        for (const name of ["pause", "mute", "volume", "speed", "duration", "track-list", "sid"]) {
            mpv.observeProperty(name)
            observe(name, mpv.observed(name))
        }
        position = mpv.observed("time-pos") || 0
    }
    Connections {
        target: ui.mpv
        function onPropertyChanged(name, value) { ui.observe(name, value) }
    }
    Timer {
        running: true
        repeat: true
        interval: ui.chromeUp ? 100 : 500
        onTriggered: ui.position = ui.mpv.observed("time-pos") || 0
    }

    // Chrome hides after 1 s without mouse movement, unless paused, a popover is open or the pointer is on a
    // control; then only a 2 px progress line remains, and a blank cursor.
    property bool pointerMoving: true
    readonly property bool overControls: controlBar.hovered || (titleBar.visible && titleBar.hovered)
    readonly property bool chromeUp: forceChrome || pointerMoving || paused || popover !== "" || playlistOpen || settingsOpen || hintShown || overControls

    function wake() {
        pointerMoving = true
        idle.restart()
    }
    Timer {
        id: idle
        interval: 1000
        running: true
        onTriggered: ui.pointerMoving = false
    }
    Connections {
        target: ui.chrome
        function onPointerMoved() { ui.wake() }
    }
    HoverHandler {
        // Qt re-sends the hover every frame while anything moves, and the video always does: only a real move counts.
        property point last
        onPointChanged: {
            if (point.position === last) return
            last = point.position
            ui.wake()
        }
    }

    GlassBackdrop {
        id: glassBackdrop
        anchors.fill: parent
        source: ui.mpv
        active: ui.chromeUp || flash.shown
    }
    MouseArea { // the video itself
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        cursorShape: ui.chromeUp ? Qt.ArrowCursor : Qt.BlankCursor
        onClicked: ui.closeTop()
        onDoubleClicked: ui.toggleFullScreen()
        onWheel: wheel => {
            ui.flashVolume()
            ui.mpv.command(["add", "volume", wheel.angleDelta.y > 0 ? 5 : -5])
        }
    }

    Rectangle { // the idle progress line
        visible: !ui.chromeUp && ui.duration > 0
        anchors.bottom: parent.bottom
        width: parent.width
        height: 2
        color: "#40FFFFFF"
        Rectangle {
            width: parent.width * Math.min(1, ui.position / Math.max(0.001, ui.duration))
            height: parent.height
            color: Theme.accent
        }
    }

    TitleBar {
        id: titleBar
        objectName: "titleBar"
        z: 2 // above settings' dimming
        visible: opacity > 0 && !ui.fullScreen
        opacity: ui.chromeUp ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.tokens.motion.base } }
        backdrop: glassBackdrop
        width: parent.width
        title: ui.mediaTitle
        chrome: ui.chrome
        window: ui.window
    }
    GlassPanel { // full screen: the name in a pill instead of the title bar
        objectName: "titlePill"
        visible: opacity > 0 && ui.fullScreen
        opacity: ui.chromeUp ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.tokens.motion.base } }
        backdrop: glassBackdrop
        x: 24
        y: 24
        width: Math.min(900, pillText.implicitWidth + 32, parent.width - 48)
        height: pillText.implicitHeight + 20
        radius: 14
        shadow: false
        fillOpacity: Theme.barOpacity
        Label {
            id: pillText
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            verticalAlignment: Text.AlignVCenter
            text: ui.mediaTitle
            color: Theme.tokens.glass.text
            font.pixelSize: Theme.tokens.type.label
            font.weight: Font.DemiBold
        }
    }

    ControlBar {
        id: controlBar
        ui: ui
        readonly property real inset: flush ? 0 : ui.fullScreen ? 24 : Theme.tokens.bar.inset
        visible: opacity > 0
        opacity: ui.chromeUp && !ui.settingsOpen ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.tokens.motion.base } }
        x: flush ? -1 : inset
        y: parent.height - height - inset + (flush ? 1 : 0)
        width: flush ? parent.width + 2 : parent.width - 2 * inset
        height: implicitHeight
    }

    component AnchoredLoader: Loader { // above the bar, centered on a button as far as the window allows
        x: {
            if (!item) return 0
            return Math.min(ui.width - item.width - 16, Math.max(16, anchorX - item.width / 2))
        }
        y: item ? controlBar.y - item.height - 12 : 0
        property real anchorX: 0
    }
    component PopoverLoader: AnchoredLoader {
        required property string name
        readonly property bool open: ui.popover === name
        active: open
    }
    PopoverLoader {
        name: "mixer"
        anchorX: controlBar.chipCenter
        sourceComponent: MixerPopover {
            ui: player
            pointerX: controlBar.chipCenter - x - parent.x
            height: implicitHeight
        }
    }
    PopoverLoader {
        name: "quick"
        anchorX: controlBar.gearCenter
        sourceComponent: QuickSettings {
            ui: player
            pointerX: controlBar.gearCenter - x - parent.x
            height: implicitHeight
        }
    }
    PopoverLoader {
        name: "subtitles"
        anchorX: controlBar.subtitlesCenter
        sourceComponent: SubtitlesPopover {
            ui: player
            pointerX: controlBar.subtitlesCenter - x - parent.x
            height: implicitHeight
        }
    }

    Loader {
        x: ui.width - 360 - Theme.tokens.bar.inset
        y: ui.fullScreen ? Theme.tokens.bar.inset : 56
        active: ui.playlistOpen
        sourceComponent: PlaylistPanel {
            ui: player
            width: 360
            height: controlBar.y - 12 - parent.y
        }
    }

    AnchoredLoader {
        anchorX: controlBar.chipCenter
        active: ui.hintShown && ui.popover === "" && !ui.settingsOpen
        sourceComponent: HintCard {
            ui: player
            pointerX: controlBar.chipCenter - x - parent.x
            height: implicitHeight
        }
    }
    AnchoredLoader {
        anchorX: controlBar.chipCenter
        active: ui.noSound && ui.chromeUp && ui.popover === "" && !ui.settingsOpen && !ui.hintShown
        sourceComponent: NoSoundCard {
            ui: player
            pointerX: controlBar.chipCenter - x - parent.x
            height: implicitHeight
        }
    }
    Loader {
        x: Theme.tokens.bar.inset
        y: item ? controlBar.y - item.height - 12 : 0
        active: !!ui.window.resumeOffer && !ui.settingsOpen
        sourceComponent: ResumeCard {
            ui: player
            height: implicitHeight
        }
    }

    Loader {
        id: settings
        z: 1
        anchors.fill: parent
        active: ui.settingsOpen
        // By name, not by type: its parts import QtQuick.Shapes, and most launches never open settings.
        onActiveChanged: if (active) setSource("SettingsOverlay.qml", {ui: ui})
    }

    VolumeFlash {
        id: flash
        volume: ui.volume
        maxVolume: PlayerSettings.volumeMax
        backdrop: glassBackdrop
        anchors.horizontalCenter: parent.horizontalCenter
        y: 64
    }
    ArrowSweep { id: sweep }
}
