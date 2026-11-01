#include <QtTest>
#include <QTemporaryDir>
#include "ErrorReport.h"

class TestErrorReport : public QObject
{
    Q_OBJECT

private slots:
    void kinds_data()
    {
        QTest::addColumn<QString>("error");
        QTest::addColumn<QString>("kind");
        QTest::newRow("format") << "unrecognized file format" << "format";
        QTest::newRow("load") << "loading failed" << "load";
        QTest::newRow("empty") << "no audio or video data played" << "empty";
        QTest::newRow("audio") << "audio output initialization failed" << "audio";
        QTest::newRow("video") << "video output initialization failed" << "video";
        QTest::newRow("unknown") << "something new from mpv" << "other";
    }
    void kinds()
    {
        QFETCH(QString, error);
        QFETCH(QString, kind);
        QCOMPARE(ErrorReport::kind(error), kind);
    }

    void reportReadsInOrder()
    {
        QTemporaryDir dir;
        const QString clip = dir.filePath("clip.mp4");
        QFile f(clip);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(2048, 'x'));
        f.close();
        const QString text = ErrorReport::report(clip, "unrecognized file format", {"[cplayer] Failed to recognize file format."},
                                                 "2.0.0", "mpv v0.41", "C:/Users/someone");
        const QStringList order = {"EchoPlay could not play this file.", "What happened: the file has no video or audio",
                                   "Step: opening the file", "Cause, from the mpv engine: unrecognized file format.", "What to try:",
                                   "Technical details", "File: clip.mp4 (.mp4, 2", "EchoPlay 2.0.0, mpv v0.41,", "Last log lines:",
                                   "[cplayer] Failed to recognize file format."};
        int at = 0;
        for (const QString &part : order) {
            const int found = text.indexOf(part, at);
            QVERIFY2(found >= 0, qPrintable("missing or out of order: " + part + "\n" + text));
            at = found;
        }
    }

    void theUserNameIsRedactedEverywhere()
    {
        const QString text = ErrorReport::report("C:/Users/Someone/Videos/clip.mp4", "loading failed",
                                                 {"[file] Cannot open C:\\Users\\someone\\Videos\\clip.mp4"}, "2.0.0", "mpv", "C:/Users/someone");
        QVERIFY2(!text.contains("someone", Qt::CaseInsensitive), qPrintable(text));
        QVERIFY2(text.contains("C:\\Users\\<user>\\Videos"), qPrintable(text));
    }

    void noLogNoLogHeading()
    {
        QVERIFY(!ErrorReport::report("x.mp4", "loading failed", {}, "2.0.0", "mpv").contains("Last log lines"));
    }

    void issueUrlIsPrefilledAndNeverTooLong()
    {
        QStringList log;
        for (int i = 0; i < 400; ++i) log << "[ffmpeg] a long line of decoder output " + QString::number(i);
        const QString text = ErrorReport::report("x.mp4", "loading failed", log, "2.0.0", "mpv");
        const QString url = ErrorReport::issueUrl(text);
        QVERIFY(url.startsWith("https://github.com/huseyincancalti/EchoPlay/issues/new?title="));
        QVERIFY(url.size() < 6000);
        const QUrlQuery query{QUrl(url)};
        // The sentences survive; only log lines at the end went.
        QVERIFY(query.queryItemValue("body", QUrl::FullyDecoded).startsWith("EchoPlay could not play this file."));
        QVERIFY(query.queryItemValue("body", QUrl::FullyDecoded).contains("What to try:"));
        QVERIFY(query.queryItemValue("title", QUrl::FullyDecoded).startsWith("What happened:"));
    }
};

QTEST_GUILESS_MAIN(TestErrorReport)
#include "tst_errorreport.moc"
