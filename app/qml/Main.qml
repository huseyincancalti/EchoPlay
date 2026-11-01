import QtQuick
import EchoPlay

// The window, mpv's setup and the startup sequence. Everything over the video is PlayerUi.qml, loaded once the
// first frame plays, so it costs the first frame nothing.
Window {
    id: root
    width: 1280
    height: 720
    minimumWidth: 640
    minimumHeight: 360
    flags: Qt.Window | Qt.FramelessWindowHint // WindowChrome gives Windows its frame back
    visible: false // shown once mpv opens the file (MpvItem's onCompleted)
    color: "black"
    title: mediaTitle || "EchoPlay"

    required property string file
    required property int benchSeconds
    required property bool glass
    required property bool mix
    required property string logFile
    required property bool mixToggle
    required property bool thumbScrub
    required property bool tierCycle
    required property real shotAt
    required property string shotOut
    required property var startup // main_ms, app_ms from C++; qml_ms, render_ms, process_ms added here
    required property QtObject instance // SingleInstance; null in bench and shot runs

    readonly property string mixGraph: "[aid1][aid2]amix=inputs=2:normalize=0[ao]"
    property bool unpauseOnSwap: false
    onFrameSwapped: {
        if (startup.paint_ms === undefined) startup.paint_ms = mpv.processAgeMs() // window first on screen
        if (unpauseOnSwap) { unpauseOnSwap = false; mpv.setProperty("pause", false) }
    }
    property bool restarted: false
    property bool videoOn: false      // the render context exists, so video can play
    property bool earlyRestart: false // the file restarted before video was on: audio only so far
    property bool endedEarly: false   // the file ended before video was on: with vid=no a file without audio
    property bool videoRetried: false // vid=auto came after mpv had picked tracks for this file: asked once more
    property bool mixOn: mix
    property real handOverSentMs: 0 // epoch ms of a second launch whose file has not started playing yet
    property string mediaTitle: ""
    property string positionFile: ""  // the file whose time-pos can be trusted: it restarted
    property var playlist: []         // mpv's: the opened file's folder
    property var fileError: null      // {path, why}: the error screen shows
    property bool resumePending: false
    property var resumeOffer: null    // {at, savedAt}: "Resume from 1:24?" is open
    property bool keepOldSpot: false  // it was asked and Start over was not chosen: only a new spot worth saving replaces the old
    property bool updateDismissed: false // Install on close, the cross or 15 s: for this run
    property real questionEndedAt: 0  // when a click outside the card ended the resume question
    property bool speedKeyUsed: false // the next speed change came from a key: say it, as nothing else shows it with the bar down
    // Bench and screenshot runs keep the spike's fixed setup. Everything that makes the dev build a usable
    // player (v1's mixer, mpv options, volume memory) stays out of them, so the numbers stay comparable.
    readonly property bool measuring: benchSeconds > 0 || shotAt >= 0 || mix || mixToggle || thumbScrub || tierCycle

    // A second launch hands its file here (SingleInstance). An empty file only brings the window forward.
    Connections {
        target: root.instance
        function onFileRequested(file, sentMs) {
            if (file) {
                root.handOverSentMs = sentMs
                root.openPath(file)
            }
            root.show()
            root.raise()
            root.requestActivate()
        }
    }

    // Opened paused: mpv dropped the first ~8 frames when the clock ran while Qt was still building its first
    // frames. Playback starts once the first video frame is on screen (onFrameSwapped). The property, not a loadfile
    // option: the folder playlist replaces the opened entry and that option is gone, and the first half second of the
    // clip played with no picture.
    function openFile(file) {
        mpv.setProperty("pause", true)
        mpv.command(["loadfile", file, "replace", -1, root.shotAt >= 0 ? "start=" + root.shotAt : ""])
    }
    // Every way of opening after the first start: the start screen, a drop, a second launch's hand-over.
    function openPath(path) {
        root.file = path
        root.earlyRestart = root.endedEarly = root.videoRetried = false
        if (root.videoOn) {
            mpv.setProperty("pause", false) // a clip opened on purpose plays, whatever the last one was doing
            mpv.command(["loadfile", path, "replace"])
        } else root.openFile(path)
    }
    function openUrl(url) {
        const path = MediaFiles.pathToOpen(url)
        if (path) root.openPath(path)
        else toast.show(I18n.strings.no_videos_in_folder)
    }
    function chooser() {
        dialogs.active = true
        return dialogs.item
    }
    function showToast(message, ms) { toast.show(message, ms) }
    // That same click is on its way to the video.
    function clickEndedQuestion() { return Date.now() - root.questionEndedAt < 1000 }
    function pickFile() { chooser().pickVideo(url => root.openUrl(url)) }
    function pickFolder() { chooser().pickFolder(url => root.openUrl(url)) }
    // The clip after the one that failed, or -1. A file that fails stops playback (showError), the playlist stays.
    function nextClip() {
        const list = (playlist || []).map(entry => entry.filename.replace(/\\/g, "/"))
        const at = list.indexOf(root.fileError ? root.fileError.path.replace(/\\/g, "/") : "")
        return at >= 0 && at + 1 < list.length ? at + 1 : -1
    }
    function playNextClip() { mpv.command(["playlist-play-index", nextClip()]) }
    // mpv would go on to the next clip by itself and the failure would pass unseen: it stops here, on the error screen.
    function showError(why, entryId) {
        mpv.command(["stop", "keep-playlist"])
        const entry = root.playlist.find(e => e.id === entryId)
        const path = entry ? entry.filename : root.file
        root.fileError = {path: path, why: why}
    }
    // Asked for when it is needed: mpv writes its last line about a failure just after it reports the failure.
    function errorReportText() {
        return ErrorReport.report(root.fileError.path, root.fileError.why, mpv.recentLog(), UpdateCheck.currentVersion,
                                  mpv.observed("mpv-version") || "mpv")
    }
    // Once per file, on its first restart: where it was left, by the resume setting.
    function applyResume() {
        if (!root.resumePending || root.measuring) return
        root.resumePending = false
        const path = mpv.observed("path") || ""
        root.positionFile = path
        const at = ResumeStore.position(path)
        if (at <= 0 || PlayerSettings.resumeMode === "never") return
        if (PlayerSettings.resumeMode === "always") mpv.command(["seek", at, "absolute+exact"])
        else {
            root.keepOldSpot = true
            root.resumeOffer = {at: at, savedAt: ResumeStore.savedAt(path)}
        }
    }
    function firstRestart() {
        if (root.restarted) return
        root.restarted = true
        root.startup.process_ms = mpv.processAgeMs()
        mpv.command(["print-text", "STARTUP " + JSON.stringify(root.startup)])
        if (root.shotAt >= 0) shotTimer.start()
        else if ((mpv.observed("track-list") || []).some(t => t.type === "video")) root.unpauseOnSwap = true
        else mpv.setProperty("pause", false) // no video frame to wait for: nothing may repaint the window
    }

    // Same switching order as v1's apply_audio(): lavfi-complex first, then aid.
    function setMix(on) {
        mixOn = on
        mpv.setProperty("lavfi-complex", on ? mixGraph : "")
        mpv.setProperty("aid", on ? "no" : 1)
    }

    MpvItem {
        id: mpv
        anchors.fill: parent
        anchors.margins: chrome.inset // maximized, part of the window is off the screen
        Component.onCompleted: {
            root.startup.qml_ms = processAgeMs()
            Object.assign(root.startup, startupMarks())
            setOption("hwdec", "auto-safe")
            setOption("scale", "lanczos")
            setOption("cscale", "lanczos")
            setOption("dscale", "mitchell")
            setOption("keep-open", "yes")
            if (root.logFile) {
                setOption("log-file", root.logFile)
                setOption("msg-level", "all=v")
            }
            if (root.mix) setOption("lavfi-complex", root.mixGraph)
            if (root.benchSeconds > 0) setOption("mute", "yes")
            if (!root.measuring) {
                // v1's mpv.conf, player side.
                setOption("autocreate-playlist", "same") // the other videos in the folder are the playlist,
                setOption("directory-mode", "ignore")    // a subfolder next to them is not
                setOption("volume", String(Math.min(AppState.volume, PlayerSettings.volumeMax)))
                setOption("screenshot-format", "png")
                setOption("media-controls", "yes") // keyboards' media keys, headset buttons, Windows' media overlay
                root.appliedOptions = PlayerSettings.mpvOptions(root.file) // end of video, loudness, audio device, ...
                for (const name in root.appliedOptions) setOption(name, root.appliedOptions[name])
                setOption("cache", "auto")
                setOption("demuxer-max-bytes", "256MiB")
                const tier = PictureQuality.activeOptions() // a forced tier from the last session
                for (const name in tier) setOption(name, tier[name])
            }
            // Video cannot play before the render context exists: a VO that fails to start stays off for the
            // file. So mpv starts with vid=no and onRenderReady turns video on. Not a loadfile option: the folder
            // playlist replaces the opened entry, and loadfile options do not carry over to it.
            setOption("vid", "no")
            start()
            chrome.attach(root)
            observeProperty("track-list")
            // The file opens (demuxer, audio device: WASAPI alone took ~190 ms) while Qt creates the window, the
            // D3D11 device and the first frame (~600 ms before; measured 2026-10-31), instead of after them.
            if (root.file) root.openFile(root.file)
            if (!root.measuring) chrome.restorePlacement(PlayerSettings.windowPlacement)
            root.visible = true
            if (!root.measuring) {
                if (PlayerSettings.windowPlacement.maximized) chrome.toggleMaximized()
                chrome.setAlwaysOnTop(PlayerSettings.alwaysOnTop)
                // What media-controls sends. No other key ever reaches mpv: Qt has them all (root.actions).
                for (const [key, action] of [["PLAY", "set pause no"], ["PAUSE", "set pause yes"], ["PLAYPAUSE", "cycle pause"],
                                             ["STOP", "set pause yes"], ["NEXT", "playlist-next"], ["PREV", "playlist-prev"]])
                    command(["keybind", key, action])
                observeProperty("volume")
                observeProperty("media-title")
                observeProperty("speed")
                observeProperty("frame-drop-count")
                observeProperty("path")
                observeProperty("time-pos")
                observeProperty("duration")
                observeProperty("playlist")
                observeProperty("mpv-version")
            } else if (root.benchSeconds > 0) {
                // Read without blocking: a synchronous get every 100 ms can stall the render loop.
                observeProperty("frame-drop-count")
                observeProperty("playback-time")
                observeProperty("duration")
            }
        }
        onRenderReady: {
            root.startup.render_ms = processAgeMs()
            root.videoOn = true
            setProperty("vid", "auto")
            if (!root.file) return
            if (root.endedEarly) { root.openFile(root.file); return } // nothing to play without video: again, with it
            // Turning video on shows its frame but sends no playback restart; a seek in place does. (While the
            // file still opens, the seek fails and the first restart comes with video anyway.)
            command(["seek", 0, "relative+exact"])
            // A file without video restarted already (audio only) and gets no second restart.
            if (root.earlyRestart && !(observed("track-list") || []).some(t => t.type === "video")) {
                root.firstRestart()
                root.applyResume()
            }
        }
        onPlaybackRestarted: {
            if (root.handOverSentMs) {
                command(["print-text", "SECOND_OPEN " + JSON.stringify({ms: Date.now() - root.handOverSentMs})])
                root.handOverSentMs = 0
            }
            if (!root.videoOn) { root.earlyRestart = true; return } // audio only so far: onRenderReady turns video on
            // On a slow open, vid=auto can land after mpv picked tracks for the file (vid=no) and only sets the
            // option: the file has video, none plays. Ask once more for this file.
            const tracks = observed("track-list") || []
            if (!root.restarted && !root.videoRetried && tracks.some(t => t.type === "video")
                    && !tracks.some(t => t.type === "video" && t.selected)) {
                root.videoRetried = true
                setProperty("vid", "auto")
                command(["seek", 0, "relative+exact"])
                return
            }
            root.firstRestart()
            root.fileError = null
            root.applyResume()
        }
        // An error, not the folder playlist's redirect: a file without audio has nothing to play while vid=no.
        // Once video is on, the same can still happen on a slow open (tracks picked before vid=auto): open it once
        // more. Anything else is a file that cannot play.
        onFileEnded: (reason, error, entryId) => {
            if (reason !== "error") return
            if (!root.restarted && !root.videoOn) { root.endedEarly = true; root.videoRetried = true; return }
            if (!root.restarted && !root.videoRetried) { root.videoRetried = true; root.openFile(root.file); return }
            if (!root.measuring) root.showError(error, entryId)
        }
    }

    // The mixer (MixerController) gets mpv's tracks and hands back what to set, lavfi-complex first.
    Connections {
        target: mpv
        enabled: !root.measuring
        function onFileStarted() {
            MixerController.fileStarted()
            PictureQuality.fileStarted()
            ResumeStore.save() // the previous file's spot, in case EchoPlay does not close cleanly later
            mpv.setProperty("ab-loop-a", "no") // a part of the last clip means nothing in this one
            mpv.setProperty("ab-loop-b", "no")
        }
        function onScreenshotFinished(saved) { toast.show(saved ? I18n.strings.shot_saved : I18n.strings.shot_failed, 2000) }
        function onPropertyChanged(name, value) {
            if (name === "track-list") MixerController.setTrackList(value || [])
            else if (name === "volume" && typeof value === "number") AppState.volume = value
            else if (name === "speed" && root.speedKeyUsed) {
                root.speedKeyUsed = false
                toast.show(I18n.strings.speed_now.arg(Number((value || 1).toFixed(2))), 1500)
            }
            else if (name === "media-title") root.mediaTitle = value || ""
            else if (name === "playlist") root.playlist = value || []
            else if (name === "path") { // seek-bar previews and the silent-track check follow the playing file
                if (value) {
                    root.resumePending = true
                    root.resumeOffer = null
                    root.keepOldSpot = false
                }
                Thumbnailer.file = value || ""
                SilenceCheck.file = value || ""
                root.applyPlayerSettings() // the screenshot folder can be the video's own
            }
        }
    }
    // Only what changed: writing af or audio-device again rebuilds mpv's audio chain, a short gap in the sound.
    property var appliedOptions: ({})
    function applyPlayerSettings() {
        const options = PlayerSettings.mpvOptions(mpv.observed("path") || root.file)
        for (const name in options) {
            if (root.appliedOptions[name] === options[name]) continue
            root.appliedOptions[name] = options[name]
            mpv.setProperty(name, options[name])
        }
        // A lower maximum also pulls the volume down: mpv's volume-max only limits later changes.
        if ((mpv.observed("volume") || 0) > PlayerSettings.volumeMax) mpv.setProperty("volume", PlayerSettings.volumeMax)
        chrome.setAlwaysOnTop(PlayerSettings.alwaysOnTop)
    }
    Connections {
        target: PlayerSettings
        enabled: !root.measuring
        function onChanged() { root.applyPlayerSettings() }
    }
    Connections {
        target: MixerController
        enabled: !root.measuring
        function onOutputChanged(lavfi, aid) {
            mpv.setProperty("lavfi-complex", lavfi)
            if (aid) mpv.setProperty("aid", aid)
        }
    }

    // Where the user is in the file, for "Resume from 1:24?" next time (v1: every 5 s and on close). Not for a file
    // still opening, whose time-pos is the previous one's; not while the question is open, or the answer would be gone.
    function rememberPosition() {
        if (root.measuring || !root.restarted || root.resumeOffer) return
        const path = mpv.observed("path") || ""
        if (path === root.positionFile) ResumeStore.update(path, mpv.observed("time-pos") || 0, mpv.observed("duration") || 0, root.keepOldSpot)
    }
    Timer {
        running: !root.measuring && root.restarted
        interval: 5000
        repeat: true
        onTriggered: root.rememberPosition()
    }
    onClosing: {
        rememberPosition()
        if (!root.measuring) PlayerSettings.windowPlacement = chrome.placement()
    }

    // Picture quality: Auto steps tiers on sustained frame drops (PictureQuality).
    Timer {
        running: !root.measuring && root.restarted
        interval: PictureQuality.pollMs
        repeat: true
        onTriggered: PictureQuality.tick(mpv.observed("frame-drop-count") || 0)
    }
    Connections {
        target: PictureQuality
        enabled: !root.measuring
        function onApplyRequested(options) {
            for (const name in options) mpv.setProperty(name, options[name])
        }
    }

    WindowChrome { id: chrome }

    // v1's default keys (main.lua KEY_ACTIONS) and the mpv defaults v1 users rely on. Settings > Shortcuts rebinds
    // them by id (PlayerSettings.shortcuts holds only what the user changed).
    readonly property var actions: [
        {id: "pause", key: "Space", run: () => mpv.command(["cycle", "pause"])},
        {id: "mute", key: "M", run: () => mpv.command(["cycle", "mute"])},
        {id: "volume_up", key: "Up", run: () => root.changeVolume(5)},
        {id: "volume_down", key: "Down", run: () => root.changeVolume(-5)},
        {id: "seek_forward", key: "Right", run: () => root.seekBy(PlayerSettings.seekStep)},
        {id: "seek_back", key: "Left", run: () => root.seekBy(-PlayerSettings.seekStep)},
        {id: "seek_fine_forward", key: "Shift+Right", run: () => root.seekBy(1, true)},
        {id: "seek_fine_back", key: "Shift+Left", run: () => root.seekBy(-1, true)},
        {id: "frame_next", key: ".", run: () => mpv.command(["frame-step"])},
        {id: "frame_previous", key: ",", run: () => mpv.command(["frame-back-step"])},
        {id: "ab_loop", key: "L", run: () => { if (ui.item) ui.item.cycleLoop() }},
        {id: "track_1", key: "Ctrl+1", run: () => MixerController.toggle(1)},
        {id: "track_2", key: "Ctrl+2", run: () => MixerController.toggle(2)},
        {id: "track_3", key: "Ctrl+3", run: () => MixerController.toggle(3)},
        {id: "fixed_speed", key: "G", run: () => root.changeSpeed(["set", "speed", String(PlayerSettings.toggledSpeed(mpv.observed("speed") || 1))])},
        {id: "speed_down", key: "S", run: () => root.changeSpeed(["add", "speed", -PlayerSettings.speedStep])},
        {id: "speed_up", key: "D", run: () => root.changeSpeed(["add", "speed", PlayerSettings.speedStep])},
        {id: "screenshot", key: "F1", run: () => mpv.screenshot("subtitles")},
        {id: "screenshot_video", key: "Shift+S", run: () => mpv.screenshot("video")},
        {id: "screenshot_window", key: "Ctrl+S", run: () => mpv.screenshot("window")},
        {id: "next", key: ">", run: () => mpv.command(["playlist-next"])},
        {id: "previous", key: "<", run: () => mpv.command(["playlist-prev"])},
        {id: "fullscreen", key: "F", run: () => chrome.toggleFullScreen()},
        {id: "always_on_top", key: "T", run: () => {
            PlayerSettings.alwaysOnTop = !PlayerSettings.alwaysOnTop
            toast.show(PlayerSettings.alwaysOnTop ? I18n.strings.on_top_on : I18n.strings.on_top_off, 1500)
        }},
    ]
    function changeVolume(step) {
        if (ui.item) ui.item.flashVolume()
        mpv.command(["add", "volume", step])
    }
    function changeSpeed(change) {
        root.speedKeyUsed = true
        mpv.command(change)
    }
    function seekBy(seconds, exact) {
        root.resumeOffer = null // a seek of their own: the question is over
        if (ui.item) ui.item.sweepArrows(seconds > 0)
        mpv.command(exact ? ["seek", seconds, "relative+exact"] : ["seek", seconds])
    }
    // Application-wide (EchoPlay has one window): a key works whichever item has focus, and the end-to-end
    // test's synthetic keys arrive even while another app holds the OS focus.
    Instantiator {
        model: root.actions
        delegate: Shortcut {
            required property var modelData
            context: Qt.ApplicationShortcut
            enabled: root.restarted && !root.fileError // with nothing open, a key would only change what the next file opens with
            sequence: PlayerSettings.shortcuts[modelData.id] || modelData.key
            onActivated: modelData.run()
        }
    }
    // 0-9 jump to that tenth of the clip, as on YouTube. Not ten more rows in Settings > Shortcuts; a digit given to
    // an action there wins.
    Instantiator {
        model: 10
        delegate: Shortcut {
            required property int index
            context: Qt.ApplicationShortcut
            enabled: root.restarted && !root.fileError && !Object.values(PlayerSettings.shortcuts).includes(String(index))
            sequence: String(index)
            onActivated: {
                root.resumeOffer = null
                mpv.command(["seek", index * 10, "absolute-percent+exact"])
            }
        }
    }
    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: "Esc"
        onActivated: {
            if (ui.item && ui.item.closeTop()) return
            if (root.visibility === Window.FullScreen) chrome.toggleFullScreen()
        }
    }

    // Bench only: log when dropped frames happen (the mpv log has no per-drop line at -v), and keep a
    // running total, since mpv resets frame-drop-count whenever the audio chain is rebuilt (mixer switch).
    property int lastDrops: 0
    property int totalDrops: 0
    function countDrops() {
        const n = mpv.observed("frame-drop-count") || 0
        if (n !== lastDrops) {
            totalDrops += n > lastDrops ? n - lastDrops : n
            mpv.command(["print-text", "DROPS " + totalDrops + " at " + mpv.observed("playback-time")])
        }
        lastDrops = n
    }
    Timer {
        running: root.benchSeconds > 0
        interval: 100
        repeat: true
        onTriggered: root.countDrops()
    }

    // Bench only (--tier-cycle): a picture tier every 2 s, Ultra, Low, Normal, and again. v1 stalled 1-2 s on
    // the first switch to a tier in a fresh process (quality.lua warms them up); the second round shows the
    // cost of a repeat switch. Logs a TIER line per switch, next to the DROPS lines.
    property int tierStep: 0
    Timer {
        running: root.tierCycle && root.restarted
        interval: 2000
        repeat: true
        onTriggered: {
            const tier = ["ultra", "low", "normal"][root.tierStep++ % 3]
            const options = PictureQuality.tierOptions(tier)
            for (const name in options) mpv.setProperty(name, options[name])
            mpv.command(["print-text", "TIER " + tier + " at " + mpv.observed("playback-time")])
        }
    }

    // Bench only (--thumb-scrub): a pointer moving over the seek bar for 1 s (a preview request every 50 ms,
    // 1% of the file apart), then resting 0.5 s (the exact seek), over and over.
    property int scrubTick: 0
    property int thumbnails: 0 // pictures the hidden player delivered
    Connections {
        target: root.thumbScrub ? Thumbnailer : null
        function onImageChanged() { root.thumbnails++ }
    }
    Timer {
        running: root.thumbScrub && root.restarted
        interval: 50
        repeat: true
        onTriggered: {
            if (root.scrubTick++ % 30 >= 20) return
            Thumbnailer.file = root.file
            Thumbnailer.request((mpv.observed("duration") || 0) * (root.scrubTick % 100) / 100)
        }
    }

    Timer {
        running: root.mixToggle && root.restarted
        interval: 2000
        repeat: true
        onTriggered: root.setMix(!root.mixOn)
    }

    // Give the paused frame time to reach the window before grabbing it.
    Timer {
        id: shotTimer
        interval: 500
        onTriggered: root.contentItem.grabToImage(r => { r.saveToFile(root.shotOut); Qt.quit() })
    }

    // Bench with glass: a bar and a popover stay open, the worst case. --no-glass stays bare, for shot-compare.
    Loader {
        id: ui
        anchors.fill: parent
        anchors.margins: chrome.inset
        active: root.restarted && (!root.measuring || root.glass)
        asynchronous: true // the video is playing by now: never stall it
        // By name, not by type: a type named here would load with Main.qml, before the first frame.
        onActiveChanged: if (active) setSource("PlayerUi.qml", {mpv: mpv, window: root, chrome: chrome, forceChrome: root.measuring,
                                                                mediaTitle: Qt.binding(() => root.mediaTitle)})
    }

    // Nothing open. Loaded by name, here and below, so a launch with a file never loads these or their imports.
    Loader {
        id: startScreen
        anchors.fill: parent
        anchors.margins: chrome.inset
        active: !root.restarted
        Component.onCompleted: if (!root.file && !root.measuring) setSource("StartScreen.qml", {chrome: chrome, window: root})
    }
    Loader {
        id: dialogs
        active: false
        source: "OpenDialogs.qml"
    }
    DropArea {
        id: drop
        property string dragged
        anchors.fill: parent
        enabled: !root.measuring
        keys: ["text/uri-list"]
        onEntered: drag => dragged = MediaFiles.displayName(drag.urls[0])
        onDropped: event => {
            root.openUrl(event.urls[0])
            event.accept(Qt.CopyAction)
        }
    }
    Loader {
        id: errorScreen
        anchors.fill: parent
        anchors.margins: chrome.inset
        active: !!root.fileError
        onActiveChanged: if (active) setSource("ErrorScreen.qml", {
            chrome: chrome, window: root,
            path: Qt.binding(() => root.fileError ? root.fileError.path : ""),
            why: Qt.binding(() => root.fileError ? root.fileError.why : "")
        })
    }
    Connections {
        target: Updater
        function onQuitRequested() { root.close() } // the setup is running and replaces the files
    }
    readonly property bool cardShown: updateNotice.active || whatsNew.active // they outlast the chrome: PlayerUi's glass stays live
    Loader { // bottom right, above the bar when there is one; not over settings, whose About has it too
        id: updateNotice
        readonly property var player: ui.item
        active: !root.measuring && !root.fileError && !root.updateDismissed && !(player && player.settingsOpen)
                && (UpdateCheck.updateAvailable || Updater.state === "downloading" || Updater.state === "installing" || Updater.state === "failed")
        x: parent.width - chrome.inset - (item ? item.width : 0) - 16
        y: (item && player ? chrome.inset + player.bar.y - item.height - 12 : parent.height - chrome.inset - (item ? item.height : 0) - 16)
        onActiveChanged: if (active) setSource("UpdateNotice.qml", {window: root, backdrop: Qt.binding(() => player ? player.backdrop : null)})
    }
    Loader { // after an update: what changed, in the same corner
        id: whatsNew
        readonly property var player: ui.item
        active: !root.measuring && !root.fileError && !updateNotice.active && !(player && player.settingsOpen)
                && UpdateCheck.whatsNew !== ""
        x: parent.width - chrome.inset - (item ? item.width : 0) - 16
        y: (item && player ? chrome.inset + player.bar.y - item.height - 12 : parent.height - chrome.inset - (item ? item.height : 0) - 16)
        onActiveChanged: if (active) setSource("WhatsNewCard.qml", {player: Qt.binding(() => player), backdrop: Qt.binding(() => player ? player.backdrop : null)})
    }
    Toast {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        y: chrome.inset + 56
    }
    Loader {
        anchors.fill: parent
        anchors.margins: chrome.inset
        active: drop.containsDrag
        onActiveChanged: if (active) setSource("DropOverlay.qml", {name: Qt.binding(() => drop.dragged), overVideo: root.restarted})
    }
    // A click anywhere but on the resume card ends the question; the click itself still reaches what is under it.
    MouseArea {
        anchors.fill: parent
        enabled: !!root.resumeOffer
        onPressed: mouse => {
            mouse.accepted = false
            const card = ui.item ? ui.item.resumeCard : null
            if (!card || !card.contains(mapToItem(card, mouse.x, mouse.y))) {
                root.resumeOffer = null
                root.questionEndedAt = Date.now()
            }
        }
    }

    Timer {
        running: root.benchSeconds > 0
        interval: root.benchSeconds * 1000
        onTriggered: {
            const keys = ["mpv-version", "current-vo", "hwdec-current", "width", "height", "display-fps",
                          "estimated-vf-fps", "playback-time", "frame-drop-count", "decoder-frame-drop-count",
                          "vo-delayed-frame-count"]
            root.countDrops()
            const r = { glass: root.glass, mix: root.mix, "frame-drop-total": root.totalDrops, thumbnails: root.thumbnails }
            for (const k of keys) r[k] = mpv.getProperty(k)
            const line = "BENCH " + JSON.stringify(r)
            mpv.command(["print-text", line])
            console.log(line)
            quitTimer.start()
        }
    }
    Timer { id: quitTimer; interval: 300; onTriggered: Qt.quit() }
}
