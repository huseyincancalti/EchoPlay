import QtQuick
import EchoPlay

// After an update: what changed in this version, in the corner where the update was offered. Once: the cross ends it,
// and over a clip so do 15 s without the pointer on it, so it never sits on the picture for a whole clip.
Popover {
    id: card

    property var player: null // PlayerUi while a clip is up; on the start screen it waits for the cross
    property int closeAfter: 15000
    readonly property var glass: Theme.tokens.glass

    objectName: "whatsNewCard"
    pointed: false
    width: 340
    implicitHeight: column.height + 32

    HoverHandler { id: hover }
    Timer {
        running: !!card.player && !hover.hovered
        interval: card.closeAfter
        onTriggered: UpdateCheck.closeWhatsNew()
    }
    Column {
        id: column
        x: 16
        y: 16
        width: parent.width - 32
        spacing: 10
        Label {
            width: parent.width - 20 // clear of the cross
            wrapMode: Text.WordWrap
            text: I18n.strings.whats_new.arg(UpdateCheck.currentVersion)
            color: card.glass.text
            font.pixelSize: Theme.tokens.type.label
            font.weight: Font.Bold
        }
        Flickable {
            width: parent.width
            height: Math.min(notes.implicitHeight, 200)
            contentHeight: notes.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            Label {
                id: notes
                objectName: "whatsNewText"
                width: parent.width
                wrapMode: Text.WordWrap
                textFormat: Text.MarkdownText
                text: UpdateCheck.whatsNew
                color: card.glass.textMuted
                font.pixelSize: Theme.tokens.type.small
                onLinkActivated: link => Qt.openUrlExternally(link)
            }
        }
    }
    IconButton {
        objectName: "whatsNewClose"
        x: card.width - width - 6
        y: 6
        width: 28
        height: 28
        iconSize: 18
        icon: "close"
        tip: I18n.strings.close
        onClicked: UpdateCheck.closeWhatsNew()
    }
}
