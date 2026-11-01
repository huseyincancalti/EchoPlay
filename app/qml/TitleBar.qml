import QtQuick
import EchoPlay

// The title bar: EchoPlay's own over the video, Windows' caption behavior from WindowChrome (drag, Aero Snap,
// double-click, system menu, snap layouts on the maximize button).
GlassPanel {
    id: bar

    required property string title
    required property WindowChrome chrome
    required property var window
    // Dark glass over a video; with nothing playing it follows the mode.
    property bool followsMode: false
    readonly property color foreground: followsMode ? Theme.colors.text : Theme.tokens.glass.text
    readonly property bool hovered: hover.hovered

    implicitHeight: 40
    radius: 0
    shadow: false
    fill: followsMode ? Theme.colors.bg : Theme.tokens.glass.fill
    fillOpacity: followsMode ? 1 : 0.72
    borderColor: "transparent"

    HoverHandler { id: hover }
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: bar.followsMode ? Theme.colors.hairline : Theme.tokens.glass.hairline
    }
    Rectangle { // the logo's waves are white: on a light title bar they need the dark tile
        x: 12
        anchors.verticalCenter: parent.verticalCenter
        width: 22
        height: 22
        radius: 6
        color: bar.followsMode && !Theme.dark ? Theme.tokens.glass.fill : "transparent"
        Image {
            anchors.fill: parent
            anchors.margins: bar.followsMode && !Theme.dark ? 3 : 0
            source: "qrc:/images/logo.png"
            sourceSize: Qt.size(44, 44)
            smooth: true
        }
    }
    Label {
        x: 44
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - x - 3 * 46 - 12
        text: bar.title
        color: bar.foreground
        font.pixelSize: Theme.tokens.type.small
        font.weight: Font.DemiBold
    }
    Row {
        anchors.right: parent.right
        height: parent.height
        component CaptionButton: Item {
            id: button
            property string icon
            property int iconSize: 20
            property bool hot: mouse.containsMouse
            property color hotColor: Qt.alpha(bar.foreground, 0.10)
            property alias enabledMouse: mouse.enabled
            signal clicked()
            width: bar.chrome.buttonWidth
            height: bar.height
            Rectangle {
                anchors.fill: parent
                color: button.hotColor
                opacity: button.hot ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.tokens.motion.fast } }
            }
            Icon {
                anchors.centerIn: parent
                name: button.icon
                size: button.iconSize
                color: bar.foreground
            }
            MouseArea {
                id: mouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: button.clicked()
            }
        }
        CaptionButton {
            objectName: "minimizeButton"
            icon: "remove"
            onClicked: bar.chrome.minimize()
        }
        CaptionButton { // Windows owns this one's input (snap layouts): WindowChrome's hit test and hover
            icon: bar.window.visibility === Window.Maximized ? "filter_none" : "crop_square"
            iconSize: 18
            hot: bar.chrome.maximizeHovered
            enabledMouse: false
        }
        CaptionButton {
            objectName: "closeButton"
            icon: "close"
            hotColor: "#C42B1C" // Windows' own close red: not a theme color, people look for it there
            onClicked: bar.window.close()
        }
    }
}
