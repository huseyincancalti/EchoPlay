import QtQuick
import EchoPlay

// A file that cannot play: what happened, the whole report from the start, and what to do about it. Solving it comes
// first (Report a problem, then Copy details); the ways out (next clip, another file) are plain. EchoPlay sends nothing
// itself.
Rectangle {
    id: screen

    required property WindowChrome chrome
    required property var window  // Main.qml
    required property string path
    required property string why  // mpv's error text
    readonly property var colors: Theme.colors
    readonly property var strings: I18n.strings
    readonly property string kind: ErrorReport.kind(why)
    property int stamp // bumped once: mpv writes its last line about a failure just after it reports the failure
    readonly property string report: { stamp; return window.errorReportText() }
    property bool copied

    objectName: "errorScreen"
    color: colors.bg

    Timer {
        running: true
        interval: 250
        onTriggered: screen.stamp++
    }
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
            width: Math.min(760, screen.width - 32)
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
                Rectangle { // the whole report, as it is copied
                    width: parent.width
                    height: mono.height + 24
                    radius: 10
                    color: Qt.alpha(screen.colors.text, 0.06)
                    Text {
                        id: mono
                        objectName: "errorText"
                        x: 14
                        y: 12
                        width: parent.width - 2 * x
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                        text: screen.report
                        color: screen.colors.textMuted
                        font.family: "Consolas"
                        font.pixelSize: Theme.tokens.type.caption
                        lineHeight: 18
                        lineHeightMode: Text.FixedHeight
                    }
                }
                Item { width: 1; height: 20 }
                Flow {
                    width: parent.width
                    spacing: 8
                    TextButton {
                        objectName: "errorReport"
                        implicitHeight: 40
                        padding: 16
                        style: "primary"
                        icon: "flag"
                        iconSize: Theme.tokens.icon.sm
                        text: screen.strings.report_problem
                        onClicked: Qt.openUrlExternally(ErrorReport.issueUrl(screen.window.errorReportText()))
                    }
                    TextButton {
                        objectName: "errorCopy"
                        implicitHeight: 40
                        padding: 16
                        icon: screen.copied ? "check" : "content_copy"
                        iconSize: Theme.tokens.icon.sm
                        text: screen.copied ? screen.strings.copied : screen.strings.copy_details
                        textColor: screen.colors.text
                        controlColor: screen.copied ? Qt.alpha(screen.colors.success, 0.14) : screen.colors.controlStrong
                        onClicked: {
                            ErrorReport.copy(screen.window.errorReportText())
                            screen.copied = true
                        }
                    }
                    TextButton {
                        visible: screen.window.nextClip() >= 0
                        objectName: "errorNext"
                        implicitHeight: 40
                        padding: 16
                        style: "plain"
                        icon: "skip_next"
                        iconFilled: true
                        iconSize: Theme.tokens.icon.sm
                        text: screen.strings.play_next_clip
                        fontWeight: Font.DemiBold
                        textColor: screen.colors.text
                        onClicked: screen.window.playNextClip()
                    }
                    TextButton {
                        objectName: "errorOpen"
                        implicitHeight: 40
                        padding: 16
                        style: "plain"
                        icon: "file_open"
                        iconSize: Theme.tokens.icon.sm
                        text: screen.strings.open_another
                        fontWeight: Font.DemiBold
                        textColor: screen.colors.text
                        onClicked: screen.window.pickFile()
                    }
                }
                Label {
                    width: parent.width
                    topPadding: 14
                    text: screen.strings.sends_nothing
                    color: screen.colors.textMuted
                    font.pixelSize: Theme.tokens.type.caption
                }
            }
        }
    }
}
