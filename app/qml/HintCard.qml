import QtQuick
import EchoPlay

// The first clip with more than one track: where the mixer is. Once, never again after Got it.
Popover {
    id: card

    required property var ui
    readonly property var glass: Theme.tokens.glass

    objectName: "hintCard"
    backdrop: ui.backdrop
    width: 320
    implicitHeight: column.height + 32
    fillOpacity: 0.88
    borderColor: Qt.alpha(Theme.accent, 0.60)

    Column {
        id: column
        x: 16
        y: 16
        width: parent.width - 32
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: I18n.strings.hint_title.arg(card.ui.tracks.length)
            color: card.glass.text
            font.pixelSize: Theme.tokens.type.label
            font.weight: Font.Bold
        }
        Label {
            width: parent.width
            topPadding: 4
            wrapMode: Text.WordWrap
            lineHeight: 19
            lineHeightMode: Text.FixedHeight
            text: I18n.strings.hint_body
            color: card.glass.textMuted
            font.pixelSize: Theme.tokens.type.small
        }
        Item { width: 1; height: 14 }
        TextButton {
            objectName: "hintGotIt"
            style: "primary"
            padding: 16
            text: I18n.strings.got_it
            onClicked: PlayerSettings.firstRunHintDone = true
        }
    }
}
