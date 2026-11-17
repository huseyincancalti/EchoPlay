import QtQuick
import EchoPlay
import "Format.js" as Format

// The mixer, EchoPlay's reason to exist: one row per audio track. Rows scroll past a maximum height, so ten
// tracks fit as well as two.
Popover {
    id: mixer

    required property var ui
    readonly property var glass: Theme.tokens.glass
    readonly property var strings: I18n.strings
    readonly property real maxHeight: ui.height - ui.bar.height - 80

    objectName: "mixerPopover"
    backdrop: ui.backdrop
    width: 440
    implicitHeight: Math.min(maxHeight, header.height + rows.contentHeight + footer.height)

    Item {
        id: header
        width: parent.width
        height: 46
        Label {
            x: 18
            y: 16
            text: mixer.strings.audio_tracks
            color: mixer.glass.text
            font.pixelSize: 16
            font.weight: Font.Bold
        }
        Label {
            anchors.right: parent.right
            anchors.rightMargin: 18
            y: 18
            text: mixer.strings.tracks_in_clip.arg(mixer.ui.tracks.length)
            color: mixer.glass.textMuted
            font.pixelSize: Theme.tokens.type.small
        }
    }
    Flickable {
        id: rows
        y: header.height
        width: parent.width
        height: mixer.height - header.height - footer.height
        contentHeight: column.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height
        Column {
            id: column
            width: rows.width
            Repeater {
                // By count, not by the list: every gain written while a slider drags gives a new list, and new rows
                // would drop the slider from under the held-down pointer.
                model: mixer.ui.tracks.length
                MixerRow {
                    required property int index
                    width: column.width
                    track: mixer.ui.tracks[index]
                    silent: SilenceCheck.silent.includes(track.id)
                }
            }
        }
    }
    Item {
        id: footer
        anchors.bottom: parent.bottom
        width: parent.width
        height: 64
        Rectangle { width: parent.width; height: 1; color: mixer.glass.hairline }
        Row {
            x: 18
            y: 12
            spacing: 8
            TextButton {
                objectName: "allOnButton"
                text: mixer.strings.all_on
                onClicked: MixerController.setAllEnabled(true)
            }
            TextButton {
                objectName: "muteAllButton"
                text: mixer.strings.mute_all
                style: "plain"
                onClicked: MixerController.setAllEnabled(false)
            }
        }
    }

    component MixerRow: Item {
        id: row
        required property var track
        property bool silent: false
        readonly property string name: Format.trackName(track, mixer.strings)
        // While a gain drags, the row shows the drag; the filter graph follows at a calmer pace (each write
        // rebuilds mpv's audio chain, a short gap).
        property real dragGain: track.gain
        readonly property real shownGain: gain.dragging ? dragGain : track.gain

        objectName: "mixerRow-" + track.id
        height: rowColumn.height + 1
        Rectangle { width: parent.width; height: 1; color: mixer.glass.hairline }
        Column {
            id: rowColumn
            x: 18
            y: 13
            width: parent.width - 36
            bottomPadding: 14
            Row {
                spacing: 12
                width: parent.width
                ToggleSwitch {
                    id: toggle
                    objectName: "trackSwitch"
                    anchors.verticalCenter: parent.verticalCenter
                    checked: row.track.enabled
                    name: row.name
                    onToggled: MixerController.toggle(row.track.id)
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - toggle.width - monoStereo.width - 24
                    Label {
                        width: parent.width
                        text: row.name
                        color: mixer.glass.text
                        font.pixelSize: Theme.tokens.type.label
                        font.weight: Font.Bold
                    }
                    Label {
                        width: parent.width
                        // The language too when the name is a title ("Commentary", then "Track 2 · English · Ctrl+2").
                        text: row.track.lang && row.track.lang !== row.name
                              ? mixer.strings.track_n.arg(row.track.id) + " · " + row.track.lang + " · Ctrl+" + row.track.id
                              : mixer.strings.track_sub.arg(row.track.id)
                        color: mixer.glass.textMuted
                        font.pixelSize: Theme.tokens.type.caption
                    }
                }
                Segmented {
                    id: monoStereo
                    anchors.verticalCenter: parent.verticalCenter
                    inset: 2
                    radius: 10
                    segmentHeight: 28
                    segmentPadding: 10
                    value: row.track.mono
                    options: [{value: true, label: mixer.strings.mono}, {value: false, label: mixer.strings.stereo}]
                    onChosen: value => MixerController.setMono(row.track.id, value)
                }
            }
            Item { width: 1; height: row.silent ? 10 : 0 }
            Rectangle {
                visible: row.silent
                x: 50
                width: parent.width - 50
                height: 34
                radius: 10
                color: Qt.alpha(mixer.glass.warning, 0.12)
                Row {
                    x: 10
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: "warning"
                        filled: true
                        size: 18
                        color: mixer.glass.warning
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: mixer.strings.track_silent.arg(row.name)
                        color: mixer.glass.warning
                        font.pixelSize: Theme.tokens.type.small
                        font.weight: Font.DemiBold
                    }
                }
            }
            Item { width: 1; height: 6 }
            Row {
                x: 50
                width: parent.width - 50
                spacing: 12
                opacity: row.track.enabled && !row.silent ? 1 : 0.45
                TrackSlider {
                    id: gain
                    objectName: "trackGain"
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - gainLabel.width - 12
                    from: 0
                    to: MixerController.gainMax
                    value: row.shownGain
                    stepSize: 0.01
                    mark: 1
                    name: row.name
                    fill: row.track.enabled ? Theme.accent : mixer.glass.mark
                    onMoved: value => {
                        // A soft stop at 100%: the mark is where most people want to land.
                        row.dragGain = Math.abs(value - 1) < 0.04 ? 1 : value
                        if (!throttle.running) throttle.start()
                    }
                    onReleased: {
                        throttle.stop()
                        MixerController.setGain(row.track.id, row.dragGain)
                    }
                    Timer {
                        id: throttle
                        interval: 250
                        onTriggered: MixerController.setGain(row.track.id, row.dragGain)
                    }
                }
                Label {
                    id: gainLabel
                    anchors.verticalCenter: parent.verticalCenter
                    width: 46
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(row.shownGain * 100) + "%"
                    color: mixer.glass.text
                    font.weight: Font.Bold
                    font.features: {"tnum": 1}
                }
            }
        }
    }
}
