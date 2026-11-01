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
// installOnClose downloads the same way without a word and waits: main.cpp starts the setup once the app has
// closed, and EchoPlay stays closed.
// state is "idle", "downloading", "ready" (waiting for the app to close), "installing" or "failed"; a failed one can
// start over with install().
class Updater : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(qint64 received READ received NOTIFY changed)
    Q_PROPERTY(qint64 total READ total NOTIFY changed) // 0 while unknown
    Q_PROPERTY(bool onClose READ onClose NOTIFY changed) // this download installs when the app closes

public:
    // downloadDir gets the setup file; launch starts it and says whether it did (injected, so the tests
    // start no process).
    Updater(const QString &downloadDir, std::function<bool(const QString &setupPath)> launch,
            QObject *parent = nullptr);
    ~Updater() override;

    QString state() const { return m_state; }
    qint64 received() const { return m_received; }
    qint64 total() const { return m_total; }
    bool onClose() const { return m_onClose; }
    // The setup main.cpp starts after the app has closed; empty when none is waiting.
    QString setupOnClose() const { return m_state == QLatin1String("ready") ? m_setupPath : QString(); }

    // sha256 is lowercase hex; without one nothing is downloaded, as the setup runs without a click. Does nothing while
    // downloading or installing. When the setup is already waiting for the app to close, starts it now.
    Q_INVOKABLE void install(const QString &url, const QString &sha256, const QString &version);
    // Nobody is watching this one: a failure goes back to idle without a word, and the next run asks again. Does
    // nothing while downloading, waiting or installing.
    Q_INVOKABLE void installOnClose(const QString &url, const QString &sha256, const QString &version);
    // Downloading -> idle, the partial file removed.
    Q_INVOKABLE void cancel();
    // What earlier updates left: a setup is still running when it reopens EchoPlay, so it cannot remove itself. One still
    // in use stays until the next start.
    void removeOldSetups();

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
    void download(const QString &url, const QString &sha256, const QString &version);
    bool store();
    void finish();
    void launch();
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
    bool m_onClose = false;
};
