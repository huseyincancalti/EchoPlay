import QtQuick
import QtQuick.Effects
import EchoPlay

// The left and right arrow keys: three chevrons sweep across in that direction and are gone. No time text.
Item {
    id: sweep

    property bool forward: true
    property real shift: 0
    readonly property var alphas: forward ? [0.30, 0.60, 1] : [1, 0.60, 0.30]

    function play(isForward) {
        forward = isForward
        animation.restart()
    }

    objectName: "arrowSweep"
    anchors.fill: parent
    visible: animation.running

    Row {
        id: chevrons
        // Where the boards place them on a 1280 wide window.
        x: (sweep.forward ? sweep.width * 860 / 1280 : sweep.width * 280 / 1280) + sweep.shift
        anchors.verticalCenter: parent.verticalCenter
        spacing: -48
        layer.enabled: sweep.visible
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: "#8C000000"
            shadowBlur: 0.4
            shadowVerticalOffset: 2
        }
        Repeater {
            model: sweep.alphas
            Icon {
                required property real modelData
                name: sweep.forward ? "chevron_right" : "chevron_left"
                size: 88
                color: "#FFFFFF"
                opacity: modelData
            }
        }
    }
    ParallelAnimation {
        id: animation
        NumberAnimation {
            target: sweep
            property: "shift"
            from: sweep.forward ? -90 : 90
            to: sweep.forward ? 150 : -150
            duration: Theme.tokens.motion.sweep
            easing.type: Easing.OutCubic
        }
        SequentialAnimation {
            NumberAnimation {
                target: chevrons
                property: "opacity"
                from: 0
                to: 1
                duration: Theme.tokens.motion.sweep * 0.35
            }
            NumberAnimation {
                target: chevrons
                property: "opacity"
                to: 0
                duration: Theme.tokens.motion.sweep * 0.65
            }
        }
    }
}
