import QtQuick
import EchoPlay
import "Format.js" as Format

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
    readonly property alias resumeCard: resumeLoader.item

    // mpv's state, as observed.
    property bool paused: false
    property bool muted: false
    property real volume: 100
    property real speed: 1
    property real duration: 0
    property real position: 0
    property var trackList: []
    property var subtitleTrack: false
    property real loopA: -1 // the A-B loop's ends in seconds, -1 while not set
    property real loopB: -1
    property real videoZoom: 0 // mpv's video-zoom: log2, 0 = 100%
    property int videoRotate: 0
    property real videoAspect: -1 // -1: the clip's own
    property real subDelay: 0 // seconds
    property real audioDelay: 0
    readonly property bool pictureChanged: ["brightness", "contrast", "saturation", "gamma"].some(name => PlayerSettings.picture[name] !== 0)
                                           || videoZoom !== 0 || videoRotate !== 0 || videoAspect !== -1

    readonly property var tracks: MixerController.tracks
    readonly property bool hasVideo: trackList.some(t => t.type === "video")
    readonly property var subtitleTracks: trackList.filter(t => t.type === "sub")
    readonly property bool fullScreen: window.visibility === Window.FullScreen

    // "" or the open popover: "mixer", "quick", "subtitles", "picture", "info" (clip info), "menu" (the right-click
    // menu).
    property string popover: forceChrome ? "mixer" : ""
    property point menuAt // where the right-click was, in this item
    property string menuPath // the clip it is about
    property bool menuForRow: false // a playlist row's
    property bool playlistOpen: false
    property bool settingsOpen: false
    // Opening anything, by mouse or by key, is doing something else: the resume question is over.
    onPopoverChanged: if (popover !== "") window.resumeOffer = null
    onPlaylistOpenChanged: if (playlistOpen) window.resumeOffer = null
    onSettingsOpenChanged: if (settingsOpen) window.resumeOffer = null
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
    function openMenu(x, y, path, forRow) {
        menuAt = Qt.point(x, y)
        menuPath = path
        menuForRow = forRow
        popover = ""
        popover = "menu" // a second right-click opens it anew, at the new place
    }
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
        const progress = Updater.onClose ? I18n.strings.download_progress_close : I18n.strings.download_progress
        return progress.arg(mb(Updater.received)).arg(mb(Updater.total))
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
    // mpv's three steps: its start here, its end here, off. The bar comes up to show them on the seek bar.
    function cycleLoop() {
        const now = mpv.observed("time-pos") || 0
        window.showToast(loopA < 0 ? I18n.strings.loop_start.arg(Format.time(now))
                         : loopB < 0 ? I18n.strings.loop_on.arg(Format.time(Math.min(loopA, now))).arg(Format.time(Math.max(loopA, now)))
                         : I18n.strings.loop_off, 2500)
        wake()
        mpv.command(["ab-loop"])
    }

    function observe(name, value) {
        switch (name) {
        case "pause": paused = value === true; break
        case "mute": muted = value === true; break
        case "volume": volume = value || 0; break
        case "speed": speed = value || 1; break
        case "duration": duration = value || 0; break
        case "track-list": trackList = value || []; break
        case "sid": subtitleTrack = value; break
        case "ab-loop-a": loopA = typeof value === "number" ? value : -1; break // "no" while not set
        case "ab-loop-b": loopB = typeof value === "number" ? value : -1; break
        case "video-zoom": videoZoom = value || 0; break
        case "video-rotate": videoRotate = value || 0; break
        case "video-aspect-override": videoAspect = value > 0 ? value : -1; break // "no" reads 0, the old auto -1
        case "sub-delay": subDelay = value || 0; break
        case "audio-delay": audioDelay = value || 0; break
        }
    }
    Component.onCompleted: {
        for (const name of ["pause", "mute", "volume", "speed", "duration", "track-list", "sid", "ab-loop-a", "ab-loop-b", "video-zoom", "video-rotate",
                            "video-aspect-override", "sub-delay", "audio-delay"]) {
            mpv.observeProperty(name)
            observe(name, mpv.observed(name))
        }
        position = mpv.observed("time-pos") || 0
        mpv.setProperty("sub-margin-y", String(subtitleMargin))
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
    // Subtitles stand above the bar while it shows, as on YouTube: mpv puts them at the bottom, under the bar, so a
    // pause hid them. sub-margin-y counts in mpv's 720-line units; 22 is its default.
    readonly property int subtitleMargin: controlBar.visible && height > 0
                                          ? Math.max(22, Math.round((height - controlBar.y + 8) * 720 / height)) : 22
    onSubtitleMarginChanged: mpv.setProperty("sub-margin-y", String(subtitleMargin))
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
        objectName: "glassBackdrop"
        anchors.fill: parent
        source: ui.mpv
        active: ui.chromeUp || flash.shown || resumeLoader.active || ui.window.cardShown
    }
    MouseArea { // the video itself
        property bool pausedByClick: false
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: ui.chromeUp ? Qt.ArrowCursor : Qt.BlankCursor
        // A click that closes something, or ends the resume question, does only that.
        onClicked: mouse => {
            if (mouse.button === Qt.RightButton) {
                ui.openMenu(mouse.x, mouse.y, ui.mpv.observed("path") || "", false)
                return
            }
            pausedByClick = !ui.closeTop() && !ui.window.clickEndedQuestion() && PlayerSettings.clickPauses
            if (pausedByClick) ui.mpv.command(["cycle", "pause"])
        }
        onDoubleClicked: mouse => {
            if (mouse.button !== Qt.LeftButton) return
            if (pausedByClick) ui.mpv.command(["cycle", "pause"]) // the first click's, as on YouTube: no wait before a pause
            ui.toggleFullScreen()
        }
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
        name: "picture"
        anchorX: controlBar.gearCenter
        sourceComponent: PicturePopover {
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

    Loader { // as tall as its clips, growing up from the bar; it scrolls only when they do not fit below the title bar
        id: playlistLoader
        readonly property real highest: ui.fullScreen ? Theme.tokens.bar.inset : 56 // the panel's top edge, at most
        x: ui.width - 360 - Theme.tokens.bar.inset
        y: controlBar.y - 12 - (item ? item.height : 0)
        active: ui.playlistOpen
        sourceComponent: PlaylistPanel {
            ui: player
            width: 360
            height: Math.min(wanted, controlBar.y - 12 - playlistLoader.highest)
        }
    }

    Loader { // clip info, in the middle
        x: item ? (ui.width - item.width) / 2 : 0
        y: item ? Math.max(16, (ui.height - item.height) / 2 - 24) : 0
        active: ui.popover === "info"
        sourceComponent: MediaInfoPanel { ui: player; height: implicitHeight }
    }
    Loader { // at the pointer, inside the window: below and right of it where it fits
        x: item ? Math.max(8, Math.min(ui.menuAt.x, ui.width - item.width - 8)) : 0
        y: item ? (ui.menuAt.y + item.height <= ui.height - 8 ? ui.menuAt.y : Math.max(8, ui.menuAt.y - item.height)) : 0
        active: ui.popover === "menu"
        sourceComponent: ContextMenu {
            ui: player
            path: player.menuPath
            forRow: player.menuForRow
            height: implicitHeight
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
        id: resumeLoader
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
