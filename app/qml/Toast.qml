import QtQuick
import EchoPlay

// One short sentence at the top that goes away by itself.
Rectangle {
    id: toast

    function show(message, ms) {
        words.text = message
        visible = true
        hide.interval = ms || 4000
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
        objectName: "toastText"
        anchors.centerIn: parent
        color: Theme.tokens.glass.text
        font.weight: Font.DemiBold
    }
    Timer {
        id: hide
        onTriggered: toast.visible = false
    }
}
