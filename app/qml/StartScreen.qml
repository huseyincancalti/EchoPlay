import QtQuick
import EchoPlay

// Nothing open: a drop zone, Open file and Open folder. Never any history here (the maintainer's decision: no
// "Continue watching", no recent folders).
Rectangle {
    id: start

    required property WindowChrome chrome
    required property var window // Main.qml: pickFile(), pickFolder()

    objectName: "startScreen"
    color: Theme.colors.bg

    TitleBar {
        width: parent.width
        height: implicitHeight
        title: "EchoPlay"
        followsMode: true
        chrome: start.chrome
        window: start.window
    }
    DashedFrame {
        anchors.fill: parent
        anchors.leftMargin: 32
        anchors.rightMargin: 32
        anchors.topMargin: 72
        anchors.bottomMargin: 32
        color: Theme.colors.track
        fill: Qt.alpha(Theme.colors.text, 0.03)

        Column {
            anchors.centerIn: parent
            width: parent.width - 48
            spacing: 12
            Rectangle { // the logo's waves are white: on a light screen they need the dark tile
                anchors.horizontalCenter: parent.horizontalCenter
                width: 72
                height: 72
                radius: Theme.tokens.radius.lg
                color: Theme.dark ? "transparent" : Theme.tokens.glass.fill
                Image {
                    anchors.fill: parent
                    anchors.margins: Theme.dark ? 0 : 8
                    source: "qrc:/images/logo.png"
                    sourceSize: Qt.size(180, 180)
                    smooth: true
                }
            }
            Label {
                width: parent.width
                topPadding: 8
                horizontalAlignment: Text.AlignHCenter
                text: I18n.strings.start_title
                color: Theme.colors.text
                font.pixelSize: Theme.tokens.type.display
                font.weight: Font.Bold
            }
            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: I18n.strings.start_body
                color: Theme.colors.textMuted
                font.pixelSize: Theme.tokens.type.label
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                topPadding: 12
                spacing: 10
                component BigButton: TextButton {
                    implicitHeight: 44
                    radius: Theme.tokens.radius.md
                    padding: 20
                    iconSize: Theme.tokens.icon.sm
                    fontSize: Theme.tokens.type.label
                    textColor: Theme.colors.text
                    controlColor: Theme.colors.controlStrong
                }
                BigButton {
                    objectName: "openFileButton"
                    style: "primary"
                    icon: "file_open"
                    text: I18n.strings.open_file
                    onClicked: start.window.pickFile()
                }
                BigButton {
                    objectName: "openFolderButton"
                    icon: "folder_open"
                    text: I18n.strings.open_folder
                    onClicked: start.window.pickFolder()
                }
            }
        }
    }
}
