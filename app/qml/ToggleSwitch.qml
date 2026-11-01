import QtQuick
import EchoPlay

// On/off. Mixer rows use 38 x 22 with a 16 px knob, settings 44 x 26 with a 20 px knob.
Item {
    id: toggle

    property bool checked: false
    property string name
    property color offColor: Theme.tokens.glass.trackOff
    signal toggled()

    implicitWidth: 38
    implicitHeight: 22
    activeFocusOnTab: true
    Accessible.role: Accessible.CheckBox
    Accessible.name: name
    Accessible.checked: checked
    Keys.onReturnPressed: toggled()

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: toggle.checked ? Theme.accent : toggle.offColor
        Behavior on color { ColorAnimation { duration: Theme.tokens.motion.fast } }
    }
    FocusRing { target: toggle; targetRadius: toggle.height / 2 }
    Rectangle {
        width: toggle.height - 6
        height: width
        radius: width / 2
        y: 3
        x: toggle.checked ? toggle.width - width - 3 : 3
        color: "#FFFFFF"
        Behavior on x { NumberAnimation { duration: Theme.tokens.motion.fast; easing.type: Easing.OutCubic } }
    }
    MouseArea {
        anchors.fill: parent
        onClicked: toggle.toggled()
    }
}
