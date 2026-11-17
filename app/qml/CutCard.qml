import QtQuick
import EchoPlay

// Save part: bottom right, above the bar. While it saves: the file name, how far, Cancel. Then Show in folder (gone by
// itself after 15 s), or why it failed (stays until closed).
Popover {
    id: card

    readonly property var glass: Theme.tokens.glass
    readonly property var strings: I18n.strings
    readonly property bool saving: CutSaver.state === "saving"
    readonly property bool failed: CutSaver.state === "failed"

    objectName: "cutCard"
    pointed: false
    width: 340
    implicitHeight: column.height + 32

    Timer {
        running: CutSaver.state === "done"
        interval: 15000
        onTriggered: CutSaver.dismiss()
    }
    Column {
        id: column
        x: 16
        y: 16
        width: parent.width - 32
        Row {
            spacing: 12
            Rectangle {
                width: 36
                height: 36
                radius: 10
                color: Qt.alpha(card.failed ? card.glass.warning : Theme.accent, 0.20)
                Icon {
                    anchors.centerIn: parent
                    name: card.failed ? "warning" : card.saving ? "content_cut" : "check_circle"
                    size: 22
                    color: card.failed ? card.glass.warning : Theme.accent
                }
            }
            Column {
                width: column.width - 48
                spacing: 2
                Label {
                    objectName: "cutTitle"
                    width: parent.width - 20 // clear of the cross
                    wrapMode: Text.WordWrap
                    text: card.failed ? card.strings.cut_failed : card.saving ? card.strings.cut_saving : card.strings.cut_saved
                    color: card.glass.text
                    font.pixelSize: Theme.tokens.type.label
                    font.weight: Font.Bold
                }
                Label {
                    width: parent.width
                    wrapMode: card.failed ? Text.WordWrap : Text.NoWrap
                    elide: card.failed ? Text.ElideNone : Text.ElideMiddle
                    maximumLineCount: 4
                    text: card.failed ? CutSaver.error : CutSaver.output.split(/[\\/]/).pop()
                    color: card.glass.textMuted
                    font.pixelSize: Theme.tokens.type.small
                }
            }
        }
        Item { visible: card.saving; width: 1; height: 14 }
        Rectangle {
            visible: card.saving
            x: 48
            width: parent.width - x
            height: 4
            radius: 2
            color: card.glass.track
            Rectangle {
                width: parent.width * CutSaver.progress
                height: parent.height
                radius: 2
                color: Theme.accent
            }
        }
        Item { visible: !card.failed; width: 1; height: card.saving ? 10 : 14 }
        Row {
            visible: !card.failed
            x: 48
            spacing: 8
            TextButton {
                visible: card.saving
                objectName: "cutCancel"
                text: card.strings.cancel
                onClicked: CutSaver.cancel()
            }
            TextButton {
                visible: !card.saving
                objectName: "cutShowInFolder"
                style: "primary"
                padding: 16
                text: card.strings.show_in_folder
                onClicked: {
                    MediaFiles.showInFolder(CutSaver.output)
                    CutSaver.dismiss()
                }
            }
        }
    }
    IconButton {
        visible: !card.saving
        objectName: "cutClose"
        x: card.width - width - 6
        y: 6
        width: 28
        height: 28
        iconSize: 18
        icon: "close"
        tip: card.strings.close
        onClicked: CutSaver.dismiss()
    }
}
