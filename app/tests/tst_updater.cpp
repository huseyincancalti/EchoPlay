#include <QtTest>
#include <QCryptographicHash>
#include <QDir>
#include <QHash>
#include <QTcpServer>
#include <QTcpSocket>
#include "Updater.h"

namespace {

// A small HTTP/1.1 server on the loopback: every path answers with a canned response, so the tests use no
// real network.
class HttpServer : public QObject
{
public:
    struct Route
    {
        int status = 200;
        QByteArray body;
        QByteArray headers;  // extra header lines, each ending in \r\n
        bool stall = false;  // sends half the body and then nothing: a transfer that hangs
    };

    explicit HttpServer(QObject *parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = m_server.nextPendingConnection()) {
                socket->setParent(this);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] { answer(socket); });
                connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
                    m_requests.remove(socket);
                    socket->deleteLater();
                });
            }
        });
    }

    bool listen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    QString url(const QString &path) const { return QString("http://127.0.0.1:%1%2").arg(m_server.serverPort()).arg(path); }

    QHash<QString, Route> routes;

private:
    void answer(QTcpSocket *socket)
    {
        QByteArray &request = m_requests[socket];
        request += socket->readAll();
        if (!request.contains("\r\n\r\n")) return;
        const QString path = QString::fromLatin1(request.split(' ').value(1));
        request.clear();

        const Route route = routes.value(path, Route{404, "not found", {}, false});
        socket->write("HTTP/1.1 " + QByteArray::number(route.status) + " Test\r\nContent-Length: "
                      + QByteArray::number(route.body.size()) + "\r\nConnection: close\r\n" + route.headers + "\r\n");
        socket->write(route.stall ? route.body.left(route.body.size() / 2) : route.body);
        if (!route.stall) socket->disconnectFromHost();
    }

    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_requests;
};

