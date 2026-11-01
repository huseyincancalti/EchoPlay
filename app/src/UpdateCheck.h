#pragma once

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QtQml/qqmlregistration.h>

class AppState;
class QJsonArray;
class QNetworkReply;

// Tells the user a newer EchoPlay exists: one GET to GitHub's latest release when EchoPlay starts and on
// Check now, no query string, no IDs. A dev build (0.0.0-dev) never checks. Every failure is silent. The
// About page reads the properties; newVersionFound fires on every check that finds a newer version. The
// release's setup asset (setupUrl, setupSha256) is what Updater downloads.
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
    Q_PROPERTY(QString setupUrl READ setupUrl NOTIFY changed)       // empty: the release has no usable setup
    Q_PROPERTY(QString setupSha256 READ setupSha256 NOTIFY changed) // lowercase hex; empty: GitHub gave no digest
    Q_PROPERTY(QString whatsNew READ whatsNew NOTIFY changed) // this version's release notes, until closed; else empty

public:
    UpdateCheck(AppState *state, const QString &currentVersion, QObject *parent = nullptr);

    QString currentVersion() const { return m_current; }
    QString latestVersion() const { return m_latest; }
    bool updateAvailable() const { return m_updateAvailable; }
    bool checking() const { return m_reply != nullptr; }
    QDateTime lastChecked() const;
    QString releasesUrl() const;
    QString setupUrl() const { return m_setupUrl; }
    QString setupSha256() const { return m_setupSha256; }
    QString whatsNew() const;

    // The card after an update is done with: it never comes back for this version.
    Q_INVOKABLE void closeWhatsNew();
    // major.minor.patch, numeric; anything unparsable is never newer.
    static bool isNewer(const QString &a, const QString &b);
    // Called 4 s after launch (main.cpp): checks, unless this is a dev build.
    void start();
    // About's "Check now": same check, still never runs for a dev build.
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
    void readSetup(const QJsonArray &assets);

    static inline UpdateCheck *s_instance = nullptr;
    AppState *m_state;
    QString m_current;
    QString m_latest;
    bool m_updateAvailable = false;
    QString m_setupUrl;
    QString m_setupSha256;
    QNetworkAccessManager m_net;
    QNetworkReply *m_reply = nullptr;
};
