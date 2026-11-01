import QtQuick
import EchoPlay

// A text button, optionally with an icon: primary (accent), control or plain.
Item {
    id: button

    property string text
    property string icon
    property bool iconFilled: false
    property bool iconAfter: false // after the text, as for "Open theme file"
    property color borderColor: "transparent"
    property string style: "control"   // "primary", "control", "plain"
    property color textColor: Theme.tokens.glass.text
    property color controlColor: Theme.tokens.glass.control
    property int fontSize: Theme.tokens.type.body
    property int fontWeight: Font.Bold
    property int iconSize: 18
    property real radius: 10
    property real padding: 14
    signal clicked()

    implicitWidth: row.implicitWidth + 2 * padding
    implicitHeight: 36
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: text
    Keys.onReturnPressed: clicked()

    readonly property color foreground: style === "primary" ? Theme.onAccent : textColor

    Rectangle {
        anchors.fill: parent
        radius: button.radius
        color: button.style === "primary" ? Theme.accent : button.style === "control" ? button.controlColor : "transparent"
        border.width: 1
        border.color: button.borderColor
    }
    Rectangle { // hover and press on any style
        anchors.fill: parent
        radius: button.radius
        color: Qt.alpha(button.foreground, mouse.pressed ? 0.14 : 0.08)
        opacity: mouse.containsMouse ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.tokens.motion.fast } }
    }
    FocusRing { target: button; targetRadius: button.radius }
    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6
        layoutDirection: button.iconAfter ? Qt.RightToLeft : Qt.LeftToRight
        Icon {
            visible: button.icon !== ""
            anchors.verticalCenter: parent.verticalCenter
            name: button.icon
            filled: button.iconFilled
            size: button.iconSize
            color: button.foreground
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: button.text
            color: button.foreground
            font.pixelSize: button.fontSize
            font.weight: button.fontWeight
        }
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: button.clicked()
    }
}
