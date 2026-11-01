import QtQuick
import EchoPlay

// About's update line: up to date, checking, ready to install, downloading, or failed.
Rectangle {
    id: card

    required property var ui
    readonly property var colors: Theme.colors
    readonly property var strings: I18n.strings
    readonly property bool downloading: Updater.state === "downloading" || Updater.state === "installing"
    readonly property bool failed: Updater.state === "failed"
    readonly property bool ready: UpdateCheck.updateAvailable && !downloading && !failed
    readonly property bool checked: !isNaN(UpdateCheck.lastChecked.getTime())

    objectName: "updateCard"
    height: Math.max(64, line.height + 28)
    radius: 14
    color: colors.control

    Icon {
        id: icon
        x: 16
        anchors.verticalCenter: parent.verticalCenter
        name: card.failed ? "warning" : UpdateCheck.updateAvailable ? "download" : card.checked ? "check_circle" : "info"
        filled: true
        size: 26
        color: card.failed ? card.colors.warning : UpdateCheck.updateAvailable ? Theme.accent
             : card.checked ? card.colors.success : card.colors.textMuted
    }
    Column {
        id: line
        x: 56
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - x - actions.width - 32
        spacing: 2
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: card.failed ? card.strings.update_failed
                 : card.downloading ? card.strings.downloading.arg(UpdateCheck.latestVersion)
                 : card.ready ? card.strings.update_ready.arg(UpdateCheck.latestVersion)
                 : UpdateCheck.checking ? card.strings.checking
                 : card.checked ? card.strings.up_to_date : card.strings.never_checked
            color: card.colors.text
            font.pixelSize: Theme.tokens.type.label
            font.weight: Font.Bold
        }
        Label {
            visible: card.failed || card.downloading || card.ready || card.checked
            width: parent.width
            wrapMode: Text.WordWrap
            text: card.failed ? card.strings.update_failed_try
                 : card.downloading ? card.ui.downloadText()
                 : card.ready ? (Updater.state === "ready" ? card.strings.update_on_close : card.strings.update_restart)
                 : card.strings.last_checked.arg(Qt.formatDateTime(UpdateCheck.lastChecked, "yyyy-MM-dd HH:mm"))
            color: card.colors.textMuted
            font.pixelSize: Theme.tokens.type.small
            font.features: {"tnum": 1}
        }
    }
    Row {
        id: actions
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        TextButton {
            visible: card.ready || card.failed
            objectName: "aboutInstall"
            style: "primary"
            icon: "download"
            text: card.failed ? card.strings.try_again : card.strings.install
            onClicked: card.ui.installUpdate()
        }
        TextButton {
            visible: card.downloading
            text: card.strings.cancel
            textColor: card.colors.text
            controlColor: card.colors.controlStrong
            onClicked: Updater.cancel()
        }
        TextButton {
            visible: !UpdateCheck.updateAvailable && !card.downloading && !card.failed
            objectName: "checkNow"
            icon: "refresh"
            text: card.strings.check_now
            textColor: card.colors.text
            controlColor: card.colors.controlStrong
            onClicked: UpdateCheck.checkNow()
        }
    }
}
