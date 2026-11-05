import QtQuick
import EchoPlay

// A file dragged over the window. Over the video: the dark scrim and glass colors, in every mode. Over the start
// screen, under its title bar: the mode's own surface, which covers the text there.
Item {
    id: overlay

    required property string name // the file or folder under the pointer
    required property bool overVideo
    required property bool subtitle // added to the playing clip
    readonly property color foreground: overVideo ? Theme.tokens.glass.text : Theme.colors.text
    readonly property color muted: overVideo ? Theme.tokens.glass.textMuted : Theme.colors.textMuted

    objectName: "dropOverlay"

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: overlay.overVideo ? 0 : 40
        color: overlay.overVideo ? Theme.tokens.glass.scrim : Theme.colors.bg
    }
    DashedFrame {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 56
        anchors.bottomMargin: 16
        color: Theme.accent
        fill: Qt.alpha(Theme.accent, 0.06)

        Column {
            anchors.centerIn: parent
            width: parent.width - 48
            spacing: 10
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 72
                height: 72
                radius: 20
                color: Theme.accent
                Icon {
                    anchors.centerIn: parent
                    name: "file_open"
                    size: 36
                    color: Theme.onAccent
                }
            }
            Label {
                width: parent.width
                topPadding: 6
                horizontalAlignment: Text.AlignHCenter
                text: overlay.subtitle ? I18n.strings.drop_subtitle_title : I18n.strings.drop_title
                color: overlay.foreground
                font.pixelSize: Theme.tokens.type.display
                font.weight: Font.Bold
            }
            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideMiddle
                text: overlay.name
                color: overlay.foreground
                font.pixelSize: Theme.tokens.type.label
                font.weight: Font.DemiBold
            }
            Label {
                visible: !overlay.subtitle
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: I18n.strings.drop_note
                color: overlay.muted
            }
        }
    }
}
