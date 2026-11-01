import QtQuick
import EchoPlay

// A slider with a knob, for values from `from` to `to`. An optional mark (mixer gain: 100%). `moved` while
// dragging, `released` at the end.
Item {
    id: slider

    property real from: 0
    property real to: 1
    property real value: 0
    property real stepSize: 0
    property real mark: NaN
    property string name
    property color fill: Theme.accent
    property color track: Theme.tokens.glass.trackSoft
    property color markColor: Theme.tokens.glass.mark
    property int knobSize: 16
    readonly property bool dragging: mouse.pressed
    signal moved(real value)
    signal released()

    implicitWidth: 200
    implicitHeight: 28
    activeFocusOnTab: true
    Accessible.role: Accessible.Slider
    Accessible.name: name

    readonly property real fraction: to > from ? Math.min(1, Math.max(0, (value - from) / (to - from))) : 0

    function step(direction) {
        const delta = stepSize > 0 ? stepSize : (to - from) / 20
        moved(Math.min(to, Math.max(from, value + direction * delta)))
        released()
    }
    Keys.onLeftPressed: step(-1)
    Keys.onRightPressed: step(1)

    Rectangle {
        id: bar
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: 4
        radius: 2
        color: slider.track
    }
    Rectangle {
        visible: !isNaN(slider.mark)
        anchors.verticalCenter: parent.verticalCenter
        x: slider.width * (slider.mark - slider.from) / (slider.to - slider.from) - 1
        width: 2
        height: 12
        radius: 1
        color: slider.markColor
    }
    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: slider.width * slider.fraction
        height: 4
        radius: 2
        color: slider.fill
    }
    FocusRing { target: slider; targetRadius: 8 }
    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        x: slider.width * slider.fraction - width / 2
        width: slider.knobSize
        height: width
        radius: width / 2
        color: "#FFFFFF"
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        anchors.leftMargin: -slider.knobSize / 2
        anchors.rightMargin: -slider.knobSize / 2
        preventStealing: true
        function apply() {
            const f = Math.min(1, Math.max(0, (mouseX - slider.knobSize / 2) / slider.width))
            let v = slider.from + f * (slider.to - slider.from)
            if (slider.stepSize > 0) v = Math.round(v / slider.stepSize) * slider.stepSize
            slider.moved(v)
        }
        onPressed: apply()
        onPositionChanged: if (pressed) apply()
        onReleased: slider.released()
    }
}
