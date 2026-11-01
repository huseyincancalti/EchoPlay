import QtQuick
import EchoPlay

// A file that cannot play: what happened, a way out (next clip, another file), and the report (Copy details, Report a
// problem). EchoPlay sends nothing itself.
Rectangle {
    id: screen

    required property WindowChrome chrome
    required property var window  // Main.qml
    required property string path
    required property string why  // mpv's error text
    readonly property var colors: Theme.colors
    readonly property var strings: I18n.strings
    readonly property string kind: ErrorReport.kind(why)
    property string report // once Copy details was clicked; it replaces mpv's short words on the card
    readonly property bool copied: report !== ""

    function details() { report = window.errorReportText() }

    objectName: "errorScreen"
    color: colors.bg

    MouseArea { anchors.fill: parent } // nothing reaches the video below
    TitleBar {
        z: 1
        width: parent.width
        height: implicitHeight
        title: screen.path.split(/[\\/]/).pop()
        followsMode: true
        chrome: screen.chrome
        window: screen.window
    }
    Flickable { // the report can be taller than a small window
        y: 40
        width: parent.width
        height: parent.height - y
        contentHeight: Math.max(height, card.height + 32)
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height
        Rectangle {
            id: card
            objectName: "errorCard"
            x: (screen.width - width) / 2
            y: Math.max(16, (parent.height - height) / 2)
            width: Math.min(screen.copied ? 760 : 520, screen.width - 32)
            height: column.height + 56
            radius: Theme.tokens.radius.xl
            color: Qt.alpha(screen.colors.text, 0.05)
            border.width: 1
            border.color: screen.colors.border
            Column {
                id: column
                x: 28
                y: 28
                width: parent.width - 56
                Icon {
                    name: "error"
                    filled: true
                    size: 40
                    color: screen.colors.danger
                }
                Label {
                    width: parent.width
                    topPadding: 10
                    wrapMode: Text.WordWrap
                    text: screen.strings["error_" + screen.kind + "_title"]
                    color: screen.colors.text
                    font.pixelSize: Theme.tokens.type.heading
                    font.weight: Font.Bold
                }
                Label {
                    width: parent.width
                    topPadding: 8
                    wrapMode: Text.WordWrap
                    lineHeight: 21
                    lineHeightMode: Text.FixedHeight
                    text: screen.strings["error_" + screen.kind + "_body"]
                    color: screen.colors.textMuted
                }
                Item { width: 1; height: 12 }
                Rectangle { // mpv's own words; after Copy details, the whole report
                    width: parent.width
                    height: mono.height + (screen.copied ? 24 : 16)
                    radius: 10
                    color: Qt.alpha(screen.colors.text, 0.06)
                    Text {
                        id: mono
                        objectName: "errorText"
                        x: screen.copied ? 14 : 10
                        y: screen.copied ? 12 : 8
                        width: parent.width - 2 * x
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                        text: screen.copied ? screen.report : "mpv: " + screen.why
                        color: screen.colors.textMuted
                        font.family: "Consolas"
                        font.pixelSize: Theme.tokens.type.caption
                        lineHeight: screen.copied ? 18 : 16
                        lineHeightMode: Text.FixedHeight
                    }
                }
                Item { visible: !screen.copied; width: 1; height: 20 }
                Flow {
                    visible: !screen.copied
                    width: parent.width
                    spacing: 8
                    TextButton {
                        visible: screen.window.nextClip() >= 0
                        objectName: "errorNext"
                        implicitHeight: 40
                        padding: 16
                        style: "primary"
                        icon: "skip_next"
                        iconFilled: true
                        iconSize: Theme.tokens.icon.sm
                        text: screen.strings.play_next_clip
                        onClicked: screen.window.playNextClip()
                    }
                    TextButton {
                        objectName: "errorOpen"
                        implicitHeight: 40
                        padding: 16
                        icon: "file_open"
                        iconSize: Theme.tokens.icon.sm
                        text: screen.strings.open_another
                        textColor: screen.colors.text
                        controlColor: screen.colors.controlStrong
                        onClicked: screen.window.pickFile()
                    }
                }
                Item { width: 1; height: 16 }
                Rectangle { width: parent.width; height: 1; color: screen.colors.hairline }
                Item { width: 1; height: 12 }
                Item {
                    width: parent.width
                    height: 36
                    Row {
                        spacing: 4
                        TextButton {
                            objectName: "errorCopy"
                            padding: 10
                            style: screen.copied ? "control" : "plain"
                            icon: screen.copied ? "check" : "content_copy"
                            text: screen.copied ? screen.strings.copied : screen.strings.copy_details
                            fontWeight: Font.DemiBold
                            textColor: screen.colors.text
                            controlColor: Qt.alpha(screen.colors.success, 0.14)
                            onClicked: {
                                screen.details()
                                ErrorReport.copy(screen.report)
                            }
                        }
                        TextButton {
                            objectName: "errorReport"
                            padding: 10
                            style: "plain"
                            icon: "flag"
                            text: screen.strings.report_problem
                            fontWeight: Font.DemiBold
                            textColor: screen.colors.text
                            onClicked: Qt.openUrlExternally(ErrorReport.issueUrl(screen.window.errorReportText()))
                        }
                    }
                    Label {
                        visible: screen.width >= 620
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: screen.strings.sends_nothing
                        color: screen.colors.textMuted
                        font.pixelSize: Theme.tokens.type.caption
                    }
                }
            }
        }
    }
}
