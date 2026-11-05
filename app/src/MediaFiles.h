#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// What opening a file or a folder means, and what the right-click menu does with a clip. A folder plays its first
// video; mpv's autocreate-playlist builds the rest.
class MediaFiles : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString mediaGlobs READ mediaGlobs CONSTANT) // for the open dialog's filter: video and audio
    Q_PROPERTY(QString videoExtensions READ videoExtensions CONSTANT) // mpv's video-exts and audio-exts, comma-separated
    Q_PROPERTY(QString audioExtensions READ audioExtensions CONSTANT)
    Q_PROPERTY(QString subtitleGlobs READ subtitleGlobs CONSTANT)

public:
    explicit MediaFiles(QObject *parent = nullptr) : QObject(parent) {}

    static QString mediaGlobs();
    static QString videoExtensions();
    static QString audioExtensions();
    static QString subtitleGlobs();
    // An existing local file with a subtitle extension (any case); a folder, a missing file or a link is not.
    Q_INVOKABLE static bool isSubtitle(const QUrl &url);
    // A file: its path. A folder: its first video by natural sort (clip2 before clip10), "" when it has none. Not
    // a local file (a link): the URL as it is, mpv opens those too.
    Q_INVOKABLE static QString pathToOpen(const QUrl &url);
    Q_INVOKABLE static QString localPath(const QUrl &url) { return url.toLocalFile(); }
    // File or folder name, for the drop overlay.
    Q_INVOKABLE static QString displayName(const QUrl &url);
    // Explorer on the clip's folder with the clip selected.
    Q_INVOKABLE static void showInFolder(const QString &path);
    // To the Recycle Bin, never deleted for good. False: it is still there (no rights, another program locks it).
    Q_INVOKABLE static bool moveToTrash(const QString &path);
};
