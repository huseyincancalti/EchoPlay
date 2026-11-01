import QtQuick
import EchoPlay

// A glass popover above its button, with a pointer at the button. It fades in; closing is immediate.
GlassPanel {
    id: popover

    default property alias content: body.data
    property real pointerX: width / 2
    property color pointerBorder: borderColor

    implicitHeight: body.implicitHeight
    opacity: 0
    Component.onCompleted: opacity = 1
    Behavior on opacity { NumberAnimation { duration: Theme.tokens.motion.base; easing.type: Easing.OutCubic } }

    MouseArea { // keeps clicks and the wheel from the video below
        anchors.fill: parent
        onWheel: wheel => wheel.accepted = true
    }
    Item {
        id: body
        anchors.fill: parent
        implicitHeight: childrenRect.height
    }
    Item { // the pointer: a rotated square, cut in half
        x: popover.pointerX - 8
        y: popover.height
        width: 16
        height: 8
        clip: true
        Rectangle {
            x: 2
            y: -6
            width: 12
            height: 12
            rotation: 45
            color: Qt.alpha(popover.fill, popover.fillOpacity)
            border.width: 1
            border.color: popover.pointerBorder
        }
    }
}
