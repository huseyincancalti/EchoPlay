#pragma once

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QtQml/qqmlregistration.h>

class AppState;
class QNetworkReply;

// Tells the user a newer EchoPlay exists: one GET to GitHub's "latest release" at most once a day, no query
// string, no IDs ("Checks GitHub once a day for a new version. Sends nothing else."). A dev build
// (0.0.0-dev) never checks. Every failure is silent. The About page reads the properties; the card about a
// new version is shown once per version (newVersionFound), not on every start.
class UpdateCheck : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY changed)
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(QDateTime lastChecked READ lastChecked NOTIFY changed) // invalid when never checked
    Q_PROPERTY(QString releasesUrl READ releasesUrl CONSTANT)

public:
    UpdateCheck(AppState *state, const QString &currentVersion, QObject *parent = nullptr);

    QString currentVersion() const { return m_current; }
    QString latestVersion() const { return m_latest; }
    bool updateAvailable() const { return m_updateAvailable; }
    bool checking() const { return m_reply != nullptr; }
    QDateTime lastChecked() const;
    QString releasesUrl() const;

    // major.minor.patch, numeric; anything unparsable is never newer.
    static bool isNewer(const QString &a, const QString &b);
    // True when a check is due: not a dev build, and 24 h or more since the last one (never = due).
    bool due(qint64 nowEpochSeconds) const;
    // Called 5 s after launch (main.cpp): checks when due.
    void start();
    // About's "Check now": ignores the 24 h gate, still never runs for a dev build.
    Q_INVOKABLE void checkNow();
    // Reads GitHub's release JSON. Public for the tests; anything unexpected changes nothing.
    void handleReply(const QByteArray &json);

    static UpdateCheck *create(QQmlEngine *, QJSEngine *)
    {
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
    static void setInstance(UpdateCheck *check) { s_instance = check; } // main.cpp, before QML loads

signals:
    void changed();
    void newVersionFound(const QString &version);

private:
    void check();

    static inline UpdateCheck *s_instance = nullptr;
    AppState *m_state;
    QString m_current;
    QString m_latest;
    bool m_updateAvailable = false;
    QNetworkAccessManager m_net;
    QNetworkReply *m_reply = nullptr;
};
