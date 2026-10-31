import QtQuick
import QtQuick.Effects
import EchoPlay

// Spike: video through libmpv inside the scene, with blurred glass panels sampling it live.
Window {
    id: root
    width: 1280
    height: 720
    visible: true
    color: "black"
    title: mediaTitle || "EchoPlay"

    required property string file
    required property int benchSeconds
    required property bool glass
    required property bool mix
    required property string logFile
    required property bool mixToggle
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
    property bool mixOn: mix
    property real handOverSentMs: 0 // epoch ms of a second launch whose file has not started playing yet
    property string mediaTitle: ""
    // Bench and screenshot runs keep the spike's fixed setup. Everything that makes the dev build a usable
    // player (v1's mixer, mpv options, volume memory) stays out of them, so the numbers stay comparable.
    readonly property bool measuring: benchSeconds > 0 || shotAt >= 0 || mix || mixToggle

    // A second launch hands its file here (SingleInstance). An empty file only brings the window forward.
    Connections {
        target: root.instance
        function onFileRequested(file, sentMs) {
            if (file) {
                root.handOverSentMs = sentMs
                // Before the render context exists a file cannot be loaded: onRenderReady loads root.file.
                if (root.startup.render_ms === undefined) root.file = file
                else mpv.command(["loadfile", file, "replace"])
            }
            root.show()
            root.raise()
            root.requestActivate()
        }
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
                setOption("keep-open", "always")         // v1's default end mode: stop on the last frame
                setOption("volume-max", "300")
                setOption("volume", String(AppState.volume))
                setOption("af", "dynaudnorm=p=0.9:m=25:g=15") // v1's loudness levelling
                setOption("screenshot-format", "png")
                setOption("screenshot-directory", "~~desktop/")
                setOption("cache", "auto")
                setOption("demuxer-max-bytes", "256MiB")
            }
            start()
            if (!root.measuring) {
                observeProperty("track-list")
                observeProperty("volume")
                observeProperty("media-title")
            }
        }
        onRenderReady: {
            root.startup.render_ms = processAgeMs()
            if (!root.file) return
            // Opened paused: mpv dropped the first ~8 frames when the clock ran while Qt was still building
            // its first frames. Playback starts once the first video frame is on screen (onFrameSwapped).
            const start = root.shotAt >= 0 ? "start=" + root.shotAt + "," : ""
            command(["loadfile", root.file, "replace", -1, start + "pause=yes"])
        }
        onPlaybackRestarted: {
            if (root.handOverSentMs) {
                command(["print-text", "SECOND_OPEN " + JSON.stringify({ms: Date.now() - root.handOverSentMs})])
                root.handOverSentMs = 0
            }
            if (root.restarted) return
            root.restarted = true
            root.startup.process_ms = processAgeMs()
            command(["print-text", "STARTUP " + JSON.stringify(root.startup)])
            if (root.shotAt >= 0) shotTimer.start()
            else root.unpauseOnSwap = true
        }
    }

    // The mixer (MixerController) gets mpv's tracks and hands back what to set, lavfi-complex first.
    Connections {
        target: mpv
        enabled: !root.measuring
        function onFileStarted() { MixerController.fileStarted() }
        function onPropertyChanged(name, value) {
            if (name === "track-list") MixerController.setTrackList(value || [])
            else if (name === "volume" && typeof value === "number") AppState.volume = value
            else if (name === "media-title") root.mediaTitle = value || ""
        }
    }
    Connections {
        target: MixerController
        enabled: !root.measuring
        function onOutputChanged(lavfi, aid) {
            mpv.setProperty("lavfi-complex", lavfi)
            if (aid) mpv.setProperty("aid", aid)
        }
    }

    // v1's default keys (main.lua KEY_ACTIONS) and the mpv defaults v1 users rely on. Rebinding comes with
    // the settings UI.
    function toggleFullScreen() {
        root.visibility = root.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen
    }
    Shortcut { sequence: "Space"; onActivated: mpv.command(["cycle", "pause"]) }
    Shortcut { sequence: "M"; onActivated: mpv.command(["cycle", "mute"]) }
    Shortcut { sequence: "Up"; onActivated: mpv.command(["add", "volume", 5]) }
    Shortcut { sequence: "Down"; onActivated: mpv.command(["add", "volume", -5]) }
    Shortcut { sequence: "Right"; onActivated: mpv.command(["seek", 10]) }
    Shortcut { sequence: "Left"; onActivated: mpv.command(["seek", -10]) }
    Shortcut { sequence: "Ctrl+1"; onActivated: MixerController.toggle(1) }
    Shortcut { sequence: "Ctrl+2"; onActivated: MixerController.toggle(2) }
    Shortcut { sequence: "Ctrl+3"; onActivated: MixerController.toggle(3) }
    Shortcut { sequence: "G"; onActivated: mpv.setProperty("speed", (mpv.getProperty("speed") || 1) > 1.01 ? 1 : 2) }
    Shortcut { sequence: "S"; onActivated: mpv.command(["add", "speed", -0.1]) }
    Shortcut { sequence: "D"; onActivated: mpv.command(["add", "speed", 0.1]) }
    Shortcut { sequence: "F1"; onActivated: mpv.command(["screenshot"]) }
    Shortcut { sequence: "Shift+S"; onActivated: mpv.command(["screenshot", "video"]) }
    Shortcut { sequence: "Ctrl+S"; onActivated: mpv.command(["screenshot", "window"]) }
    Shortcut { sequence: ">"; onActivated: mpv.command(["playlist-next"]) }
    Shortcut { sequence: "<"; onActivated: mpv.command(["playlist-prev"]) }
    Shortcut { sequence: "F"; onActivated: root.toggleFullScreen() }
    Shortcut { sequence: "Esc"; onActivated: if (root.visibility === Window.FullScreen) root.visibility = Window.Windowed }
    MouseArea {
        anchors.fill: parent
        onDoubleClicked: root.toggleFullScreen()
        onWheel: wheel => mpv.command(["add", "volume", wheel.angleDelta.y > 0 ? 5 : -5])
    }

    // Bench only: log when dropped frames happen (the mpv log has no per-drop line at -v), and keep a
    // running total, since mpv resets frame-drop-count whenever the audio chain is rebuilt (mixer switch).
    property int lastDrops: 0
    property int totalDrops: 0
    function countDrops() {
        const n = mpv.getProperty("frame-drop-count") || 0
        if (n !== lastDrops) {
            totalDrops += n > lastDrops ? n - lastDrops : n
            mpv.command(["print-text", "DROPS " + totalDrops + " at " + mpv.getProperty("playback-time")])
        }
        lastDrops = n
    }
    Timer {
        running: root.benchSeconds > 0
        interval: 100
        repeat: true
        onTriggered: root.countDrops()
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

    component Glass: Item {
        id: glass
        property int blurMax: 64
        visible: root.glass
        ShaderEffectSource {
            id: behind
            anchors.fill: parent
            visible: false
            live: true
            sourceItem: mpv
            sourceRect: Qt.rect(glass.x, glass.y, glass.width, glass.height)
        }
        MultiEffect {
            anchors.fill: parent
            source: behind
            blurEnabled: true
            blur: 1.0
            blurMax: glass.blurMax
        }
        Rectangle {
            anchors.fill: parent
            radius: 16
            color: "#59141210"
            border.color: "#29ffffff"
        }
    }

    // Control bar and a popover-sized panel: the two glass surfaces v2 shows most.
    Glass {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 24 }
        height: 76
        // MultiEffect left this 76 px strip completely unblurred at blurMax 64 (checked in a screenshot).
        // MVP: blur the whole frame once and let each panel crop its region, instead of per-panel blurs.
        blurMax: 24
    }
    Glass {
        width: 420
        height: 260
        anchors { right: parent.right; bottom: parent.bottom; rightMargin: 120; bottomMargin: 120 }
    }

    Timer {
        running: root.benchSeconds > 0
        interval: root.benchSeconds * 1000
        onTriggered: {
            const keys = ["mpv-version", "current-vo", "hwdec-current", "width", "height", "display-fps",
                          "estimated-vf-fps", "playback-time", "frame-drop-count", "decoder-frame-drop-count",
                          "vo-delayed-frame-count"]
            root.countDrops()
            const r = { glass: root.glass, mix: root.mix, "frame-drop-total": root.totalDrops }
            for (const k of keys) r[k] = mpv.getProperty(k)
            const line = "BENCH " + JSON.stringify(r)
            mpv.command(["print-text", line])
            console.log(line)
            quitTimer.start()
        }
    }
    Timer { id: quitTimer; interval: 300; onTriggered: Qt.quit() }
}
