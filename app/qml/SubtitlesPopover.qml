import QtQuick
import EchoPlay

// Which subtitle track shows (off, one of the clip's own, or a file added here) and how: when, how big, how high.
// Size and height are kept (PlayerSettings); the delay is for this clip only (Main.qml, onFileStarted).
Popover {
    id: subtitles

    required property var ui
    readonly property var glass: Theme.tokens.glass
    readonly property var strings: I18n.strings
    readonly property real maxHeight: ui.height - ui.bar.height - 80
    readonly property bool showing: ui.subtitleTrack > 0

    objectName: "subtitlesPopover"
    backdrop: ui.backdrop
    width: 320
    implicitHeight: Math.min(maxHeight, 50 + column.height + 10)

    component StepRow: Item {
        property alias text: caption.text
        property alias value: stepper.text
        property alias lessName: stepper.lessName
        property alias moreName: stepper.moreName
        property alias stepperName: stepper.objectName
        signal less()
        signal more()
        width: parent.width
        height: 44
        Label {
            id: caption
            x: 10
            anchors.verticalCenter: parent.verticalCenter
            color: subtitles.glass.text
            font.weight: Font.DemiBold
        }
        Stepper {
            id: stepper
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            valueWidth: 64
            onLess: parent.less()
            onMore: parent.more()
        }
    }

    Label {
        x: 18
        y: 16
        text: subtitles.strings.subtitles
        color: subtitles.glass.text
        font.pixelSize: 16
        font.weight: Font.Bold
    }
    TextButton {
        objectName: "subtitleAdd"
        anchors.right: parent.right
        anchors.rightMargin: 10
        y: 10
        implicitHeight: 32
        style: "plain"
        icon: "upload_file"
        text: subtitles.strings.add_subtitle_file
        fontSize: Theme.tokens.type.small
        onClicked: {
            const window = subtitles.ui.window
            subtitles.ui.popover = ""
            window.pickSubtitle()
        }
    }
    Flickable {
        id: list
        y: 50
        width: parent.width
        height: subtitles.height - y - 10
        contentHeight: column.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height
        Column {
            id: column
            x: 8
            width: list.width - 16
            Repeater {
                model: [{id: 0, label: subtitles.strings.off}].concat(subtitles.ui.subtitleTracks.map(t => ({id: t.id, label: subtitles.ui.trackLabel(t)})))
                Item {
                    id: row
                    required property var modelData
                    readonly property bool selected: modelData.id === 0 ? !subtitles.showing : subtitles.ui.subtitleTrack === modelData.id
                    objectName: "subtitleRow-" + modelData.id
                    width: column.width
                    height: 44
                    activeFocusOnTab: true
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: modelData.label
                    Accessible.checked: selected
                    Keys.onReturnPressed: choose()
                    // A string: mpv refuses a track id that arrives as a double (1.000000), and the id comes from JS.
                    function choose() { subtitles.ui.mpv.setProperty("sid", modelData.id === 0 ? "no" : String(modelData.id)) }
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
                        elide: Text.ElideRight
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
            Column { // how the chosen one shows
                visible: subtitles.showing
                width: parent.width
                Item { width: parent.width; height: 9
                    Rectangle { x: 2; y: 4; width: parent.width - 4; height: 1; color: subtitles.glass.hairline }
                }
                StepRow {
                    stepperName: "subtitleDelay"
                    text: subtitles.strings.subtitle_delay
                    value: (subtitles.ui.subDelay > 0.04 ? "+" : "") + subtitles.ui.subDelay.toFixed(1) + " s"
                    lessName: subtitles.strings.earlier
                    moreName: subtitles.strings.later
                    onLess: subtitles.ui.mpv.command(["add", "sub-delay", -0.1])
                    onMore: subtitles.ui.mpv.command(["add", "sub-delay", 0.1])
                }
                StepRow {
                    stepperName: "subtitleSize"
                    text: subtitles.strings.subtitle_size
                    value: Math.round(PlayerSettings.subtitleScale * 100) + "%"
                    lessName: subtitles.strings.smaller
                    moreName: subtitles.strings.bigger
                    onLess: PlayerSettings.subtitleScale -= 0.1
                    onMore: PlayerSettings.subtitleScale += 0.1
                }
                StepRow { // mpv's sub-pos: 100 is the bottom edge, lower numbers are higher up
                    stepperName: "subtitleHeight"
                    text: subtitles.strings.subtitle_height
                    value: String(100 - PlayerSettings.subtitlePosition)
                    lessName: subtitles.strings.lower
                    moreName: subtitles.strings.higher
                    onLess: PlayerSettings.subtitlePosition += 5
                    onMore: PlayerSettings.subtitlePosition -= 5
                }
            }
        }
    }
}
