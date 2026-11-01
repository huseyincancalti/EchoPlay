import QtQuick
import EchoPlay

// A new version: bottom right, above the bar. Every start while one exists. Install now, or it installs when EchoPlay
// closes: Install on close, the cross and 15 s all mean that. While it downloads after Install, or after a failure, it
// stays until the user answers.
Popover {
    id: notice

    required property var window // Main.qml: updateDismissed
    readonly property var glass: Theme.tokens.glass
    readonly property var strings: I18n.strings
    readonly property bool downloading: Updater.state === "downloading" || Updater.state === "installing"
    readonly property bool failed: Updater.state === "failed"
    readonly property real fraction: Updater.total > 0 ? Updater.received / Updater.total : 0

    objectName: "updateNotice"
    pointed: false
    width: 340
    implicitHeight: column.height + 32

    function install() {
        if (UpdateCheck.setupUrl) Updater.install(UpdateCheck.setupUrl, UpdateCheck.setupSha256, UpdateCheck.latestVersion)
        else Qt.openUrlExternally(UpdateCheck.releasesUrl) // the release has no setup EchoPlay can use
    }
    function later() {
        if (UpdateCheck.setupUrl) Updater.installOnClose(UpdateCheck.setupUrl, UpdateCheck.setupSha256, UpdateCheck.latestVersion)
        notice.window.updateDismissed = true
    }

    Timer {
        running: !notice.downloading && !notice.failed
        interval: 15000
        onTriggered: notice.later()
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
                color: Qt.alpha(notice.failed ? notice.glass.warning : Theme.accent, 0.20)
                Icon {
                    anchors.centerIn: parent
                    name: notice.failed ? "warning" : "download"
                    size: 22
                    color: notice.failed ? notice.glass.warning : Theme.accent
                }
            }
            Column {
                width: column.width - 48
                spacing: 2
                Label {
                    width: parent.width - 20 // clear of the cross
                    wrapMode: Text.WordWrap
                    text: notice.failed ? notice.strings.update_failed
                         : notice.downloading ? notice.strings.downloading.arg(UpdateCheck.latestVersion)
                         : notice.strings.update_ready.arg(UpdateCheck.latestVersion)
                    color: notice.glass.text
                    font.pixelSize: Theme.tokens.type.label
                    font.weight: Font.Bold
                }
                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    lineHeight: 18
                    lineHeightMode: Text.FixedHeight
                    text: notice.failed ? notice.strings.update_failed_try
                         : Updater.state === "downloading"
                           ? notice.strings.download_progress.arg(Math.round(Updater.received / 1048576)).arg(Math.round(Updater.total / 1048576))
                         : notice.strings.update_restart
                    color: notice.glass.textMuted
                    font.pixelSize: Theme.tokens.type.small
                    font.features: {"tnum": 1}
                }
            }
        }
        Item { visible: notice.downloading; width: 1; height: 14 }
        Rectangle {
            visible: notice.downloading
            x: 48
            width: parent.width - x
            height: 4
            radius: 2
            color: notice.glass.track
            Rectangle {
                width: parent.width * notice.fraction
                height: parent.height
                radius: 2
                color: Theme.accent
            }
        }
        Item { width: 1; height: notice.downloading ? 10 : 14 }
        Row {
            x: 48
            spacing: 8
            TextButton {
                visible: !notice.downloading
                objectName: "updateInstall"
                style: "primary"
                padding: 16
                text: notice.failed ? notice.strings.try_again : notice.strings.install
                onClicked: notice.install()
            }
            TextButton {
                visible: !notice.downloading
                objectName: "updateLater"
                style: "plain"
                text: UpdateCheck.setupUrl ? notice.strings.install_on_close : notice.strings.later
                onClicked: notice.later()
            }
            TextButton {
                visible: Updater.state === "downloading"
                text: notice.strings.cancel
                onClicked: Updater.cancel()
            }
        }
    }
    IconButton {
        visible: !notice.downloading
        objectName: "updateClose"
        x: notice.width - width - 6
        y: 6
        width: 28
        height: 28
        iconSize: 18
        icon: "close"
        tip: notice.strings.close
        onClicked: notice.later()
    }
}
