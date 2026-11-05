import QtQuick
import QtQuick.Dialogs
import EchoPlay

// Windows' own open dialogs. Loaded on first use: QtQuick.Dialogs costs startup time.
Item {
    id: dialogs

    property var done: null // what to do with the chosen url

    function pickVideo(callback) { pick(callback, [I18n.strings.videos + " (" + MediaFiles.videoGlobs + ")", I18n.strings.all_files + " (*)"]) }
    function pickSubtitle(callback) { pick(callback, [I18n.strings.subtitle_files + " (" + MediaFiles.subtitleGlobs + ")", I18n.strings.all_files + " (*)"]) }
    function pickFont(callback) { pick(callback, [I18n.strings.fonts + " (*.ttf *.otf)"]) }
    function pick(callback, filters) {
        done = callback
        file.nameFilters = filters
        file.open()
    }
    function pickFolder(callback) {
        done = callback
        folder.open()
    }

    FileDialog {
        id: file
        onAccepted: dialogs.done(selectedFile)
    }
    FolderDialog {
        id: folder
        onAccepted: dialogs.done(selectedFolder)
    }
}
