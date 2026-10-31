#include "SingleInstance.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
constexpr int ConnectTimeoutMs = 300; // a live instance answers at once; a dead name must not delay start-up
}

SingleInstance::SingleInstance(const QString &serverName, QObject *parent) : QObject(parent), m_name(serverName)
{
    connect(&m_server, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
}

QString SingleInstance::defaultName()
{
#ifdef Q_OS_WIN
    return "echoplay-" + qEnvironmentVariable("USERNAME");
#else
    return "echoplay-" + qEnvironmentVariable("USER");
#endif
}

bool SingleInstance::sendToRunning(const QString &file, qint64 sentAtEpochMs)
{
    QLocalSocket socket;
    socket.connectToServer(m_name);
    if (!socket.waitForConnected(ConnectTimeoutMs)) return false;
#ifdef Q_OS_WIN
    // Windows blocks focus stealing; this process was just launched by the user, so it may pass the right on.
    AllowSetForegroundWindow(ASFW_ANY);
#endif
    const QJsonObject msg{{"file", file}, {"sent_ms", sentAtEpochMs}};
    socket.write(QJsonDocument(msg).toJson(QJsonDocument::Compact) + '\n');
    // Without the waits the process can exit before the bytes leave (named pipes lose them).
    const bool sent = socket.waitForBytesWritten(ConnectTimeoutMs);
    socket.disconnectFromServer();
    if (socket.state() != QLocalSocket::UnconnectedState) socket.waitForDisconnected(ConnectTimeoutMs);
    return sent;
}

bool SingleInstance::listen()
{
    if (m_server.listen(m_name)) return true;
    if (m_server.serverError() != QAbstractSocket::AddressInUseError) return false;
    // Unix: a crashed instance leaves its socket file behind. Only called after sendToRunning found no
    // live instance, so the name is stale.
    QLocalServer::removeServer(m_name);
    return m_server.listen(m_name);
}

void SingleInstance::onNewConnection()
{
    while (QLocalSocket *socket = m_server.nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { readMessages(socket); });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        readMessages(socket); // the sender may have written before we connected the signal
    }
}

// One JSON object per line. Anything else is ignored: the pipe is open to every process of this user.
void SingleInstance::readMessages(QLocalSocket *socket)
{
    while (socket->canReadLine()) {
        const QJsonObject msg = QJsonDocument::fromJson(socket->readLine()).object();
        if (msg.value("file").isString())
            emit fileRequested(msg.value("file").toString(), msg.value("sent_ms").toInteger());
    }
}
