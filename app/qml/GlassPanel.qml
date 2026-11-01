import QtQuick
import QtQuick.Effects
import EchoPlay

// A rounded glass surface: the blurred video behind it (GlassBackdrop), the theme's fill and a 1 px border.
// Move the panel itself, not a parent: its part of the backdrop follows its own geometry.
Item {
    id: panel

    property GlassBackdrop backdrop: null
    property color fill: Theme.tokens.glass.fill
    property real fillOpacity: Theme.popoverOpacity
    property color borderColor: Theme.tokens.glass.border
    property real radius: Theme.tokens.radius.lg
    property bool shadow: true

    RectangularShadow {
        visible: panel.shadow
        anchors.fill: parent
        offset: Qt.vector2d(0, Theme.tokens.shadow.y)
        blur: Theme.tokens.shadow.blur
        radius: panel.radius
        color: Qt.rgba(0, 0, 0, Theme.tokens.shadow.opacity)
    }
    ShaderEffect {
        anchors.fill: parent
        fragmentShader: "qrc:/shaders/glass.frag.qsb"
        property var backdrop: panel.backdrop ? panel.backdrop.texture : null
        property vector4d region: {
            const b = panel.backdrop
            if (!b || b.width <= 0 || b.height <= 0) return Qt.vector4d(0, 0, 0, 0)
            panel.x; panel.y // the binding follows these and the parent's; mapToItem alone would not
            if (panel.parent) { panel.parent.x; panel.parent.y }
            const origin = panel.mapToItem(b, 0, 0)
            return Qt.vector4d(origin.x / b.width, origin.y / b.height, panel.width / b.width, panel.height / b.height)
        }
        property vector2d itemSize: Qt.vector2d(width, height)
        property real radius: panel.radius
        property real pixel: 1 / Screen.devicePixelRatio
        property real borderWidth: 1
        property color fill: Qt.alpha(panel.fill, panel.fillOpacity)
        property color border: panel.borderColor
    }
}
