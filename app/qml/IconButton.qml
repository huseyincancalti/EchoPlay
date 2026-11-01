import QtQuick
import EchoPlay

// A round icon button over the video. Plain items: no Qt Quick Controls, they cost startup time.
Item {
    id: button

    property string icon
    property bool filled: false
    property int iconSize: Theme.tokens.icon.md
    property color color: Theme.tokens.glass.text
    property color hoverColor: Theme.tokens.glass.controlSubtle
    property color background: "transparent"
    property real radius: width / 2
    property bool enabledLook: true
    property string tip
    signal clicked()

    implicitWidth: 44
    implicitHeight: 44
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: tip
    Keys.onReturnPressed: clicked()

    Rectangle {
        anchors.fill: parent
        radius: button.radius
        color: mouse.containsMouse && button.enabledLook ? button.hoverColor : button.background
        Behavior on color { ColorAnimation { duration: Theme.tokens.motion.fast } }
    }
    FocusRing { target: button; targetRadius: button.radius }
    Icon {
        anchors.centerIn: parent
        name: button.icon
        filled: button.filled
        size: button.iconSize
        color: button.color
        opacity: button.enabledLook ? 1 : 0.4
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: if (button.enabledLook) button.clicked()
    }
}
