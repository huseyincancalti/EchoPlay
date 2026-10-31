#include <QtTest>
#include <QLocalSocket>
#include <QThread>
#include <QUuid>
#include <functional>
#include <memory>
#include "SingleInstance.h"

namespace {

// A unique name per test: tests must not see each other's servers or a real EchoPlay.
QString uniqueName()
{
    return "echoplay-test-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

// A named-pipe write only completes when the other end reads. The real sender is another process; here
// it gets its own thread while this thread's event loop serves the receiving side (same thread deadlocks).
void runOnThread(const std::function<void()> &fn)
{
    std::unique_ptr<QThread> thread(QThread::create(fn));
    QSignalSpy finished(thread.get(), &QThread::finished);
    thread->start();
    QVERIFY(finished.wait(5000));
    thread->wait();
}

bool send(const QString &name, const QString &file, qint64 sentAtEpochMs)
{
    bool sent = false;
    runOnThread([&] { sent = SingleInstance(name).sendToRunning(file, sentAtEpochMs); });
    return sent;
}

} // namespace

class TestSingleInstance : public QObject
{
    Q_OBJECT

private slots:
    void sendWithNobodyListeningReturnsFalse()
    {
        QVERIFY(!SingleInstance(uniqueName()).sendToRunning("clip.mp4", 1));
    }

    void listeningInstanceReceivesFileAndTimeExactly()
    {
        const QString name = uniqueName();
        SingleInstance running(name);
        QVERIFY(running.listen());
        QSignalSpy spy(&running, &SingleInstance::fileRequested);

        QVERIFY(send(name, "clip.mp4", 1761900000123));
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
        QCOMPARE(spy.at(0).at(0).toString(), QString("clip.mp4"));
        QCOMPARE(spy.at(0).at(1).toLongLong(), qint64(1761900000123));
    }

    void pathWithNonAsciiAndSpacesArrivesUnchanged()
    {
        const QString name = uniqueName();
        SingleInstance running(name);
        QVERIFY(running.listen());
        QSignalSpy spy(&running, &SingleInstance::fileRequested);

        const QString path = QString::fromUtf8("C:/Videos/\u00c7al\u0131\u015fma klas\u00f6r\u00fc/clip 1.mp4");
        QVERIFY(send(name, path, 5));
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
        QCOMPARE(spy.at(0).at(0).toString(), path);
    }

    void emptyFileArrivesEmpty()
    {
        const QString name = uniqueName();
        SingleInstance running(name);
        QVERIFY(running.listen());
        QSignalSpy spy(&running, &SingleInstance::fileRequested);

        QVERIFY(send(name, QString(), 7));
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
        QCOMPARE(spy.at(0).at(0).toString(), QString());
        QCOMPARE(spy.at(0).at(1).toLongLong(), qint64(7));
    }

    void malformedMessagesAreIgnored()
    {
        const QString name = uniqueName();
        SingleInstance running(name);
        QVERIFY(running.listen());
        QSignalSpy spy(&running, &SingleInstance::fileRequested);

        runOnThread([&] {
            QLocalSocket raw;
            raw.connectToServer(name);
            if (!raw.waitForConnected(2000)) return;
            raw.write("not json\n[1,2]\n{\"sent_ms\":1}\n{\"file\":\"ok.mp4\",\"sent_ms\":9}\n");
            raw.waitForBytesWritten(2000);
        });
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
        QTest::qWait(100); // a wrongly accepted line would arrive by now
        QCOMPARE(spy.count(), 1); // only the last line is a valid message
        QCOMPARE(spy.at(0).at(0).toString(), QString("ok.mp4"));
    }
};

QTEST_GUILESS_MAIN(TestSingleInstance)
#include "tst_singleinstance.moc"
