import QtQuick
import EchoPlay

// Every row of every tab, in one column: a row shows on its tab or when the search finds it (SettingRow).
Column {
    id: content

    required property var overlay
    required property var ui
    readonly property var colors: Theme.colors
    readonly property var strings: I18n.strings
    readonly property var glassLevels: [strings.level_low, strings.level_medium, strings.level_high]
    readonly property var loudnessLevels: ["low", "medium", "high"]
    property string capturing: "" // the action whose new key is awaited
    property string keyMessage    // why the key was refused, on keyMessageRow's row until the next step
    property string keyMessageRow: ""
    property bool fontFailed: false
    property var audioDevices: []
    property string engine

    function shortcutKey(action) { return PlayerSettings.shortcuts[action.id] || action.key }
    // Windows' open dialogs live in Main.qml (OpenDialogs), loaded on first use.
    function chooser() { return ui.window.chooser() }

    Component.onCompleted: {
        ui.mpv.observeProperty("audio-device-list")
        audioDevices = ui.mpv.observed("audio-device-list") || []
        ui.mpv.observeProperty("mpv-version")
        engine = ui.mpv.observed("mpv-version") || ""
    }
    Connections {
        target: content.ui.mpv
        function onPropertyChanged(name, value) {
            if (name === "audio-device-list") content.audioDevices = value || []
            else if (name === "mpv-version") content.engine = value || ""
        }
    }
    Connections {
        target: content.overlay
        function onTabChanged() { content.keyMessageRow = "" }
        function onSearchChanged() { content.keyMessageRow = "" }
    }

    component Row_: SettingRow { settings: content.overlay }
    component Seg: Segmented {
        segmentHeight: 34
        background: content.colors.control
        textColor: content.colors.text
    }
    component Drop: Dropdown {
        settings: content.overlay
        implicitWidth: 288
    }
    component Step: Stepper {
        textColor: content.colors.text
        buttonColor: content.colors.controlStrong
        fontSize: Theme.tokens.type.body
        valueWidth: 56
    }
    component Button_: TextButton {
        implicitHeight: 40
        padding: 14
        iconAfter: true
        textColor: content.colors.text
        controlColor: content.colors.control
        borderColor: content.colors.border
    }
    component Levels: Row { // a slider and what it is at
        id: levels
        property alias from: slider.from
        property alias to: slider.to
        property alias value: slider.value
        property alias stepSize: slider.stepSize
        property alias name: slider.name
        property string label
        signal moved(real value)
        spacing: 24
        TrackSlider {
            id: slider
            anchors.verticalCenter: parent.verticalCenter
            track: content.colors.track
            onMoved: value => levels.moved(value)
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            width: 64
            text: levels.label
            color: content.colors.text
            font.weight: Font.Bold
            font.features: {"tnum": 1}
        }
    }

    // Audio
    Row_ {
        tab: "audio"
        title: content.strings.loudness_title
        description: content.strings.loudness_desc
        ToggleSwitch {
            objectName: "loudnessSwitch"
            width: 44
            height: 26
            name: content.strings.loudness_title
            offColor: content.colors.track
            checked: PlayerSettings.loudnessBoost
            onToggled: PlayerSettings.loudnessBoost = !checked
        }
    }
    Row_ {
        tab: "audio"
        title: content.strings.strength_title
        description: content.strings.strength_desc
        Levels {
            opacity: PlayerSettings.loudnessBoost ? 1 : 0.4
            enabled: PlayerSettings.loudnessBoost
            from: 0
            to: 2
            stepSize: 1
            name: content.strings.strength_title
            value: content.loudnessLevels.indexOf(PlayerSettings.loudnessStrength)
            label: content.glassLevels[value]
            onMoved: value => PlayerSettings.loudnessStrength = content.loudnessLevels[Math.round(value)]
        }
    }
    Row_ {
        tab: "audio"
        title: content.strings.device_title
        description: content.strings.device_desc
        Drop {
            objectName: "audioDevice"
            name: content.strings.device_title
            options: content.audioDevices.map(d => ({value: d.name, label: d.name === "auto" ? content.strings.device_default : d.description}))
            value: PlayerSettings.audioDevice
            onChosen: value => PlayerSettings.audioDevice = value
        }
    }
    Row_ {
        tab: "audio"
        title: content.strings.maxvol_title
        description: content.strings.maxvol_desc
        Levels {
            objectName: "maxVolume"
            from: 100
            to: 300
            stepSize: 10
            name: content.strings.maxvol_title
            value: PlayerSettings.volumeMax
            label: PlayerSettings.volumeMax + "%"
            onMoved: value => PlayerSettings.volumeMax = value
        }
    }
    Row_ {
        tab: "audio"
        title: content.strings.tracks_title
        description: content.strings.tracks_desc
        Drop {
            name: content.strings.tracks_title
            options: [{value: "last", label: content.strings.tracks_last}, {value: "all", label: content.strings.tracks_all}]
            value: PlayerSettings.tracksOnOpen
            onChosen: value => PlayerSettings.tracksOnOpen = value
        }
    }

    // Playback
    Row_ {
        tab: "playback"
        title: content.strings.seek_title
        description: content.strings.seek_desc
        Seg {
            segmentPadding: 14
            options: [3, 5, 10, 30].map(s => ({value: s, label: content.strings.seconds.arg(s)}))
            value: PlayerSettings.seekStep
            onChosen: value => PlayerSettings.seekStep = value
        }
    }
    Row_ {
        tab: "playback"
        title: content.strings.speedstep_title
        description: content.strings.speedstep_desc
        Step {
            objectName: "speedStep"
            text: PlayerSettings.speedStep.toFixed(2) + "x"
            lessName: content.strings.speedstep_title
            moreName: content.strings.speedstep_title
            onLess: PlayerSettings.speedStep = Math.max(0.05, PlayerSettings.speedStep - 0.05)
            onMore: PlayerSettings.speedStep = Math.min(1, PlayerSettings.speedStep + 0.05)
        }
    }
    Row_ {
        tab: "playback"
        title: content.strings.fixed_title
        description: content.strings.fixed_desc
        Step {
            objectName: "fixedSpeed"
            text: PlayerSettings.speedFactor.toFixed(2) + "x"
            lessName: content.strings.slower
            moreName: content.strings.faster
            onLess: PlayerSettings.speedFactor -= 0.25
            onMore: PlayerSettings.speedFactor += 0.25
        }
    }
    Row_ {
        tab: "playback"
        title: content.strings.end_title
        description: content.strings.end_desc
        Seg {
            options: [{value: "next", label: content.strings.end_next}, {value: "loop", label: content.strings.end_loop},
                      {value: "stop", label: content.strings.end_stop}]
            value: PlayerSettings.endMode
            onChosen: value => PlayerSettings.endMode = value
        }
    }
    Row_ {
        tab: "playback"
        title: content.strings.resume_title
        description: content.strings.resume_desc
        Drop {
            implicitWidth: 200
            name: content.strings.resume_title
            options: [{value: "ask", label: content.strings.resume_ask}, {value: "always", label: content.strings.resume_always},
                      {value: "never", label: content.strings.resume_never}]
            value: PlayerSettings.resumeMode
            onChosen: value => PlayerSettings.resumeMode = value
        }
    }

    // Picture
    Row_ {
        tab: "picture"
        title: content.strings.quality_title
        description: content.strings.quality_desc
        Seg {
            options: [{value: "auto", label: content.strings.tier_auto}, {value: "ultra", label: content.strings.tier_ultra},
                      {value: "normal", label: content.strings.tier_normal}, {value: "low", label: content.strings.tier_low}]
            value: PictureQuality.pref
            onChosen: value => PictureQuality.pref = value
        }
    }

    // Appearance
    Row_ {
        tab: "appearance"
        title: content.strings.theme_title
        description: content.strings.theme_desc
        Seg {
            options: [{value: "dark", label: content.strings.theme_dark, icon: "dark_mode"},
                      {value: "light", label: content.strings.theme_light, icon: "light_mode"},
                      {value: "system", label: content.strings.theme_system, icon: "contrast"}]
            value: Theme.mode
            onChosen: value => Theme.mode = value
        }
    }
    Row_ {
        tab: "appearance"
        title: content.strings.accent_title
        description: content.strings.accent_desc
        Row {
            spacing: 12
            Repeater {
                model: [""].concat(Theme.tokens.swatches)
                Item {
                    id: swatch
                    required property string modelData
                    readonly property color shown: modelData || Theme.tokens.accent
                    readonly property bool selected: Qt.colorEqual(shown, Theme.accent)
                    objectName: "swatch-" + modelData
                    width: 28
                    height: 28
                    activeFocusOnTab: true
                    Keys.onReturnPressed: Theme.accentChoice = modelData
                    Rectangle {
                        visible: swatch.selected
                        anchors.fill: parent
                        anchors.margins: -4
                        radius: width / 2
                        color: "transparent"
                        border.width: 2
                        border.color: swatch.shown
                    }
                    Rectangle {
                        anchors.fill: parent
                        radius: width / 2
                        color: swatch.shown
                    }
                    FocusRing { target: swatch; targetRadius: 14 }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: Theme.accentChoice = swatch.modelData
                    }
                }
            }
            IconButton {
                id: anyColor
                objectName: "anyColor"
                implicitWidth: 28
                implicitHeight: 28
                icon: "colorize"
                iconSize: 16
                color: content.colors.text
                hoverColor: content.colors.control
                tip: content.strings.any_color
                onClicked: content.overlay.showColorPicker(anyColor)
                DashedFrame {
                    anchors.fill: parent
                    z: -1
                    radius: 14
                    lineWidth: 1
                    color: content.colors.textMuted
                    fill: "transparent"
                }
            }
        }
    }
    Row_ {
        tab: "appearance"
        title: content.strings.bar_title
        description: content.strings.bar_desc
        Seg {
            options: [{value: "island", label: content.strings.bar_island}, {value: "full", label: content.strings.bar_full}]
            value: Theme.barStyle
            onChosen: value => Theme.barStyle = value
        }
    }
    Row_ {
        tab: "appearance"
        title: content.strings.font_title
        description: content.fontFailed ? content.strings.font_bad : content.strings.font_desc
        Row {
            spacing: 12
            Drop {
                implicitWidth: 160
                name: content.strings.font_title
                options: Theme.fonts.map(f => ({value: f, label: f}))
                value: Theme.fontFamily
                onChosen: value => Theme.fontFamily = value
            }
            Button_ {
                icon: "upload_file"
                text: content.strings.load_font
                onClicked: content.chooser().pickFont(url => {
                    const family = Theme.loadFontFile(url)
                    content.fontFailed = !family
                    if (family) Theme.fontFamily = family
                })
            }
        }
    }
    Row_ {
        tab: "appearance"
        title: content.strings.glass_title
        description: content.strings.glass_desc
        Levels {
            from: 0
            to: 2
            stepSize: 1
            name: content.strings.glass_title
            value: Theme.glassStrength
            label: content.glassLevels[Theme.glassStrength]
            onMoved: value => Theme.glassStrength = Math.round(value)
        }
    }
    Row_ {
        tab: "appearance"
        title: content.strings.themefile_title
        description: content.strings.themefile_desc
        Button_ {
            icon: "open_in_new"
            text: content.strings.themefile_open
            onClicked: Theme.openThemeFile()
        }
    }

    // Language
    Row_ {
        tab: "language"
        title: content.strings.language_title
        description: content.strings.language_desc
        Drop {
            objectName: "languageList"
            implicitWidth: 200
            name: content.strings.language_title
            options: I18n.languages.map(l => ({value: l.code, label: l.name}))
            value: I18n.language
            onChosen: value => I18n.language = value
        }
    }

    // Screenshots
    Row_ {
        tab: "screenshots"
        title: content.strings.shotfolder_title
        description: content.strings.shotfolder_desc
        Drop {
            implicitWidth: 220
            name: content.strings.shotfolder_title
            options: [{value: "desktop", label: content.strings.folder_desktop}, {value: "videos", label: content.strings.folder_videos},
                      {value: "pictures", label: content.strings.folder_pictures}, {value: "video_folder", label: content.strings.folder_video},
                      {value: "custom", label: content.strings.folder_custom}]
            value: PlayerSettings.screenshotFolder
            onChosen: value => PlayerSettings.screenshotFolder = value
        }
    }
    Row_ {
        visible: PlayerSettings.screenshotFolder === "custom" && !content.overlay.search && content.overlay.tab === "screenshots"
        tab: "screenshots"
        title: content.strings.shotcustom_title
        description: PlayerSettings.screenshotCustomPath || content.strings.no_folder
        Button_ {
            icon: "folder_open"
            text: content.strings.choose_folder
            onClicked: content.chooser().pickFolder(url => PlayerSettings.screenshotCustomPath = MediaFiles.localPath(url))
        }
    }

    // Shortcuts: a click on a key waits for the new one, Esc keeps the old.
    Row_ {
        tab: "shortcuts"
        title: content.strings.shortcuts_title
        description: content.strings.shortcuts_desc
        Button_ {
            objectName: "resetShortcuts"
            icon: "refresh"
            text: content.strings.reset_shortcuts
            onClicked: { content.keyMessageRow = ""; PlayerSettings.resetShortcuts() }
        }
    }
    Repeater {
        model: content.ui.window.actions
        Row_ {
            id: shortcutRow
            required property var modelData
            objectName: "keyRow-" + modelData.id
            tab: "shortcuts"
            title: content.strings["action_" + modelData.id]
            description: content.keyMessageRow === modelData.id ? content.keyMessage : ""
            descriptionColor: content.colors.danger
            TextButton {
                id: keyChip
                objectName: "key-" + shortcutRow.modelData.id
                readonly property bool waiting: content.capturing === shortcutRow.modelData.id
                implicitHeight: 36
                implicitWidth: Math.max(96, keyText.implicitWidth + 28)
                textColor: content.colors.text
                controlColor: content.colors.control
                borderColor: waiting ? Theme.accent : content.colors.border
                onClicked: {
                    content.keyMessageRow = ""
                    content.capturing = shortcutRow.modelData.id
                    forceActiveFocus()
                }
                Label {
                    id: keyText
                    anchors.centerIn: parent
                    text: keyChip.waiting ? content.strings.press_key : content.shortcutKey(shortcutRow.modelData)
                    color: keyChip.waiting ? Theme.accent : content.colors.text
                    font.weight: Font.Bold
                }
                Keys.onShortcutOverride: event => event.accepted = waiting
                Keys.onPressed: event => {
                    if (!waiting) return
                    event.accepted = true
                    if (event.key === Qt.Key_Escape) { content.capturing = ""; return }
                    const key = PlayerSettings.keyName(event.key, event.modifiers)
                    if (!key) return // a modifier alone: the key comes next
                    const owner = content.ui.window.actions.find(a => a.id !== shortcutRow.modelData.id && content.shortcutKey(a) === key)
                    if (owner) {
                        content.keyMessage = content.strings.key_taken.arg(content.strings["action_" + owner.id])
                        content.keyMessageRow = shortcutRow.modelData.id
                    } else PlayerSettings.setShortcut(shortcutRow.modelData.id, key)
                    content.capturing = ""
                }
                onActiveFocusChanged: if (!activeFocus && waiting) content.capturing = ""
            }
        }
    }

    // About
    Column {
        visible: content.overlay.tab === "about" && !content.overlay.search
        width: parent.width
        Row {
            topPadding: 12
            bottomPadding: 20
            spacing: 16
            Rectangle {
                width: 64
                height: 64
                radius: 16
                color: Theme.dark ? "transparent" : Theme.tokens.glass.fill
                Image {
                    anchors.fill: parent
                    anchors.margins: Theme.dark ? 0 : 8
                    source: "qrc:/images/logo.png"
                    sourceSize: Qt.size(128, 128)
                    smooth: true
                }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Label {
                    text: "EchoPlay " + UpdateCheck.currentVersion
                    color: content.colors.text
                    font.pixelSize: Theme.tokens.type.heading
                    font.weight: Font.Bold
                }
                Label {
                    text: content.strings.publisher.arg("Hüseyin Can ÇALTI")
                    color: content.colors.textMuted
                }
            }
        }
        UpdateCard {
            width: parent.width
            ui: content.ui
        }
        Grid {
            topPadding: 24
            columns: 2
            columnSpacing: 16
            rowSpacing: 14
            component Key: Label {
                width: 160
                color: content.colors.textMuted
            }
            component Link: Row {
                id: link
                property string text
                property string url
                spacing: 4
                Label {
                    text: link.text
                    color: content.colors.text
                    font.weight: Font.DemiBold
                    font.underline: linkMouse.containsMouse
                    MouseArea {
                        id: linkMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally(link.url)
                    }
                }
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "open_in_new"
                    size: 16
                    color: content.colors.text
                }
            }
            Key { text: content.strings.engine }
            Label {
                text: content.engine ? content.engine + " (libmpv)" : "libmpv"
                color: content.colors.text
                font.weight: Font.DemiBold
            }
            Key { text: content.strings.source_code }
            Link {
                text: "github.com/huseyincancalti/EchoPlay"
                url: "https://github.com/huseyincancalti/EchoPlay"
            }
            Key { text: content.strings.licenses }
            Link {
                text: content.strings.open_licenses
                url: "https://github.com/huseyincancalti/EchoPlay/blob/main/app/THIRD_PARTY_NOTICES.md"
            }
        }
        Item { width: 1; height: 28 }
        Rectangle { width: parent.width; height: 1; color: content.colors.hairline }
        Row {
            topPadding: 16
            spacing: 8
            Icon {
                name: "shield"
                size: 18
                color: content.colors.textMuted
            }
            Label {
                width: content.width - 26
                wrapMode: Text.WordWrap
                text: content.strings.privacy_line
                color: content.colors.textMuted
                font.pixelSize: Theme.tokens.type.small
            }
        }
    }
}
