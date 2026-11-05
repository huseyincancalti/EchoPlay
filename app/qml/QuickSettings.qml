import QtQuick
import EchoPlay

// The gear's popover: what people change while watching. Everything else is one click further, in Settings.
Popover {
    id: quick

    required property var ui
    readonly property var glass: Theme.tokens.glass
    readonly property var strings: I18n.strings

    objectName: "quickPopover"
    backdrop: ui.backdrop
    width: 340
    implicitHeight: column.height + 16

    component Heading: Item {
        property string icon
        property string text
        property string note
        x: 10
        width: parent.width - 20
        height: 22
        Icon {
            anchors.verticalCenter: parent.verticalCenter
            name: parent.icon
            size: 22
            color: quick.glass.text
        }
        Label {
            x: 34
            anchors.verticalCenter: parent.verticalCenter
            text: parent.text
            color: quick.glass.text
            font.pixelSize: Theme.tokens.type.label
            font.weight: Font.DemiBold
        }
        Label {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: parent.note
            color: quick.glass.textMuted
            font.pixelSize: Theme.tokens.type.small
        }
    }
    component LinkRow: Item {
        id: link
        property string icon
        property string text
        property string note
        property string trailing: "chevron_right"
        property bool available: true
        signal clicked()
        width: parent.width
        height: 48
        opacity: available ? 1 : 0.4
        activeFocusOnTab: available
        Accessible.role: Accessible.Button
        Accessible.name: text
        Keys.onReturnPressed: clicked()
        Rectangle {
            anchors.fill: parent
            radius: Theme.tokens.radius.md
            color: quick.glass.controlSubtle
            opacity: linkMouse.containsMouse && link.available ? 1 : 0
        }
        FocusRing { target: link; targetRadius: Theme.tokens.radius.md }
        Heading {
            anchors.verticalCenter: parent.verticalCenter
            icon: link.icon
            text: link.text
            note: link.note
            width: parent.width - 20 - 28
        }
        Icon {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            name: link.trailing
            size: Theme.tokens.icon.sm
            color: quick.glass.textMuted
        }
        MouseArea {
            id: linkMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: if (link.available) link.clicked()
        }
    }

    Column {
        id: column
        x: 8
        y: 8
        width: parent.width - 16

        Item {
            width: parent.width
            height: 52
            Heading {
                anchors.verticalCenter: parent.verticalCenter
                icon: "speed"
                text: quick.strings.speed
            }
            Stepper {
                objectName: "speed"
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: quick.ui.speed.toFixed(2) + "x"
                lessName: quick.strings.slower
                moreName: quick.strings.faster
                onLess: quick.ui.mpv.command(["add", "speed", -PlayerSettings.speedStep])
                onMore: quick.ui.mpv.command(["add", "speed", PlayerSettings.speedStep])
            }
        }
        Column {
            width: parent.width
            topPadding: 8
            bottomPadding: 12
            spacing: 10
            Heading {
                icon: "hd"
                text: quick.strings.picture
                note: PictureQuality.pref === "auto"
                      ? quick.strings.picture_auto_on.arg(quick.strings["tier_" + PictureQuality.active]) : ""
            }
            Segmented {
                x: 10
                width: parent.width - 20
                equal: true
                options: [{value: "auto", label: quick.strings.tier_auto}, {value: "ultra", label: quick.strings.tier_ultra},
                          {value: "normal", label: quick.strings.tier_normal}, {value: "low", label: quick.strings.tier_low}]
                value: PictureQuality.pref
                onChosen: value => PictureQuality.pref = value
            }
        }
        Column {
            width: parent.width
            topPadding: 8
            bottomPadding: 12
            spacing: 10
            Heading {
                icon: "skip_next"
                text: quick.strings.end_title
            }
            Segmented {
                x: 10
                width: parent.width - 20
                equal: true
                options: [{value: "next", label: quick.strings.end_next}, {value: "loop", label: quick.strings.end_loop},
                          {value: "stop", label: quick.strings.end_stop}]
                value: PlayerSettings.endMode
                onChosen: value => PlayerSettings.endMode = value
            }
        }
        LinkRow {
            objectName: "quickSubtitles"
            icon: "subtitles"
            text: quick.strings.subtitles
            note: quick.ui.subtitleName
            available: quick.ui.subtitleTracks.length > 0
            onClicked: quick.ui.popover = "subtitles"
        }
        LinkRow {
            objectName: "quickPicture"
            icon: "contrast"
            text: quick.strings.adjust_picture
            note: quick.ui.pictureChanged ? quick.strings.adjusted : ""
            onClicked: quick.ui.popover = "picture"
        }
        Item { width: parent.width; height: 9
            Rectangle { x: 2; y: 4; width: parent.width - 4; height: 1; color: quick.glass.hairline }
        }
        LinkRow {
            objectName: "allSettings"
            icon: "tune"
            text: quick.strings.all_settings
            trailing: "arrow_forward"
            onClicked: quick.ui.openSettings()
        }
    }
}
