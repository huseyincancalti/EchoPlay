import QtQuick
import EchoPlay

// The keyboard focus mark: 2 px accent, 2 px outside the control. A click never moves focus here (MouseArea
// does not take it), so only Tab shows it.
Rectangle {
    required property Item target
    property real targetRadius: 0

    visible: target.activeFocus
    anchors.fill: parent
    anchors.margins: -4
    radius: targetRadius + 4
    color: "transparent"
    border.width: 2
    border.color: Theme.accent
}
