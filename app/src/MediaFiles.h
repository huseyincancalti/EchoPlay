#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// What opening a file or a folder means. A folder plays its first video; mpv's autocreate-playlist builds the rest.
class MediaFiles : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString videoGlobs READ videoGlobs CONSTANT) // for the open dialog's filter

public:
    explicit MediaFiles(QObject *parent = nullptr) : QObject(parent) {}

    static QString videoGlobs();
    // A file: its path. A folder: its first video by natural sort (clip2 before clip10), "" when it has none. Not
    // a local file (a link): the URL as it is, mpv opens those too.
    Q_INVOKABLE static QString pathToOpen(const QUrl &url);
    // File or folder name, for the drop overlay.
    Q_INVOKABLE static QString displayName(const QUrl &url);
};