QString sha256(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QByteArray readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

class TestUpdater : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        QVERIFY(m_server.listen());
        m_body.resize(300000); // several reads, so the download really streams
        for (int i = 0; i < m_body.size(); ++i) m_body[i] = char(i * 31 + i / 256);
    }

    // Its own folder per test, not created: install() makes it.
    void init() { m_downloads = m_dir.filePath(QString("downloads-%1").arg(m_n++)); }

    void goodDownloadIsVerifiedRenamedAndLaunched()
    {
        m_server.routes["/setup"] = {200, m_body};
        QDir().mkpath(m_downloads);
        QFile old(m_downloads + "/EchoPlay-Setup-2.1.0.exe"); // an earlier download of this version
        QVERIFY(old.open(QIODevice::WriteOnly));
        old.write("old");
        old.close();

        QStringList launched;
        Updater updater(m_downloads, [&](const QString &path) { launched << path; return true; });
        QSignalSpy quit(&updater, &Updater::quitRequested);
        QCOMPARE(updater.state(), QString("idle"));

        updater.install(m_server.url("/setup"), sha256(m_body), "2.1.0");
        QCOMPARE(updater.state(), QString("downloading"));
        QVERIFY(quit.wait(10000));

        const QString setup = m_downloads + "/EchoPlay-Setup-2.1.0.exe";
        QCOMPARE(updater.state(), QString("installing"));
        QCOMPARE(updater.received(), qint64(m_body.size()));
        QCOMPARE(updater.total(), qint64(m_body.size()));
        QCOMPARE(launched, QStringList{setup});
        QCOMPARE(readFile(setup), m_body);
        QCOMPARE(files(), QStringList{"EchoPlay-Setup-2.1.0.exe"}); // no .part left
        QCOMPARE(quit.count(), 1);
    }

    void emptyShaIsAccepted()
    {
        m_server.routes["/setup"] = {200, m_body};
        QStringList launched;
        Updater updater(m_downloads, [&](const QString &path) { launched << path; return true; });
        QSignalSpy quit(&updater, &Updater::quitRequested);

        updater.install(m_server.url("/setup"), QString(), "2.1.0");
        QVERIFY(quit.wait(10000));
        QCOMPARE(launched.size(), 1);
        QCOMPARE(readFile(launched.at(0)), m_body);
    }

    void redirectIsFollowed()
    {
        m_server.routes["/setup"] = {200, m_body};
        m_server.routes["/latest"] = {302, {}, "Location: /setup\r\n", false};
        Updater updater(m_downloads, [](const QString &) { return true; });
        QSignalSpy quit(&updater, &Updater::quitRequested);

        updater.install(m_server.url("/latest"), sha256(m_body), "2.1.0");
        QVERIFY(quit.wait(10000));
        QCOMPARE(readFile(m_downloads + "/EchoPlay-Setup-2.1.0.exe"), m_body);
    }

    void wrongShaFailsAndLeavesNothing()
    {
        m_server.routes["/setup"] = {200, m_body};
        int launches = 0;
        Updater updater(m_downloads, [&](const QString &) { ++launches; return true; });
        QSignalSpy quit(&updater, &Updater::quitRequested);

        updater.install(m_server.url("/setup"), sha256("something else"), "2.1.0");
        QTRY_COMPARE_WITH_TIMEOUT(updater.state(), QString("failed"), 10000);
        QVERIFY(files().isEmpty());
        QCOMPARE(launches, 0);
        QCOMPARE(quit.count(), 0);
    }

    void notFoundFailsAndLeavesNothing()
    {
        int launches = 0;
        Updater updater(m_downloads, [&](const QString &) { ++launches; return true; });

        updater.install(m_server.url("/missing"), sha256(m_body), "2.1.0");
        QTRY_COMPARE_WITH_TIMEOUT(updater.state(), QString("failed"), 10000);
        QVERIFY(files().isEmpty());
        QCOMPARE(launches, 0);
    }

    void emptyFileFails()
    {
        m_server.routes["/empty"] = {200, {}};
        Updater updater(m_downloads, [](const QString &) { return true; });

        updater.install(m_server.url("/empty"), QString(), "2.1.0");
        QTRY_COMPARE_WITH_TIMEOUT(updater.state(), QString("failed"), 10000);
        QVERIFY(files().isEmpty());
    }

    void oddVersionFailsWithoutTouchingTheDisk()
    {
        Updater updater(m_downloads, [](const QString &) { return true; });
        for (const char *version : {"", "..\\evil", "../evil", "a b"}) {
            updater.install(m_server.url("/setup"), QString(), version);
            QCOMPARE(updater.state(), QString("failed"));
        }
        QVERIFY(!QDir(m_downloads).exists());
    }

    void launchRefusedFails()
    {
        m_server.routes["/setup"] = {200, m_body};
        QStringList launched;
        Updater updater(m_downloads, [&](const QString &path) { launched << path; return false; });
        QSignalSpy quit(&updater, &Updater::quitRequested);

        updater.install(m_server.url("/setup"), sha256(m_body), "2.1.0");
        QTRY_COMPARE_WITH_TIMEOUT(updater.state(), QString("failed"), 10000);
        QCOMPARE(launched, QStringList{m_downloads + "/EchoPlay-Setup-2.1.0.exe"});
        QCOMPARE(quit.count(), 0);
        QVERIFY(files().isEmpty());
    }

    void cancelStopsATransferAndLeavesNothing()
    {
        m_server.routes["/slow"] = {200, m_body, {}, true};
        Updater updater(m_downloads, [](const QString &) { return true; });

        updater.install(m_server.url("/slow"), sha256(m_body), "2.1.0");
        QTRY_VERIFY_WITH_TIMEOUT(updater.received() > 0, 10000);
        QCOMPARE(updater.state(), QString("downloading"));
        QVERIFY(updater.received() < m_body.size());

        updater.cancel();
        QCOMPARE(updater.state(), QString("idle"));
        QVERIFY(files().isEmpty());
        QTest::qWait(200); // nothing from the aborted transfer comes back
        QCOMPARE(updater.state(), QString("idle"));
        QVERIFY(files().isEmpty());
    }

    void installWhileDownloadingDoesNothing()
    {
        m_server.routes["/slow"] = {200, m_body, {}, true};
        Updater updater(m_downloads, [](const QString &) { return true; });

        updater.install(m_server.url("/slow"), sha256(m_body), "2.1.0");
        QTRY_VERIFY_WITH_TIMEOUT(updater.received() > 0, 10000);
        updater.install(m_server.url("/slow"), sha256(m_body), "9.9.9");
        QCOMPARE(files(), QStringList{"EchoPlay-Setup-2.1.0.exe.part"});
        updater.cancel();
    }

    void failedStateCanStartOver()
    {
        m_server.routes["/setup"] = {200, m_body};
        Updater updater(m_downloads, [](const QString &) { return true; });
        QSignalSpy quit(&updater, &Updater::quitRequested);

        updater.install(m_server.url("/missing"), QString(), "2.1.0");
        QTRY_COMPARE_WITH_TIMEOUT(updater.state(), QString("failed"), 10000);

        updater.install(m_server.url("/setup"), sha256(m_body), "2.1.0");
        QVERIFY(quit.wait(10000));
        QCOMPARE(updater.state(), QString("installing"));
        QCOMPARE(readFile(m_downloads + "/EchoPlay-Setup-2.1.0.exe"), m_body);
    }

    void installOnCloseDownloadsAndWaits()
    {
        m_server.routes["/setup"] = {200, m_body};
        int launches = 0;
        Updater updater(m_downloads, [&](const QString &) { ++launches; return true; });
        QSignalSpy quit(&updater, &Updater::quitRequested);
        QVERIFY(updater.setupOnClose().isEmpty());

        updater.installOnClose(m_server.url("/setup"), sha256(m_body), "2.1.0");
        QCOMPARE(updater.state(), QString("downloading"));
        QVERIFY(updater.onClose());
        QVERIFY(updater.setupOnClose().isEmpty()); // not until it is all here
        QTRY_COMPARE_WITH_TIMEOUT(updater.state(), QString("ready"), 10000);

        const QString setup = m_downloads + "/EchoPlay-Setup-2.1.0.exe";
        QCOMPARE(updater.setupOnClose(), setup);
        QCOMPARE(readFile(setup), m_body);
        QCOMPARE(files(), QStringList{"EchoPlay-Setup-2.1.0.exe"});
        QCOMPARE(launches, 0); // main.cpp starts it, after the app has closed
        QCOMPARE(quit.count(), 0);
    }

    void installAfterReadyStartsWithoutDownloadingAgain()
    {
        m_server.routes["/setup"] = {200, m_body};
        QStringList launched;
        Updater updater(m_downloads, [&](const QString &path) { launched << path; return true; });
        QSignalSpy quit(&updater, &Updater::quitRequested);
        updater.installOnClose(m_server.url("/setup"), sha256(m_body), "2.1.0");
        QTRY_COMPARE_WITH_TIMEOUT(updater.state(), QString("ready"), 10000);

        m_server.routes.remove("/setup"); // a new download would fail
        updater.install(m_server.url("/setup"), sha256(m_body), "2.1.0");
        QCOMPARE(updater.state(), QString("installing"));
        QCOMPARE(launched, QStringList{m_downloads + "/EchoPlay-Setup-2.1.0.exe"});
        QCOMPARE(quit.count(), 1);
        QVERIFY(updater.setupOnClose().isEmpty()); // started once, not again at the close
    }

    void aSetupThatWillNotStartAfterReadySaysSo()
    {
        m_server.routes["/setup"] = {200, m_body};
        Updater updater(m_downloads, [](const QString &) { return false; });
        updater.installOnClose(m_server.url("/setup"), sha256(m_body), "2.1.0");
        QTRY_COMPARE_WITH_TIMEOUT(updater.state(), QString("ready"), 10000);

        updater.install(m_server.url("/setup"), sha256(m_body), "2.1.0");
        QCOMPARE(updater.state(), QString("failed")); // the user asked for this one: not a silent failure
        QVERIFY(files().isEmpty());
    }

    void installOnCloseFailsWithoutAWord()
    {
        int launches = 0;
        Updater updater(m_downloads, [&](const QString &) { ++launches; return true; });

        updater.installOnClose(m_server.url("/missing"), sha256(m_body), "2.1.0");
        QTRY_COMPARE_WITH_TIMEOUT(updater.state(), QString("idle"), 10000);
        QVERIFY(!updater.onClose());
        QVERIFY(updater.setupOnClose().isEmpty());
        QVERIFY(files().isEmpty());
        QCOMPARE(launches, 0);
    }

    void aWrongHashLeavesNothingForTheClose()
    {
        m_server.routes["/setup"] = {200, m_body};
        Updater updater(m_downloads, [](const QString &) { return true; });
        updater.installOnClose(m_server.url("/setup"), sha256("something else"), "2.1.0");
        QTRY_COMPARE_WITH_TIMEOUT(updater.state(), QString("idle"), 10000);
        QVERIFY(updater.setupOnClose().isEmpty());
        QVERIFY(files().isEmpty());
    }

    void installOnCloseDoesNothingWhileBusy()
    {
        m_server.routes["/setup"] = {200, m_body};
        m_server.routes["/slow"] = {200, m_body, {}, true};
        Updater updater(m_downloads, [](const QString &) { return true; });

        updater.install(m_server.url("/slow"), sha256(m_body), "2.1.0");
        QTRY_VERIFY_WITH_TIMEOUT(updater.received() > 0, 10000);
        updater.installOnClose(m_server.url("/setup"), sha256(m_body), "9.9.9");
        QVERIFY(!updater.onClose()); // the one the user asked for stays what it was
        QCOMPARE(files(), QStringList{"EchoPlay-Setup-2.1.0.exe.part"});
        updater.cancel();

        updater.installOnClose(m_server.url("/setup"), sha256(m_body), "2.1.0");
        QTRY_COMPARE_WITH_TIMEOUT(updater.state(), QString("ready"), 10000);
        updater.installOnClose(m_server.url("/missing"), sha256(m_body), "2.1.0");
        QCOMPARE(updater.state(), QString("ready"));
    }

    void cancelOrClosingMidDownloadLeavesNothingForTheClose()
    {
        m_server.routes["/slow"] = {200, m_body, {}, true};
        {
            Updater updater(m_downloads, [](const QString &) { return true; });
            updater.installOnClose(m_server.url("/slow"), sha256(m_body), "2.1.0");
            QTRY_VERIFY_WITH_TIMEOUT(updater.received() > 0, 10000);
            updater.cancel();
            QCOMPARE(updater.state(), QString("idle"));
            QVERIFY(!updater.onClose());
            QVERIFY(files().isEmpty());

            updater.installOnClose(m_server.url("/slow"), sha256(m_body), "2.1.0");
            QTRY_VERIFY_WITH_TIMEOUT(updater.received() > 0, 10000);
            QVERIFY(updater.setupOnClose().isEmpty()); // the app closes now: nothing to start
        }
        QVERIFY(files().isEmpty());
    }

private:
    // What is in this test's download folder (empty when it does not exist).
    QStringList files() const { return QDir(m_downloads).entryList(QDir::Files | QDir::NoDotAndDotDot); }

    QTemporaryDir m_dir;
    HttpServer m_server;
    QByteArray m_body;
    QString m_downloads;
    int m_n = 0;
};

QTEST_GUILESS_MAIN(TestUpdater)
#include "tst_updater.moc"
