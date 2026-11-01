import QtQuick
import EchoPlay

// Settings over the video, which keeps playing: categories and search on the left, the rows on the right.
Item {
    id: settings

    required property var ui
    property string tab: "audio"
    readonly property string search: searchInput.text.trim().toLowerCase()
    readonly property var colors: Theme.colors
    readonly property var strings: I18n.strings
    readonly property var tabs: [{id: "audio", icon: "graphic_eq"}, {id: "playback", icon: "play_circle"}, {id: "picture", icon: "hd"},
                                 {id: "appearance", icon: "palette"}, {id: "language", icon: "translate"},
                                 {id: "screenshots", icon: "photo_camera"}, {id: "shortcuts", icon: "keyboard"}, {id: "about", icon: "info"}]

    objectName: "settingsOverlay"

    // The top thing closes first.
    function closeTop() {
        if (listLayer.options.length > 0 || pickerLayer.visible) { closeLayers(); return true }
        if (content.capturing) { content.capturing = ""; return true }
        ui.settingsOpen = false
        return true
    }
    function closeLayers() {
        listLayer.options = []
        pickerLayer.visible = false
    }

    // Dropdown's list, under its button; above it when there is no room below.
    function showList(anchor, options, value, chosen) {
        const at = anchor.mapToItem(settings, 0, 0)
        const height = Math.min(listBox.maxHeight, options.length * 36 + 8)
        listBox.x = Math.min(at.x, settings.width - anchor.width - 8)
        listBox.width = anchor.width
        listBox.y = at.y + anchor.height + 4 + height <= settings.height - 8 ? at.y + anchor.height + 4 : Math.max(8, at.y - height - 4)
        listLayer.value = value
        listLayer.chosen = chosen
        listLayer.options = options
    }
    function showColorPicker(anchor) {
        const at = anchor.mapToItem(settings, 0, 0)
        pickerBox.active = false
        pickerBox.active = true
        pickerBox.x = Math.max(8, Math.min(at.x + anchor.width - pickerBox.item.width, settings.width - pickerBox.item.width - 8))
        pickerBox.y = Math.max(8, Math.min(at.y + anchor.height + 6, settings.height - pickerBox.item.height - 8))
        pickerLayer.visible = true
    }

    MouseArea { // the dimmed video: a click closes settings, the wheel changes nothing
        anchors.fill: parent
        onClicked: settings.ui.settingsOpen = false
        onWheel: wheel => wheel.accepted = true
        Rectangle {
            anchors.fill: parent
            color: Theme.tokens.glass.overlayScrim
        }
    }
    GlassPanel {
        id: panel
        readonly property real areaTop: settings.ui.fullScreen ? 0 : 40
        backdrop: settings.ui.backdrop
        width: Math.min(1000, settings.width - 32)
        height: Math.min(600, settings.height - areaTop - 32)
        x: (settings.width - width) / 2
        y: areaTop + (settings.height - areaTop - height) / 2
        radius: Theme.tokens.radius.xl
        fill: settings.colors.panel
        fillOpacity: settings.colors.panelOpacity
        borderColor: settings.colors.border

        MouseArea { // a click on the panel takes the focus from the search field and closes nothing
            anchors.fill: parent
            onClicked: panel.forceActiveFocus()
            onWheel: wheel => wheel.accepted = true
        }
        Flickable {
            id: nav
            width: 248
            height: parent.height
            contentHeight: navColumn.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height
            Column {
                id: navColumn
                x: 14
                y: 18
                width: nav.width - 28
                height: Math.max(implicitHeight + 36, nav.height)
                spacing: 4
                Item {
                    width: parent.width
                    height: 46
                    Rectangle { // the logo's waves are white: on a light panel they need the dark tile
                        x: 6
                        y: 4
                        width: 28
                        height: 28
                        radius: 8
                        color: Theme.dark ? "transparent" : Theme.tokens.glass.fill
                        Image {
                            anchors.fill: parent
                            anchors.margins: Theme.dark ? 0 : 4
                            source: "qrc:/images/logo.png"
                            sourceSize: Qt.size(56, 56)
                            smooth: true
                        }
                    }
                    Label {
                        x: 44
                        y: 8
                        text: settings.strings.settings
                        color: settings.colors.text
                        font.pixelSize: Theme.tokens.type.title
                        font.weight: Font.Bold
                    }
                }
                Rectangle {
                    width: parent.width
                    height: 40
                    radius: 10
                    color: settings.colors.control
                    border.width: searchInput.activeFocus ? 2 : 0
                    border.color: Theme.accent
                    Icon {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        name: "search"
                        size: Theme.tokens.icon.sm
                        color: settings.colors.textMuted
                    }
                    TextInput {
                        id: searchInput
                        objectName: "settingsSearch"
                        x: 40
                        width: parent.width - x - 12
                        anchors.verticalCenter: parent.verticalCenter
                        color: settings.colors.text
                        selectionColor: Theme.accent
                        selectedTextColor: Theme.onAccent
                        selectByMouse: true
                        clip: true
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.tokens.type.body
                        activeFocusOnTab: true
                        Accessible.role: Accessible.EditableText
                        Accessible.name: settings.strings.search_settings
                        Label {
                            visible: !searchInput.text
                            anchors.verticalCenter: parent.verticalCenter
                            text: settings.strings.search_settings
                            color: settings.colors.textMuted
                        }
                    }
                }
                Item { width: 1; height: 6 }
                Repeater {
                    model: settings.tabs
                    Item {
                        id: navItem
                        required property var modelData
                        readonly property bool selected: settings.tab === modelData.id && !settings.search
                        objectName: "settingsTab-" + modelData.id
                        width: navColumn.width
                        height: 40
                        activeFocusOnTab: true
                        Accessible.role: Accessible.PageTab
                        Accessible.name: settings.strings["tab_" + modelData.id]
                        Keys.onReturnPressed: choose()
                        function choose() {
                            searchInput.text = ""
                            settings.tab = modelData.id
                            rows.contentY = 0
                        }
                        Rectangle {
                            anchors.fill: parent
                            radius: 10
                            color: navItem.selected ? settings.colors.selected : navMouse.containsMouse ? settings.colors.control : "transparent"
                        }
                        FocusRing { target: navItem; targetRadius: 10 }
                        Icon {
                            x: 12
                            anchors.verticalCenter: parent.verticalCenter
                            name: navItem.modelData.icon
                            filled: navItem.selected
                            size: Theme.tokens.icon.sm
                            color: navItem.selected ? Theme.accent : settings.colors.text
                        }
                        Label {
                            x: 44
                            anchors.verticalCenter: parent.verticalCenter
                            text: settings.strings["tab_" + navItem.modelData.id]
                            color: settings.colors.text
                            font.weight: Font.DemiBold
                        }
                        MouseArea {
                            id: navMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: navItem.choose()
                        }
                    }
                }
                Item {
                    width: 1
                    height: Math.max(8, nav.height - y - 18 - 30)
                }
                Label {
                    x: 12
                    text: settings.strings.esc_closes
                    color: settings.colors.textMuted
                    font.pixelSize: Theme.tokens.type.caption
                }
            }
        }
        Rectangle {
            x: nav.width
            width: 1
            height: parent.height
            color: settings.colors.hairline
        }
        Item {
            id: section
            x: nav.width + 1 + 32
            y: 22
            width: parent.width - x - 28
            height: parent.height - y - 24
            Label {
                anchors.verticalCenter: closeButton.verticalCenter
                width: parent.width - 56
                text: settings.search ? settings.strings.search_results.arg(searchInput.text.trim()) : settings.strings["tab_" + settings.tab]
                color: settings.colors.text
                font.pixelSize: Theme.tokens.type.heading
                font.weight: Font.Bold
            }
            IconButton {
                id: closeButton
                objectName: "settingsClose"
                anchors.right: parent.right
                implicitWidth: 40
                implicitHeight: 40
                icon: "close"
                iconSize: 22
                color: settings.colors.text
                background: settings.colors.control
                hoverColor: settings.colors.controlStrong
                tip: settings.strings.close_settings
                onClicked: settings.ui.settingsOpen = false
            }
            Flickable {
                id: rows
                objectName: "settingsRows"
                y: closeButton.height + 10
                width: parent.width
                height: parent.height - y
                contentHeight: content.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                interactive: contentHeight > height
                SettingsContent {
                    id: content
                    width: rows.width
                    overlay: settings
                    ui: settings.ui
                }
                Label {
                    visible: settings.search && content.height === 0
                    width: rows.width
                    y: 16
                    text: settings.strings.search_none
                    color: settings.colors.textMuted
                }
            }
        }
    }

    // The top layer: a dropdown's list or the color picker. A click anywhere else closes it.
    MouseArea {
        id: listLayer
        property var options: []
        property var value
        property var chosen: null
        anchors.fill: parent
        visible: options.length > 0
        onClicked: settings.closeLayers()
        onWheel: wheel => wheel.accepted = true
        Rectangle {
            id: listBox
            readonly property real maxHeight: Math.min(settings.height - 16, 320)
            height: Math.min(maxHeight, listColumn.height + 8)
            radius: 10
            color: settings.colors.surface
            border.width: 1
            border.color: settings.colors.border
            Flickable {
                x: 4
                y: 4
                width: parent.width - 8
                height: parent.height - 8
                contentHeight: listColumn.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                interactive: contentHeight > height
                Column {
                    id: listColumn
                    width: parent.width
                    Repeater {
                        model: listLayer.options
                        Item {
                            id: option
                            required property var modelData
                            readonly property bool selected: modelData.value === listLayer.value
                            objectName: "option-" + modelData.value
                            width: listColumn.width
                            height: 36
                            Rectangle {
                                anchors.fill: parent
                                radius: 7
                                color: optionMouse.containsMouse ? settings.colors.control : "transparent"
                            }
                            Label {
                                x: 10
                                width: parent.width - 42
                                anchors.verticalCenter: parent.verticalCenter
                                text: option.modelData.label
                                color: settings.colors.text
                                font.weight: option.selected ? Font.Bold : Font.Medium
                            }
                            Icon {
                                visible: option.selected
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                name: "check"
                                size: 18
                                color: Theme.accent
                            }
                            MouseArea {
                                id: optionMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    const chosen = listLayer.chosen
                                    const value = option.modelData.value
                                    settings.closeLayers()
                                    chosen(value)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    MouseArea {
        id: pickerLayer
        anchors.fill: parent
        visible: false
        onClicked: settings.closeLayers()
        onWheel: wheel => wheel.accepted = true
        Loader {
            id: pickerBox
            active: false
            sourceComponent: ColorPicker {}
        }
    }
}
