import QtQuick
import EchoPlay

// The picture's look. Brightness, contrast, saturation and gamma are kept (PlayerSettings); zoom, rotation and
// aspect ratio are for this clip only: the next one opens as it is (Main.qml, onFileStarted).
Popover {
    id: picture

    required property var ui
    readonly property var glass: Theme.tokens.glass
    readonly property var strings: I18n.strings
    readonly property real maxHeight: ui.height - ui.bar.height - 80
    readonly property var zoomSteps: [50, 67, 75, 90, 100, 110, 125, 150, 175, 200, 250, 300, 400] // percent
    readonly property int zoomPercent: Math.round(Math.pow(2, ui.videoZoom) * 100)
    readonly property var aspects: [{value: "-1", label: strings.aspect_auto, ratio: -1}, {value: "16:9", label: "16:9", ratio: 16 / 9},
                                    {value: "4:3", label: "4:3", ratio: 4 / 3}, {value: "21:9", label: "21:9", ratio: 21 / 9}]

    objectName: "picturePopover"
    backdrop: ui.backdrop
    width: 340
    implicitHeight: Math.min(maxHeight, 50 + column.height + 14)

    function setZoom(percent) { ui.mpv.setProperty("video-zoom", String(Math.log2(percent / 100))) }
    function stepZoom(direction) {
        const steps = direction > 0 ? zoomSteps : zoomSteps.slice().reverse()
        const next = steps.find(p => direction > 0 ? p > zoomPercent : p < zoomPercent)
        if (next !== undefined) setZoom(next)
    }
    function reset() {
        PlayerSettings.resetPicture()
        ui.window.resetGeometry()
    }

    component Caption: Label {
        color: picture.glass.text
        font.pixelSize: Theme.tokens.type.small
        font.weight: Font.DemiBold
    }

    Label {
        x: 18
        y: 16
        text: picture.strings.picture
        color: picture.glass.text
        font.pixelSize: 16
        font.weight: Font.Bold
    }
    TextButton {
        objectName: "pictureReset"
        anchors.right: parent.right
        anchors.rightMargin: 10
        y: 10
        implicitHeight: 32
        visible: picture.ui.pictureChanged
        style: "plain"
        icon: "replay"
        text: picture.strings.reset
        fontSize: Theme.tokens.type.small
        onClicked: picture.reset()
    }
    Flickable {
        id: list
        y: 50
        width: parent.width
        height: picture.height - y - 14
        contentHeight: column.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height
        Column {
            id: column
            x: 18
            width: list.width - 36
            spacing: 6
            Repeater {
                model: ["brightness", "contrast", "saturation", "gamma"]
                Item {
                    id: adjust
                    required property string modelData
                    readonly property int value: PlayerSettings.picture[modelData] || 0
                    width: column.width
                    height: 46
                    Caption { text: picture.strings[adjust.modelData] }
                    Label {
                        anchors.right: parent.right
                        text: (adjust.value > 0 ? "+" : "") + adjust.value
                        color: adjust.value !== 0 ? Theme.accent : picture.glass.textMuted
                        font.pixelSize: Theme.tokens.type.small
                        font.weight: Font.DemiBold
                        font.features: {"tnum": 1}
                    }
                    TrackSlider {
                        objectName: "picture-" + adjust.modelData
                        y: 18
                        x: 8
                        width: parent.width - 16
                        from: -100
                        to: 100
                        stepSize: 1
                        origin: 0
                        mark: 0
                        value: adjust.value
                        name: picture.strings[adjust.modelData]
                        onMoved: value => PlayerSettings.setPicture(adjust.modelData, Math.round(value))
                    }
                    MouseArea { // a double click on the name or the number: back to 0
                        width: parent.width
                        height: 18
                        onDoubleClicked: PlayerSettings.setPicture(adjust.modelData, 0)
                    }
                }
            }
            Item { width: parent.width; height: 9
                Rectangle { y: 4; width: parent.width; height: 1; color: picture.glass.hairline }
            }
            Item {
                width: parent.width
                height: 40
                Caption { anchors.verticalCenter: parent.verticalCenter; text: picture.strings.zoom }
                Stepper {
                    objectName: "pictureZoom"
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: picture.zoomPercent + "%"
                    lessName: picture.strings.zoom_out
                    moreName: picture.strings.zoom_in
                    onLess: picture.stepZoom(-1)
                    onMore: picture.stepZoom(1)
                }
            }
            Caption { topPadding: 6; text: picture.strings.rotate }
            Segmented {
                objectName: "pictureRotate"
                width: parent.width
                equal: true
                options: [0, 90, 180, 270].map(degrees => ({value: degrees, label: degrees + "°"}))
                value: picture.ui.videoRotate
                onChosen: value => picture.ui.mpv.setProperty("video-rotate", String(value))
            }
            Caption { topPadding: 6; text: picture.strings.aspect }
            Segmented {
                objectName: "pictureAspect"
                width: parent.width
                equal: true
                options: picture.aspects
                value: (picture.aspects.find(a => Math.abs(a.ratio - picture.ui.videoAspect) < 0.01) || {}).value
                onChosen: value => picture.ui.mpv.setProperty("video-aspect-override", value)
            }
        }
    }
}
