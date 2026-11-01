import QtQuick
import EchoPlay

// Which subtitle track shows: off, or one of the clip's own.
Popover {
    id: subtitles

    required property var ui
    readonly property var glass: Theme.tokens.glass
    readonly property real maxHeight: ui.height - ui.bar.height - 80

    objectName: "subtitlesPopover"
    backdrop: ui.backdrop
    width: 300
    implicitHeight: Math.min(maxHeight, 46 + list.contentHeight + 8)

    Label {
        x: 18
        y: 16
        text: I18n.strings.subtitles
        color: subtitles.glass.text
        font.pixelSize: 16
        font.weight: Font.Bold
    }
    Flickable {
        id: list
        y: 46
        width: parent.width
        height: subtitles.height - 46 - 8
        contentHeight: column.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height
        Column {
            id: column
            x: 8
            width: list.width - 16
            Repeater {
                model: [{id: 0, label: I18n.strings.off}].concat(subtitles.ui.subtitleTracks.map(t => ({id: t.id, label: subtitles.ui.trackLabel(t)})))
                Item {
                    id: row
                    required property var modelData
                    readonly property bool selected: modelData.id === 0 ? !(subtitles.ui.subtitleTrack > 0) : subtitles.ui.subtitleTrack === modelData.id
                    objectName: "subtitleRow-" + modelData.id
                    width: column.width
                    height: 44
                    activeFocusOnTab: true
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: modelData.label
                    Accessible.checked: selected
                    Keys.onReturnPressed: choose()
                    function choose() {
                        subtitles.ui.mpv.setProperty("sid", modelData.id === 0 ? "no" : modelData.id)
                        subtitles.ui.popover = ""
                    }
                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: subtitles.glass.controlSubtle
                        opacity: rowMouse.containsMouse ? 1 : 0
                    }
                    FocusRing { target: row; targetRadius: 10 }
                    Label {
                        x: 10
                        width: parent.width - 48
                        anchors.verticalCenter: parent.verticalCenter
                        text: row.modelData.label
                        color: subtitles.glass.text
                        font.weight: row.selected ? Font.Bold : Font.DemiBold
                    }
                    Icon {
                        visible: row.selected
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        name: "check"
                        size: Theme.tokens.icon.sm
                        color: Theme.accent
                    }
                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: row.choose()
                    }
                }
            }
        }
    }
}
