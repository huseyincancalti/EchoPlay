import QtQuick
import EchoPlay

// The bar's volume slider. It never changes size and never moves: 0-100% takes 84 of its 128 px, the boost zone
// the rest, a line marks 100%, and past 100% only the color changes. Dragging stays on 100% for a few pixels
// past the line, so nobody passes it by accident.
Item {
    id: slider

    required property real volume
    required property real maxVolume
    required property color color
    readonly property bool dragging: mouse.pressed
    signal moved(real volume)

    width: 128
    implicitHeight: 44
    objectName: "volumeSlider"

    readonly property int stickPixels: 4
    // At a maximum of 300% the full zone; a lower maximum gives 100% more of the slider; 100% has no zone.
    readonly property real baseWidth: maxVolume <= 100 ? width : 84 + 44 * (300 - maxVolume) / 200
    readonly property real zoneWidth: width - baseWidth

    function volumeAt(x) {
        if (x <= baseWidth) return Math.round(100 * Math.max(0, x) / baseWidth)
        if (x <= baseWidth + stickPixels || zoneWidth <= stickPixels) return 100
        const f = Math.min(1, (x - baseWidth - stickPixels) / (zoneWidth - stickPixels))
        return Math.round(100 + f * (maxVolume - 100))
    }

    Item {
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: 4
        Rectangle { // 0-100%
            width: slider.zoneWidth > 0 ? slider.baseWidth + 2 : slider.baseWidth
            height: 4
            radius: 2
            color: Theme.tokens.glass.trackOff
        }
        Rectangle { // the boost zone
            visible: slider.zoneWidth > 0
            x: slider.baseWidth
            width: slider.zoneWidth
            height: 4
            radius: 2
            color: Qt.alpha(Theme.accent, 0.30)
        }
        Rectangle { // the level up to 100%
            width: Math.min(slider.volume, 100) / 100 * slider.baseWidth
            height: 4
            radius: 2
            color: Theme.tokens.glass.text
        }
        Rectangle { // the level past 100%
            visible: slider.volume > 100 && slider.maxVolume > 100
            x: slider.baseWidth
            width: (slider.volume - 100) / (slider.maxVolume - 100) * slider.zoneWidth
            height: 4
            radius: 2
            color: slider.color
        }
        Rectangle { // the line at 100%
            visible: slider.zoneWidth > 0
            x: slider.baseWidth - 1
            y: -4
            width: 2
            height: 12
            radius: 1
            color: Theme.tokens.glass.line
        }
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        preventStealing: true
        onPressed: slider.moved(slider.volumeAt(mouseX))
        onPositionChanged: if (pressed) slider.moved(slider.volumeAt(mouseX))
    }
}
