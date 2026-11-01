import QtQuick
import EchoPlay

// One choice of many. The list opens in the settings panel's top layer (SettingsOverlay.showList): it must not be
// clipped by the scrolling rows.
Item {
    id: dropdown

    required property var settings
    property var options: [] // [{value, label}]
    property var value
    property string name
    readonly property var colors: Theme.colors
    readonly property var current: options.find(o => o.value === value)
    signal chosen(var value)

    implicitWidth: 200
    implicitHeight: 40
    activeFocusOnTab: true
    Accessible.role: Accessible.ComboBox
    Accessible.name: name
    Keys.onReturnPressed: open()

    function open() { settings.showList(dropdown, options, value, v => dropdown.chosen(v)) }

    Rectangle {
        anchors.fill: parent
        radius: 10
        color: mouse.containsMouse ? dropdown.colors.controlStrong : dropdown.colors.control
        border.width: 1
        border.color: dropdown.colors.border
    }
    FocusRing { target: dropdown; targetRadius: 10 }
    Label {
        x: 14
        width: parent.width - x - 34
        anchors.verticalCenter: parent.verticalCenter
        text: dropdown.current ? dropdown.current.label : ""
        color: dropdown.colors.text
        font.weight: Font.DemiBold
    }
    Icon {
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        name: "expand_more"
        size: Theme.tokens.icon.sm
        color: dropdown.colors.text
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: dropdown.open()
    }
}
