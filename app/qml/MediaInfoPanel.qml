import QtQuick
import EchoPlay
import "Format.js" as Format

// What the playing clip is made of: the file, then each track with its codec, size, rate and bitrate, as the
// container reports them (mpv's track-list). Copy puts the same as text on the clipboard, for a bug report.
Popover {
    id: info

    required property var ui
    readonly property var glass: Theme.tokens.glass
    readonly property var strings: I18n.strings
    property var fileSize: ui.mpv.observed("file-size")
    property var fileFormat: ui.mpv.observed("file-format")
    property var videoParams: ui.mpv.observed("video-params")
    readonly property var sections: [
        {title: strings.info_video, type: "video"}, {title: strings.audio_tracks, type: "audio"}, {title: strings.subtitles, type: "sub"}
    ].map(s => ({title: s.title, tracks: ui.trackList.filter(t => t.type === s.type && !t.image)})).filter(s => s.tracks.length > 0)

    objectName: "mediaInfo"
    backdrop: ui.backdrop
    pointed: false
    width: Math.min(460, ui.width - 32)
    implicitHeight: column.y + column.height + 18

    Component.onCompleted: {
        for (const name of ["file-size", "file-format", "video-params"]) ui.mpv.observeProperty(name)
    }
    Connections {
        target: info.ui.mpv
        function onPropertyChanged(name, value) {
            if (name === "file-size") info.fileSize = value
            else if (name === "file-format") info.fileFormat = value
            else if (name === "video-params") info.videoParams = value
        }
    }

    // Audio tracks by the mixer's name (the recorder's metadata, as Mixer.cpp reads it), others by their title.
    function name(t) {
        if (t.type === "audio") return (ui.tracks.find(a => a.id === t.id) || {}).name || ""
        return t.title || ""
    }
    function codec(name) {
        const known = {h264: "H.264", hevc: "HEVC", av1: "AV1", vp9: "VP9", vp8: "VP8", mpeg4: "MPEG-4", aac: "AAC", opus: "Opus",
                       mp3: "MP3", flac: "FLAC", vorbis: "Vorbis", ac3: "AC-3", eac3: "E-AC-3", subrip: "SubRip", ass: "ASS",
                       mov_text: "MP4 text", webvtt: "WebVTT", hdmv_pgs_subtitle: "PGS"}
        return known[name] || (name.startsWith("pcm_") ? "PCM" : String(name || "?").toUpperCase())
    }
    function bitrate(bits) {
        if (!(bits > 0)) return ""
        return bits >= 1e6 ? (bits / 1e6).toFixed(1) + " Mb/s" : Math.round(bits / 1e3) + " kb/s"
    }
    function channels(count) {
        return count === 1 ? strings.mono : count === 2 ? strings.stereo : count === 6 ? "5.1" : count === 8 ? "7.1"
             : count > 0 ? strings.info_channels.arg(count) : ""
    }
    function details(t) {
        const parts = [codec(t.codec) + (t["codec-profile"] && t.type === "video" ? " " + t["codec-profile"] : "")]
        if (t.type === "video") {
            if (t["demux-w"] > 0) parts.push(t["demux-w"] + " × " + t["demux-h"])
            if (t["demux-fps"] > 0) parts.push(Number(t["demux-fps"].toFixed(2)) + " fps")
            if (t.selected && videoParams && (videoParams.gamma === "pq" || videoParams.gamma === "hlg")) parts.push("HDR")
        } else if (t.type === "audio") {
            parts.push(channels(t["demux-channel-count"]))
            if (t["demux-samplerate"] > 0) parts.push(Number((t["demux-samplerate"] / 1000).toFixed(1)) + " kHz")
        }
        parts.push(bitrate(t["demux-bitrate"]))
        return parts.filter(p => p).join(" · ")
    }
    readonly property string fileLine: {
        // The file's own extension: mpv's file-format names the demuxer family ("mov" for an .mp4).
        const extension = /\.([a-z0-9]{2,5})$/i.exec(ui.mpv.observed("path") || "")
        const parts = [extension ? extension[1].toUpperCase() : String(fileFormat || "").split(",")[0]]
        if (fileSize > 0) parts.push(fileSize >= 1073741824 ? (fileSize / 1073741824).toFixed(2) + " GB" : (fileSize / 1048576).toFixed(1) + " MB")
        if (ui.duration > 0) parts.push(Format.time(ui.duration))
        if (fileSize > 0 && ui.duration > 0) parts.push(bitrate(fileSize * 8 / ui.duration))
        return parts.filter(p => p).join(" · ")
    }
    function asText() {
        const lines = [ui.mediaTitle, fileLine]
        for (const s of sections) {
            lines.push("", s.title)
            for (const t of s.tracks) lines.push(t.id + "  " + (name(t) ? name(t) + "  " : "") + details(t))
        }
        return lines.join("\n")
    }

    Label {
        x: 20
        y: 18
        text: info.strings.clip_info
        color: info.glass.text
        font.pixelSize: 16
        font.weight: Font.Bold
    }
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 10
        y: 12
        spacing: 4
        TextButton {
            objectName: "mediaInfoCopy"
            implicitHeight: 32
            style: "plain"
            icon: "content_copy"
            text: info.strings.copy
            fontSize: Theme.tokens.type.small
            onClicked: {
                ErrorReport.copy(info.asText())
                info.ui.window.showToast(info.strings.copied, 1500)
            }
        }
        IconButton {
            objectName: "mediaInfoClose"
            implicitWidth: 32
            implicitHeight: 32
            icon: "close"
            iconSize: Theme.tokens.icon.sm
            background: "transparent"
            hoverColor: info.glass.controlSubtle
            tip: info.strings.close
            onClicked: info.ui.popover = ""
        }
    }
    Column {
        id: column
        x: 20
        y: 54
        width: parent.width - 40
        spacing: 14
        Column {
            width: parent.width
            spacing: 3
            Label {
                width: parent.width
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 2
                elide: Text.ElideRight
                text: info.ui.mediaTitle
                color: info.glass.text
                font.pixelSize: Theme.tokens.type.label
                font.weight: Font.DemiBold
            }
            Label {
                objectName: "mediaInfoFile"
                width: parent.width
                text: info.fileLine
                color: info.glass.textMuted
                font.pixelSize: Theme.tokens.type.small
                font.features: {"tnum": 1}
            }
        }
        Repeater {
            model: info.sections
            Column {
                id: section
                required property var modelData
                width: column.width
                spacing: 6
                Label {
                    text: section.modelData.title.toUpperCase()
                    color: info.glass.textMuted
                    font.pixelSize: Theme.tokens.type.caption
                    font.weight: Font.Bold
                    font.letterSpacing: 0.6
                }
                Repeater {
                    model: section.modelData.tracks
                    Column {
                        id: track
                        required property var modelData
                        objectName: "mediaInfoTrack-" + modelData.type + "-" + modelData.id
                        readonly property string details: info.details(modelData)
                        width: section.width
                        spacing: 1
                        Label {
                            visible: text !== ""
                            width: parent.width
                            elide: Text.ElideRight
                            text: info.name(track.modelData)
                            color: info.glass.text
                            font.pixelSize: Theme.tokens.type.small
                            font.weight: Font.DemiBold
                        }
                        Label {
                            width: parent.width
                            wrapMode: Text.Wrap
                            text: track.details
                            color: info.name(track.modelData) ? info.glass.textMuted : info.glass.text
                            font.pixelSize: Theme.tokens.type.small
                            font.features: {"tnum": 1}
                        }
                    }
                }
            }
        }
    }
}
