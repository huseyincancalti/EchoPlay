import QtQuick
import EchoPlay

// A short explanation for a setting whose name alone may not say enough: the pointer resting on it for half a second
// shows it in a small box below (above, near the window's bottom). Drawn on the window itself, so no panel clips it.
Item {
    id: tip

    property string text

    HoverHandler { id: hover }
    Timer {
        id: wait
        interval: 500
        running: hover.hovered && tip.text !== ""
        onTriggered: {
            const window = tip.Window.contentItem
            const below = tip.mapToItem(window, 0, tip.height + 6)
            box.x = Math.max(8, Math.min(below.x, window.width - box.width - 8))
            box.y = below.y + box.height <= window.height - 8 ? below.y : tip.mapToItem(window, 0, -box.height - 6).y
            box.visible = true
        }
    }
    Connections {
        target: hover
        function onHoveredChanged() { if (!hover.hovered) box.visible = false }
    }
    Rectangle {
        id: box
        objectName: "hoverTip"
        parent: tip.Window.contentItem
        visible: false
        z: 1000
        width: Math.min(300, words.implicitWidth + 24)
        height: words.implicitHeight + 16
        radius: 8
        color: Qt.alpha(Theme.tokens.glass.fill, 0.96)
        border.width: 1
        border.color: Theme.tokens.glass.border
        Label {
            id: words
            x: 12
            y: 8
            width: Math.min(implicitWidth, 300 - 24)
            text: tip.text
            color: Theme.tokens.glass.text
            font.pixelSize: Theme.tokens.type.small
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }
    }
}
