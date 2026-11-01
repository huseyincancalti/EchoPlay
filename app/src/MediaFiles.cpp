#include "MediaFiles.h"

#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <algorithm>

namespace {

// mpv's own video-exts, so a folder opens on a file autocreate-playlist also takes.
const QStringList VideoExtensions = {"3g2", "3gp", "avi", "flv", "ivf", "m2ts", "m4v", "mj2", "mkv", "mov",
                                     "mp4", "mpeg", "mpg", "mxf", "ogv", "rmvb", "ts", "webm", "wmv", "y4m"};

QStringList globs()
{
    QStringList result;
    for (const QString &extension : VideoExtensions) result << "*." + extension;
    return result;
}

} // namespace

QString MediaFiles::videoGlobs()
{
    return globs().join(' ');
}

QString MediaFiles::pathToOpen(const QUrl &url)
{
    if (!url.isLocalFile()) return url.toString();
    const QString path = url.toLocalFile();
    if (!QFileInfo(path).isDir()) return path;

    QDir folder(path);
    QStringList videos = folder.entryList(globs(), QDir::Files | QDir::Readable);
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
