import QtQuick
import EchoPlay

// A right-click on the video, or on a playlist row (forRow: only what concerns that clip). Delete asks first, in place.
Popover {
    id: menu

    required property var ui
    required property string path
    property bool forRow: false
    property bool confirming: false
    readonly property var glass: Theme.tokens.glass
    readonly property var strings: I18n.strings
    readonly property bool local: path !== "" && !/^[a-z][a-z0-9+.-]+:\/\//i.test(path) // a link has no folder

    objectName: "contextMenu"
    backdrop: ui.backdrop
    pointed: false
    width: 260
    implicitHeight: confirming ? confirm.y + confirm.height + 14 : rows.height + 12

    // The menu closes first: what a row does may open something else.
    function run(action) {
        ui.popover = ""
        action()
    }
    function keyOf(id) { return PlayerSettings.shortcuts[id] || ui.window.actions.find(a => a.id === id).key }

    component MenuRow: Item {
        id: row
        property string icon
        property string text
        property string key
        property bool danger: false
        property bool available: true
        property bool switched: false // a switch at the end
        property bool on: false
        signal clicked()
        width: parent.width
        height: 40
        opacity: available ? 1 : 0.4
        activeFocusOnTab: available
        Accessible.role: switched ? Accessible.CheckBox : Accessible.MenuItem
        Accessible.name: text
        Accessible.checked: on
        Keys.onReturnPressed: if (available) clicked()
        readonly property color tone: danger ? menu.glass.danger : menu.glass.text
        Rectangle {
            anchors.fill: parent
            radius: Theme.tokens.radius.md
            color: menu.glass.controlSubtle
            opacity: rowMouse.containsMouse && row.available ? 1 : 0
        }
        FocusRing { target: row; targetRadius: Theme.tokens.radius.md }
        Icon {
            x: 10
            anchors.verticalCenter: parent.verticalCenter
            name: row.icon
            filled: row.on
            size: 20
            color: row.on ? Theme.accent : row.tone
        }
        Label {
            x: 42
            width: parent.width - x - 60
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            text: row.text
            color: row.tone
            font.pixelSize: Theme.tokens.type.label
            font.weight: Font.Medium
        }
        Label {
            visible: !row.switched
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: row.key
            color: menu.glass.textMuted
            font.pixelSize: Theme.tokens.type.small
        }
        ToggleSwitch {
            visible: row.switched
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            activeFocusOnTab: false // the row takes the focus and the click
            checked: row.on
            name: row.text
        }
        MouseArea {
            id: rowMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton // as in Windows' own menus
            onClicked: if (row.available) row.clicked()
        }
    }
    component Divider: Item {
        width: parent.width
        height: 9
        Rectangle { x: 6; y: 4; width: parent.width - 12; height: 1; color: menu.glass.hairline }
    }

    MouseArea { // a right-click on the menu's own padding does not open another menu under it
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
    }
    Column {
        id: rows
        visible: !menu.confirming
        x: 6
        y: 6
        width: parent.width - 12
        MenuRow {
            objectName: "menuOpen"
            visible: !menu.forRow
            icon: "file_open"
            text: menu.strings.open_file
            onClicked: menu.run(() => menu.ui.window.pickFile())
        }
        Divider { visible: !menu.forRow }
        MenuRow {
            objectName: "menuShowInFolder"
            icon: "folder_open"
            text: menu.strings.show_in_folder
            available: menu.local
            onClicked: menu.run(() => MediaFiles.showInFolder(menu.path))
        }
        MenuRow {
            objectName: "menuInfo"
            visible: !menu.forRow
            icon: "info"
            text: menu.strings.clip_info
            key: menu.keyOf("clip_info")
            onClicked: menu.run(() => menu.ui.popover = "info")
        }
        MenuRow {
            objectName: "menuDelete"
            icon: "delete"
            text: menu.strings.delete_clip
            danger: true
            available: menu.local
            onClicked: menu.confirming = true
        }
        Divider { visible: !menu.forRow }
        MenuRow {
            objectName: "menuScreenshot"
            visible: !menu.forRow
            icon: "photo_camera"
            text: menu.strings.action_screenshot
            key: menu.keyOf("screenshot")
            onClicked: menu.run(() => menu.ui.mpv.screenshot("subtitles"))
        }
        MenuRow {
            objectName: "menuOnTop"
            visible: !menu.forRow
            icon: "push_pin"
            text: menu.strings.action_always_on_top
            switched: true
            on: PlayerSettings.alwaysOnTop
            onClicked: menu.run(() => menu.ui.window.toggleOnTop())
        }
        Divider { visible: !menu.forRow }
        MenuRow {
            objectName: "menuSettings"
            visible: !menu.forRow
            icon: "tune"
            text: menu.strings.all_settings
            onClicked: menu.run(() => menu.ui.openSettings())
        }
    }

    Column { // Delete's question, in the menu's place
        id: confirm
        objectName: "menuConfirm"
        visible: menu.confirming
        x: 16
        y: 14
        width: parent.width - 32
        spacing: 4
        Label {
            width: parent.width
            wrapMode: Text.Wrap
            text: menu.strings.trash_title
            color: menu.glass.text
            font.pixelSize: Theme.tokens.type.label
            font.weight: Font.Bold
        }
        Label {
            width: parent.width
            elide: Text.ElideMiddle
            text: menu.path.split(/[\\/]/).pop()
            color: menu.glass.textMuted
            font.pixelSize: Theme.tokens.type.small
        }
        Item { width: 1; height: 10 }
        Row {
            anchors.right: parent.right
            spacing: 8
            TextButton {
                objectName: "menuCancel"
                text: menu.strings.cancel
                style: "plain"
                onClicked: menu.run(() => {})
            }
            TextButton {
                objectName: "menuConfirmDelete"
                text: menu.strings.delete_clip
                controlColor: menu.glass.danger
                textColor: menu.glass.fill // dark on the light red, as on the accent: white read at 2.9:1
                onClicked: {
                    const window = menu.ui.window
                    const path = menu.path
                    menu.run(() => window.trashClip(path)) // the start screen may replace the whole player
                }
            }
        }
    }
}
