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
    title: "EchoPlay spike"

    required property string file
    required property int benchSeconds
    required property bool glass
    required property bool mix
    required property string logFile
    required property bool mixToggle
    required property real shotAt
    required property string shotOut
    required property var startup // main_ms, app_ms from C++; qml_ms, render_ms, process_ms added here

    readonly property string mixGraph: "[aid1][aid2]amix=inputs=2:normalize=0[ao]"
    property bool restarted: false
    property bool mixOn: mix

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
            start()
        }
        onRenderReady: {
            root.startup.render_ms = processAgeMs()
            if (!root.file) return
            if (root.shotAt >= 0) command(["loadfile", root.file, "replace", -1, "start=" + root.shotAt + ",pause=yes"])
            else command(["loadfile", root.file])
        }
        onPlaybackRestarted: {
            if (root.restarted) return
            root.restarted = true
            root.startup.process_ms = processAgeMs()
            command(["print-text", "STARTUP " + JSON.stringify(root.startup)])
            if (root.shotAt >= 0) shotTimer.start()
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

    component Glass: Item {
        id: glass
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
            blurMax: 64
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
            const r = { glass: root.glass, mix: root.mix }
            for (const k of keys) r[k] = mpv.getProperty(k)
            const line = "BENCH " + JSON.stringify(r)
            mpv.command(["print-text", line])
            console.log(line)
            quitTimer.start()
        }
    }
    Timer { id: quitTimer; interval: 300; onTriggered: Qt.quit() }
}
