import QtQuick
import EchoPlay

// Every track off: said over the chip while the chrome is up, not over the picture all the time.
Popover {
    id: card

    required property var ui
    readonly property var glass: Theme.tokens.glass

    objectName: "noSoundCard"
    backdrop: ui.backdrop
    width: 320
    implicitHeight: column.height + 32

    Column {
        id: column
        x: 16
        y: 16
        width: parent.width - 32
        Row {
            spacing: 10
            Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: "volume_off"
                filled: true
                size: 22
                color: card.glass.warning
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: I18n.strings.no_sound_title
                color: card.glass.text
                font.pixelSize: Theme.tokens.type.label
                font.weight: Font.Bold
            }
        }
        Label {
            width: parent.width
            leftPadding: 32
            topPadding: 4
            wrapMode: Text.WordWrap
            text: I18n.strings.no_sound_body
            color: card.glass.textMuted
            font.pixelSize: Theme.tokens.type.small
        }
        Item { width: 1; height: 12 }
        TextButton {
            x: 32
            objectName: "turnAllOn"
            text: I18n.strings.turn_all_on
            onClicked: MixerController.setAllEnabled(true)
        }
    }
}
