#pragma once

#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QtQml/qqmlregistration.h>
#include <functional>
#include <memory>

class QNetworkReply;

// Installs a new EchoPlay from inside the app: downloads the setup the update check found
// (UpdateCheck.setupUrl), checks its SHA-256, starts it silently and asks the app to quit so the setup can
// replace the files. The setup starts the new EchoPlay when it is done (installer/windows/v2/EchoPlay.iss).
// state is "idle", "downloading", "installing" or "failed"; a failed one can start over with install().
class Updater : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(qint64 received READ received NOTIFY changed)
    Q_PROPERTY(qint64 total READ total NOTIFY changed) // 0 while unknown

public:
    // downloadDir gets the setup file; launch starts it and says whether it did (injected, so the tests
    // start no process).
    Updater(const QString &downloadDir, std::function<bool(const QString &setupPath)> launch,
            QObject *parent = nullptr);
    ~Updater() override;

    QString state() const { return m_state; }
    qint64 received() const { return m_received; }
    qint64 total() const { return m_total; }

    // sha256 is lowercase hex; empty skips the check. Does nothing while downloading or installing.
    Q_INVOKABLE void install(const QString &url, const QString &sha256, const QString &version);
    // Downloading -> idle, the partial file removed.
    Q_INVOKABLE void cancel();

    static Updater *create(QQmlEngine *, QJSEngine *)
    {
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
    static void setInstance(Updater *updater) { s_instance = updater; } // main.cpp, before QML loads

signals:
    void changed();
    void quitRequested(); // the setup is running: QML closes the window

private:
    void setState(const QString &state);
    bool store();
    void finish();
    void fail();
    void discard();

    static inline Updater *s_instance = nullptr;
    QString m_dir;
    std::function<bool(const QString &)> m_launch;
    QNetworkAccessManager m_net;
    QNetworkReply *m_reply = nullptr;
    std::unique_ptr<QFile> m_file; // the .part being written; null when no download runs
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    QString m_state = QStringLiteral("idle");
    QString m_sha256;
    QString m_setupPath;
    qint64 m_received = 0;
    qint64 m_total = 0;
};
