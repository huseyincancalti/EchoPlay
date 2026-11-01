import QtQuick
import EchoPlay

// Any accent color: saturation and brightness, hue, or a hex value. It applies while it moves.
Rectangle {
    id: picker

    readonly property var colors: Theme.colors
    property real hue: 0
    property real saturation: 1
    property real brightness: 1
    readonly property color picked: Qt.hsva(hue, saturation, brightness, 1)

    objectName: "colorPicker"
    width: 232
    height: column.height + 32
    radius: Theme.tokens.radius.md
    color: colors.surface
    border.width: 1
    border.color: colors.border

    function apply() { Theme.accentChoice = picked.toString() }
    Component.onCompleted: {
        hue = Math.max(0, Theme.accent.hsvHue)
        saturation = Theme.accent.hsvSaturation
        brightness = Theme.accent.hsvValue
    }
    MouseArea { anchors.fill: parent } // a click here is no click outside

    Column {
        id: column
        x: 16
        y: 16
        width: parent.width - 32
        spacing: 12
        Rectangle { // saturation left to right, brightness bottom to top
            width: parent.width
            height: 120
            radius: Theme.tokens.radius.sm
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "white" }
                GradientStop { position: 1; color: Qt.hsva(picker.hue, 1, 1, 1) }
            }
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                gradient: Gradient {
                    GradientStop { position: 0; color: "transparent" }
                    GradientStop { position: 1; color: "black" }
                }
            }
            Rectangle {
                x: picker.saturation * parent.width - width / 2
                y: (1 - picker.brightness) * parent.height - height / 2
                width: 14
                height: 14
                radius: 7
                color: picker.picked
                border.width: 2
                border.color: "white"
            }
            MouseArea {
                anchors.fill: parent
                preventStealing: true
                function apply() {
                    picker.saturation = Math.min(1, Math.max(0, mouseX / width))
                    picker.brightness = 1 - Math.min(1, Math.max(0, mouseY / height))
                    picker.apply()
                }
                onPressed: apply()
                onPositionChanged: if (pressed) apply()
            }
        }
        Rectangle { // hue
            width: parent.width
            height: 12
            radius: 6
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "#FF0000" }
                GradientStop { position: 0.167; color: "#FFFF00" }
                GradientStop { position: 0.333; color: "#00FF00" }
                GradientStop { position: 0.5; color: "#00FFFF" }
                GradientStop { position: 0.667; color: "#0000FF" }
                GradientStop { position: 0.833; color: "#FF00FF" }
                GradientStop { position: 1; color: "#FF0000" }
            }
            Rectangle {
                x: picker.hue * parent.width - width / 2
                anchors.verticalCenter: parent.verticalCenter
                width: 16
                height: 16
                radius: 8
                color: Qt.hsva(picker.hue, 1, 1, 1)
                border.width: 2
                border.color: "white"
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -4
                preventStealing: true
                function apply() {
                    picker.hue = Math.min(1, Math.max(0, (mouseX - 4) / (width - 8)))
                    picker.apply()
                }
                onPressed: apply()
                onPositionChanged: if (pressed) apply()
            }
        }
        Row {
            spacing: 10
            Rectangle {
                width: 32
                height: 32
                radius: 8
                color: picker.picked
                border.width: 1
                border.color: picker.colors.border
            }
            Rectangle {
                width: 120
                height: 32
                radius: 8
                color: picker.colors.control
                border.width: 1
                border.color: hex.activeFocus ? Theme.accent : picker.colors.border
                TextInput {
                    id: hex
                    objectName: "colorHex"
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    verticalAlignment: TextInput.AlignVCenter
                    color: picker.colors.text
                    selectionColor: Theme.accent
                    selectedTextColor: Theme.onAccent
                    selectByMouse: true
                    maximumLength: 7
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.tokens.type.body
                    font.features: {"tnum": 1}
                    text: picker.picked.toString().toUpperCase()
                    validator: RegularExpressionValidator { regularExpression: /#?[0-9a-fA-F]{0,6}/ }
                    onTextEdited: {
                        const value = text.startsWith("#") ? text : "#" + text
                        if (value.length !== 7) return
                        const color = Qt.color(value)
                        picker.hue = Math.max(0, color.hsvHue)
                        picker.saturation = color.hsvSaturation
                        picker.brightness = color.hsvValue
                        picker.apply()
                    }
                }
            }
        }
    }
}
