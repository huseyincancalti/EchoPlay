#include "MediaFiles.h"

#include <QCollator>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#endif

namespace {

// mpv's own video-exts and audio-exts (mpv --list-options), and video types other players open too: camcorder .mts and
// .m2t, DVD .vob, .f4v, .divx, .asf, .rm. Main.qml gives both lists to mpv, so the folder playlist, the open dialog and
// the installer's file types (installer/windows/v2/EchoPlay.iss, checked by tst_mediafiles) all take the same files.
const QStringList VideoExtensions = {"3g2", "3gp", "asf", "avi", "divx", "f4v", "flv", "ivf", "m2t", "m2ts", "m4v",
                                     "mj2", "mkv", "mov", "mp4", "mpeg", "mpg", "mts", "mxf", "ogv", "rm", "rmvb",
                                     "ts", "vob", "webm", "wmv", "y4m"};
const QStringList AudioExtensions = {"aac", "ac3", "aiff", "ape", "au", "dff", "dsf", "dts", "eac3", "flac", "m4a",
                                     "mka", "mp1", "mp2", "mp3", "mpc", "oga", "ogg", "ogm", "opus", "tak", "thd",
                                     "tta", "wav", "wma", "wv"};

const QStringList SubtitleExtensions = {"ass", "srt", "ssa", "sub", "sup", "vtt"};

QStringList globs(const QStringList &extensions)
{
    QStringList result;
    for (const QString &extension : extensions) result << "*." + extension;
    return result;
}

} // namespace

QString MediaFiles::mediaGlobs()
{
    return globs(VideoExtensions + AudioExtensions).join(' ');
}

QString MediaFiles::videoExtensions()
{
    return VideoExtensions.join(',');
}

QString MediaFiles::audioExtensions()
{
    return AudioExtensions.join(',');
}

QString MediaFiles::subtitleGlobs()
{
    return globs(SubtitleExtensions).join(' ');
}

bool MediaFiles::isSubtitle(const QUrl &url)
{
    if (!url.isLocalFile()) return false;
    const QFileInfo file(url.toLocalFile());
    return file.isFile() && SubtitleExtensions.contains(file.suffix(), Qt::CaseInsensitive);
}

QString MediaFiles::pathToOpen(const QUrl &url)
{
    if (!url.isLocalFile()) return url.toString();
    const QString path = url.toLocalFile();
    if (!QFileInfo(path).isDir()) return path;

    QDir folder(path);
    QStringList videos = folder.entryList(globs(VideoExtensions), QDir::Files | QDir::Readable);
    if (videos.isEmpty()) return {};
    QCollator natural;
    natural.setNumericMode(true);
    natural.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(videos.begin(), videos.end(), natural);
    return folder.filePath(videos.first());
}

QString MediaFiles::displayName(const QUrl &url)
{
    return url.isLocalFile() ? QFileInfo(url.toLocalFile()).fileName() : url.toString();
}

void MediaFiles::showInFolder(const QString &path)
{
#ifdef Q_OS_WIN
    // The shell's own call, not "explorer /select,": a comma in the path breaks that one. COM is up: Qt's GUI thread
    // starts OLE.
    if (PIDLIST_ABSOLUTE item = ILCreateFromPathW(reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(path).utf16()))) {
        const HRESULT shown = SHOpenFolderAndSelectItems(item, 0, nullptr, 0);
        ILFree(item);
        if (SUCCEEDED(shown)) return;
    }
#endif
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

bool MediaFiles::moveToTrash(const QString &path)
{
    return QFile::moveToTrash(path);
}
