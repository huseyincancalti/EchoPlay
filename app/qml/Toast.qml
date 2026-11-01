import QtQuick
import EchoPlay

// One short sentence at the top that goes away by itself.
Rectangle {
    id: toast

    function show(message) {
        words.text = message
        visible = true
        hide.restart()
    }

    objectName: "toast"
    visible: false
    width: words.implicitWidth + 32
    height: words.implicitHeight + 20
    radius: height / 2
    color: Qt.alpha(Theme.tokens.glass.fill, Theme.popoverOpacity)
    border.width: 1
    border.color: Theme.tokens.glass.border
    Label {
        id: words
        anchors.centerIn: parent
        color: Theme.tokens.glass.text
        font.weight: Font.DemiBold
    }
    Timer {
        id: hide
        interval: 4000
        onTriggered: toast.visible = false
    }
}
