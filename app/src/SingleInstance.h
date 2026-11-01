#pragma once

#include <QLocalServer>
#include <QObject>
#include <QString>

class QLocalSocket;

// One running EchoPlay per user. A second launch hands its file to the running one (sendToRunning) and
// exits; the running one announces it with fileRequested. Opening clip 2..n then skips the whole
// Qt/D3D11/libmpv start-up.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(const QString &serverName, QObject *parent = nullptr);

    static QString defaultName(); // "echoplay-<user>": one per user, not per machine

    // True when a running instance took the message. An empty file only asks it to come to the front.
    bool sendToRunning(const QString &file, qint64 sentAtEpochMs);
    // Become the running instance. Call it only after sendToRunning returned false.
    bool listen();
    // The app is closing: the next launch must not hand its file to this process, it would be lost.
    void stopListening() { m_server.close(); }

signals:
    void fileRequested(const QString &file, qint64 sentAtEpochMs);

private:
    void onNewConnection();
    void readMessages(QLocalSocket *socket);

    QString m_name;
    QLocalServer m_server;
};
