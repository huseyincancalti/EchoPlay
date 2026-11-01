import QtQuick
import QtQuick.Dialogs
import EchoPlay

// Windows' own open dialogs. Loaded on first use: QtQuick.Dialogs costs startup time.
Item {
    id: dialogs

    signal chosen(url url)

    function pickFile() { file.open() }
    function pickFolder() { folder.open() }

    FileDialog {
        id: file
        nameFilters: [I18n.strings.videos + " (" + MediaFiles.videoGlobs + ")", I18n.strings.all_files + " (*)"]
        onAccepted: dialogs.chosen(selectedFile)
    }
    FolderDialog {
        id: folder
        onAccepted: dialogs.chosen(selectedFolder)
    }
}
