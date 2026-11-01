import QtQuick
import EchoPlay

// The level at the top, for the volume keys and the wheel. Past 100% it grows and jolts on every step up: the
// one place the boost may be loud. The bar's own slider never moves.
GlassPanel {
    id: flash

    required property real volume
    required property real maxVolume
    readonly property var glass: Theme.tokens.glass
    readonly property bool boosted: volume > 100
    readonly property color level: volume >= 200 ? glass.danger : boosted ? Theme.accent : glass.text
    readonly property real trackWidth: boosted ? 160 + (volume - 100) * 0.6 : 160
    readonly property bool shown: opacity > 0

    property bool armed: false
    property real previous: volume
    property real joltTime: 1
    readonly property bool strong: volume >= 200
    // 240 ms: a start, then settling (the StateBoost board's keyframes).
    readonly property var joltScale: strong ? [1.12, 1.09, 1.06, 1.03, 1.01, 1] : [1.07, 1.05, 1.03, 1.02, 1.01, 1]
    readonly property var joltX: strong ? [0, -4, 4, -3, 2, 0] : [0, -2, 2, -1, 1, 0]

    function frame(values) {
        const t = joltTime * (values.length - 1)
        const i = Math.min(values.length - 2, Math.floor(t))
        return values[i] + (values[i + 1] - values[i]) * (t - i)
    }

    // The volume keys and the wheel call this just before they change the volume.
    function request() {
        armed = true
        previous = volume
        opacity = 1
        hide.restart()
    }

    onVolumeChanged: {
        if (!armed) return
        hide.restart()
        if (volume > previous && volume > 100) {
            jolt.restart()
        }
        previous = volume
    }

    objectName: "volumeFlash"
    width: content.implicitWidth + 36
    height: 48
    radius: height / 2
    shadow: false
    fillOpacity: Theme.barOpacity
    opacity: 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: Theme.tokens.motion.fast } }
    transform: [
        Scale {
            origin.x: flash.width / 2
            origin.y: flash.height / 2
            xScale: flash.frame(flash.joltScale)
            yScale: flash.frame(flash.joltScale)
        },
        Translate { x: flash.frame(flash.joltX) }
    ]

    NumberAnimation {
        id: jolt
        target: flash
        property: "joltTime"
        from: 0
        to: 1
        duration: Theme.tokens.motion.jolt
    }
    Timer {
        id: hide
        interval: 1000
        onTriggered: {
            flash.armed = false
            flash.opacity = 0
        }
    }
    Row {
        id: content
        x: 18
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12
        Icon {
            anchors.verticalCenter: parent.verticalCenter
            name: flash.volume <= 0 ? "volume_off" : "volume_up"
            filled: flash.boosted
            color: flash.level
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: flash.trackWidth
            height: flash.boosted ? 4 + (flash.volume - 100) / 50 : 4
            radius: height / 2
            color: flash.boosted ? flash.level : flash.glass.trackOff
            Rectangle {
                width: Math.min(flash.volume, 100) * 1.6
                height: parent.height
                radius: height / 2
                color: flash.glass.text
            }
            Rectangle {
                visible: flash.maxVolume > 100
                x: 159
                y: -4
                width: 2
                height: parent.height + 8
                radius: 1
                color: flash.glass.line
            }
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            width: 50
            text: Math.round(flash.volume) + "%"
            color: flash.level
            font.pixelSize: Theme.tokens.type.label
            font.weight: Font.Bold
            font.features: {"tnum": 1}
        }
    }
}
