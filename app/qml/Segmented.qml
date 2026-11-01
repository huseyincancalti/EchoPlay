import QtQuick
import EchoPlay

// One choice of a few. `options`: [{value, label, icon (optional)}]. Equal widths when `equal`, else per label.
Item {
    id: segmented

    property var options: []
    property var value
    property bool equal: false
    property int segmentHeight: 32
    property real segmentPadding: 12
    property real inset: 3
    property real radius: 12
    property color background: Theme.tokens.glass.controlSubtle
    property color textColor: Theme.tokens.glass.text
    signal chosen(var value)

    implicitWidth: row.implicitWidth + 2 * inset
    implicitHeight: segmentHeight + 2 * inset

    Rectangle {
        anchors.fill: parent
        radius: segmented.radius
        color: segmented.background
    }
    Row {
        id: row
        x: segmented.inset
        y: segmented.inset
        Repeater {
            model: segmented.options
            Item {
                id: segment
                required property var modelData
                readonly property bool selected: modelData.value === segmented.value
                width: segmented.equal ? (segmented.width - 2 * segmented.inset) / segmented.options.length
                                       : content.implicitWidth + 2 * segmented.segmentPadding
                height: segmented.segmentHeight
                objectName: "segment-" + modelData.value
                activeFocusOnTab: true
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData.label
                Accessible.checked: selected
                Keys.onReturnPressed: segmented.chosen(modelData.value)

                Rectangle {
                    anchors.fill: parent
                    radius: segmented.radius - segmented.inset
                    color: segment.selected ? Theme.accent : Qt.alpha(segmented.textColor, mouse.containsMouse ? 0.08 : 0)
                    Behavior on color { ColorAnimation { duration: Theme.tokens.motion.fast } }
                }
                FocusRing { target: segment; targetRadius: segmented.radius - segmented.inset }
                Row {
                    id: content
                    anchors.centerIn: parent
                    spacing: 6
                    Icon {
                        visible: !!segment.modelData.icon
                        anchors.verticalCenter: parent.verticalCenter
                        name: segment.modelData.icon || ""
                        filled: segment.selected
                        size: 18
                        color: segment.selected ? Theme.onAccent : segmented.textColor
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: segment.modelData.label
                        font.pixelSize: Theme.tokens.type.small
                        font.weight: Font.Bold
                        color: segment.selected ? Theme.onAccent : segmented.textColor
                    }
                }
                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: segmented.chosen(segment.modelData.value)
                }
            }
        }
    }
}
