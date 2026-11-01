import QtQuick
import EchoPlay

// A value with a smaller and a bigger button: speed and its steps.
Row {
    id: stepper

    property string text
    property string lessName
    property string moreName
    property color textColor: Theme.tokens.glass.text
    property color buttonColor: Theme.tokens.glass.control
    property int fontSize: Theme.tokens.type.label
    property real valueWidth: 52
    signal less()
    signal more()

    spacing: 6

    component StepButton: IconButton {
        implicitWidth: 32
        implicitHeight: 32
        anchors.verticalCenter: parent.verticalCenter
        iconSize: Theme.tokens.icon.sm
        color: stepper.textColor
        background: stepper.buttonColor
        hoverColor: Qt.alpha(stepper.textColor, 0.2)
    }
    StepButton {
        objectName: stepper.objectName + "Less"
        icon: "remove"
        tip: stepper.lessName
        onClicked: stepper.less()
    }
    Label {
        anchors.verticalCenter: parent.verticalCenter
        width: stepper.valueWidth
        horizontalAlignment: Text.AlignHCenter
        text: stepper.text
        color: stepper.textColor
        font.pixelSize: stepper.fontSize
        font.weight: Font.Bold
        font.features: {"tnum": 1}
    }
    StepButton {
        objectName: stepper.objectName + "More"
        icon: "add"
        tip: stepper.moreName
        onClicked: stepper.more()
    }
}
