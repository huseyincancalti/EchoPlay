import QtQuick
import EchoPlay

// A picture with rounded corners and an optional border, through the glass shader.
Item {
    id: root

    property alias source: picture.source
    property alias sourceSize: picture.sourceSize
    property alias status: picture.status
    property real radius: Theme.tokens.radius.sm
    property color borderColor: "transparent"
    property real borderWidth: 0
    readonly property real aspect: picture.implicitHeight > 0 ? picture.implicitWidth / picture.implicitHeight : 16 / 9

    Image {
        id: picture
        visible: false
        anchors.fill: parent
        cache: false
    }
    ShaderEffect {
        anchors.fill: parent
        fragmentShader: "qrc:/shaders/glass.frag.qsb"
        property var backdrop: picture
        property vector4d region: Qt.vector4d(0, 0, 1, 1)
        property vector2d itemSize: Qt.vector2d(width, height)
        property real radius: root.radius
        property real pixel: 1 / Screen.devicePixelRatio
        property real borderWidth: root.borderWidth
        property color fill: "transparent"
        property color border: root.borderColor
    }
}
