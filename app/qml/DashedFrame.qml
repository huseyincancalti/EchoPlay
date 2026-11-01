import QtQuick
import QtQuick.Shapes

// A rounded dashed outline with a soft fill: where a file can be dropped. Only loaded when it is on screen.
Item {
    id: frame

    property color color
    property color fill
    property real radius: 22
    property real lineWidth: 2

    Rectangle {
        anchors.fill: parent
        radius: frame.radius
        color: frame.fill
    }
    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: frame.color
            strokeWidth: frame.lineWidth
            strokeStyle: ShapePath.DashLine
            dashPattern: [4, 3] // in line widths
            fillColor: "transparent"
            PathRectangle {
                x: frame.lineWidth / 2
                y: frame.lineWidth / 2
                width: frame.width - frame.lineWidth
                height: frame.height - frame.lineWidth
                radius: frame.radius - frame.lineWidth / 2
            }
        }
    }
}
