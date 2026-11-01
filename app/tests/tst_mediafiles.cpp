#include <QtTest>
#include <QTemporaryDir>
#include "MediaFiles.h"

class TestMediaFiles : public QObject
{
    Q_OBJECT

    static void touch(const QString &path)
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
    }

private slots:
    void aFileIsItself()
    {
        QTemporaryDir dir;
        const QString clip = dir.filePath("clip.mp4");
        touch(clip);
        QCOMPARE(MediaFiles::pathToOpen(QUrl::fromLocalFile(clip)), clip);
    }

    void aFolderOpensItsFirstVideoByNaturalSort()
    {
        QTemporaryDir dir;
        touch(dir.filePath("clip10.mp4"));
        touch(dir.filePath("clip2.MKV")); // extensions in any case
        touch(dir.filePath("a-notes.txt"));
        touch(dir.filePath("a-cover.jpg"));
        QVERIFY(QDir(dir.path()).mkdir("a-subfolder.mp4")); // a subfolder never counts, whatever its name
        QCOMPARE(MediaFiles::pathToOpen(QUrl::fromLocalFile(dir.path())), dir.filePath("clip2.MKV"));
    }

    void aFolderWithoutVideosOpensNothing()
    {
        QTemporaryDir dir;
        touch(dir.filePath("notes.txt"));
        QCOMPARE(MediaFiles::pathToOpen(QUrl::fromLocalFile(dir.path())), QString());
    }

    void aLinkStaysALink()
    {
        const QUrl link("https://example.com/clip.mp4");
        QCOMPARE(MediaFiles::pathToOpen(link), link.toString());
    }

    void displayNames()
    {
        QTemporaryDir dir;
        QCOMPARE(MediaFiles::displayName(QUrl::fromLocalFile(dir.filePath("My clip.mp4"))), QString("My clip.mp4"));
    }
};

QTEST_GUILESS_MAIN(TestMediaFiles)
#include "tst_mediafiles.moc"
