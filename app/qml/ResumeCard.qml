import QtQuick
import EchoPlay
import "Format.js" as Format

// "Resume from 1:24?" while the clip already plays from the start. Unanswered, it goes after 10 s, or as soon as the
// person does something else (Main.qml, PlayerUi.qml); the saved spot stays either way.
Popover {
    id: card

    required property var ui
    readonly property var offer: ui.window.resumeOffer
    readonly property var glass: Theme.tokens.glass
    readonly property var strings: I18n.strings

    objectName: "resumeCard"
    backdrop: ui.backdrop
    pointed: false
    width: 360
    implicitHeight: column.height + 32

    Timer {
        running: true
        interval: 10000
        onTriggered: card.ui.window.resumeOffer = null
    }
    Column {
        id: column
        x: 16
        y: 16
        width: parent.width - 32
        Row {
            spacing: 10
            Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: "history"
                size: 22
                color: card.glass.text
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: card.offer ? card.strings.resume_ask_title.arg(Format.time(card.offer.at)) : ""
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
            text: card.offer ? card.strings.resume_ask_body.arg(card.offer.savedAt.toLocaleDateString(Qt.locale(I18n.language), "d MMMM")) : ""
            color: card.glass.textMuted
            font.pixelSize: Theme.tokens.type.small
        }
        Row {
            leftPadding: 32
            topPadding: 14
            spacing: 8
            TextButton {
                objectName: "resumeButton"
                style: "primary"
                icon: "play_arrow"
                iconFilled: true
                text: card.strings.resume_button
                onClicked: {
                    const at = card.offer.at
                    card.ui.window.resumeOffer = null
                    card.ui.seekTo(at, true)
                }
            }
            TextButton {
                objectName: "startOverButton"
                icon: "replay"
                text: card.strings.start_over
                onClicked: {
                    card.ui.window.keepOldSpot = false
                    card.ui.window.resumeOffer = null
                }
            }
        }
    }
}
