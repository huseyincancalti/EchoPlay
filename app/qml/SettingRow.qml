import QtQuick
import EchoPlay

// One setting: what it is on the left, its control on the right (below the text when the panel is narrow). It shows
// on its own tab, or anywhere while its words match the search.
Item {
    id: row

    required property var settings // SettingsOverlay: tab, search
    required property string tab
    required property string title
    property string description
    default property alias control: holder.data

    readonly property bool narrow: width < 480
    readonly property var colors: Theme.colors

    objectName: "settingRow"
    visible: settings.search ? (title + " " + description).toLowerCase().includes(settings.search) : tab === settings.tab
    width: parent ? parent.width : 0
    height: 1 + 16 + (narrow ? words.height + 12 + holder.height : Math.max(words.height, holder.height)) + 16

    Rectangle {
        width: parent.width
        height: 1
        color: row.colors.hairline
    }
    Column {
        id: words
        y: row.narrow ? 17 : (row.height + 1 - height) / 2
        width: row.narrow ? parent.width : parent.width - holder.width - 24
        spacing: 3
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: row.title
            color: row.colors.text
            font.pixelSize: Theme.tokens.type.label
            font.weight: Font.Bold
        }
        Label {
            visible: row.description !== ""
            width: parent.width
            wrapMode: Text.WordWrap
            lineHeight: 18
            lineHeightMode: Text.FixedHeight
            text: row.description
            color: row.colors.textMuted
            font.pixelSize: Theme.tokens.type.small
        }
    }
    Item {
        id: holder
        x: row.narrow ? 0 : parent.width - width
        y: row.narrow ? 17 + words.height + 12 : (row.height + 1 - height) / 2
        width: childrenRect.width
        height: childrenRect.height
    }
}
