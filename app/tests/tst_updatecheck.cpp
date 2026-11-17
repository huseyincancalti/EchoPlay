#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "AppState.h"
#include "UpdateCheck.h"

namespace {

const QString Download = "https://github.com/huseyincancalti/EchoPlay/releases/download/v2.1.0/";
const QString Digest = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

// A release with one asset; an empty digest leaves the field out, like a release GitHub gave no digest for.
QByteArray release(const QString &name, const QString &url, const QString &digest = QString())
{
    QJsonObject asset{{"name", name}, {"browser_download_url", url}};
    if (!digest.isEmpty()) asset.insert("digest", digest);
    return QJsonDocument(QJsonObject{{"tag_name", "v2.1.0"}, {"assets", QJsonArray{asset}}}).toJson();
}

} // namespace

class TestUpdateCheck : public QObject
{
    Q_OBJECT

private slots:
    void init() { QVERIFY(m_dir.isValid()); m_path = m_dir.filePath(QString("state-%1.json").arg(m_n++)); }

    void isNewerCompares()
    {
        QVERIFY(UpdateCheck::isNewer("1.4.1", "1.4.0"));  // patch
        QVERIFY(UpdateCheck::isNewer("1.5.0", "1.4.9"));  // minor
        QVERIFY(UpdateCheck::isNewer("2.0.0", "1.9.9"));  // major
        QVERIFY(!UpdateCheck::isNewer("1.4.0", "1.4.0")); // equal
        QVERIFY(!UpdateCheck::isNewer("1.3.0", "1.4.0")); // older
        QVERIFY(!UpdateCheck::isNewer("nightly", "1.4.0"));
        QVERIFY(!UpdateCheck::isNewer("1.4.1", "nightly"));
    }

    void devBuildNeverChecks()
    {
        AppState state(m_path);
        UpdateCheck check(&state, "0.0.0-dev");
        check.start();
        check.checkNow();
        QVERIFY(!check.checking());
        QVERIFY(!check.lastChecked().isValid());
    }

    void newerTagIsAvailableAndAnnouncedOnEveryCheck()
    {
        AppState state(m_path);
        UpdateCheck check(&state, "2.0.0");
        QSignalSpy found(&check, &UpdateCheck::newVersionFound);

        check.handleReply(R"({"tag_name": "v2.1.0"})");
        QCOMPARE(check.latestVersion(), QString("2.1.0"));
        QVERIFY(check.updateAvailable());
        QCOMPARE(found.count(), 1);
        QCOMPARE(found.at(0).at(0).toString(), QString("2.1.0"));

        check.handleReply(R"({"tag_name": "v2.1.0"})");
        QVERIFY(check.updateAvailable());
        QCOMPARE(found.count(), 2); // the same version again: the next start announces it again
        QCOMPARE(found.at(1).at(0).toString(), QString("2.1.0"));
    }

    void equalOrOlderTagIsNotAvailable()
    {
        AppState state(m_path);
        UpdateCheck check(&state, "2.0.0");
        QSignalSpy found(&check, &UpdateCheck::newVersionFound);
        for (const char *tag : {"v2.0.0", "v1.9.0"}) {
            check.handleReply(QByteArray(R"({"tag_name": ")") + tag + "\"}");
            QVERIFY(!check.updateAvailable());
        }
        QCOMPARE(found.count(), 0);
    }

    void badRepliesChangeNothing()
    {
        AppState state(m_path);
        UpdateCheck check(&state, "2.0.0");
        QSignalSpy found(&check, &UpdateCheck::newVersionFound);
        QSignalSpy changed(&check, &UpdateCheck::changed);
        for (const char *json : {"{not json", "", "[]", R"({"name": "v9.9.9"})", R"({"tag_name": 5})"})
            check.handleReply(json);
        QCOMPARE(check.latestVersion(), QString());
        QVERIFY(!check.updateAvailable());
        QCOMPARE(found.count(), 0);
        QCOMPARE(changed.count(), 0);
    }

    void setupAssetGivesUrlAndLowercaseDigest()
    {
#ifndef Q_OS_WIN
        QSKIP("Windows only: off Windows the card opens the release page, there is no setup");
#endif
        AppState state(m_path);
        UpdateCheck check(&state, "2.0.0");
        check.handleReply(release("EchoPlay-Setup.exe", Download + "EchoPlay-Setup.exe", "sha256:" + Digest.toUpper()));
        QCOMPARE(check.setupUrl(), Download + "EchoPlay-Setup.exe");
        QCOMPARE(check.setupSha256(), Digest);
    }

    void noSetupOffWindows()
    {
#ifdef Q_OS_WIN
        QSKIP("Windows installs its setup");
#endif
        AppState state(m_path);
        UpdateCheck check(&state, "2.0.0");
        check.handleReply(release("EchoPlay-Setup.exe", Download + "EchoPlay-Setup.exe", "sha256:" + Digest));
        QVERIFY(check.updateAvailable());
        QVERIFY(check.setupUrl().isEmpty());
        QVERIFY(check.setupSha256().isEmpty());
    }

    void wrongAssetNameGivesNoSetup()
    {
        AppState state(m_path);
        UpdateCheck check(&state, "2.0.0");
        for (const char *name : {"EchoPlay-Setup-2.1.0.exe", "EchoPlay-Setup.exe.sha256", "echoplay-setup.exe"}) {
            check.handleReply(release(name, Download + name, "sha256:" + Digest));
            QVERIFY(check.setupUrl().isEmpty());
            QVERIFY(check.setupSha256().isEmpty());
        }
        check.handleReply(R"({"tag_name": "v2.1.0"})"); // no assets at all
        QVERIFY(check.setupUrl().isEmpty());
    }

    void foreignDownloadHostGivesNoSetup()
    {
        AppState state(m_path);
        UpdateCheck check(&state, "2.0.0");
        for (const char *url : {"https://example.com/EchoPlay-Setup.exe",
                                "http://github.com/huseyincancalti/EchoPlay/releases/download/v2.1.0/EchoPlay-Setup.exe",
                                "https://github.com/someone/EchoPlay/releases/download/v2.1.0/EchoPlay-Setup.exe"}) {
            check.handleReply(release("EchoPlay-Setup.exe", url, "sha256:" + Digest));
            QVERIFY(check.setupUrl().isEmpty());
            QVERIFY(check.setupSha256().isEmpty()); // a setup that is not taken has no hash either
        }
    }

    void missingOrOddDigestGivesNoSetup()
    {
        AppState state(m_path);
        UpdateCheck check(&state, "2.0.0");
        for (const char *digest : {"", "md5:" "0123456789abcdef0123456789abcdef", "sha256:", "sha256:xyz", "sha256:0123"}) {
            check.handleReply(release("EchoPlay-Setup.exe", Download + "EchoPlay-Setup.exe", digest));
            QVERIFY(check.setupUrl().isEmpty());
            QVERIFY(check.setupSha256().isEmpty());
            QVERIFY(check.updateAvailable()); // the card still tells, and leads to the release page
        }
    }

    void notesWaitForTheVersionTheyBelongTo()
    {
        AppState state(m_path);
        const QByteArray reply = QJsonDocument(QJsonObject{{"tag_name", "v2.1.0"}, {"body", "  Better mixer  "}}).toJson();
        {
            UpdateCheck old(&state, "2.0.0");
            old.handleReply(reply);
            QCOMPARE(old.whatsNew(), QString()); // an offer, nothing to tell yet
            state.save();
        }
        AppState restarted(m_path); // the new version's first start: no check has run, maybe no network
        UpdateCheck updated(&restarted, "2.1.0");
        QCOMPARE(updated.whatsNew(), QString("Better mixer"));

        QSignalSpy changed(&updated, &UpdateCheck::changed);
        updated.closeWhatsNew();
        QCOMPARE(updated.whatsNew(), QString());
        QCOMPARE(changed.count(), 1);
        restarted.save();
        AppState again(m_path);
        QCOMPARE(UpdateCheck(&again, "2.1.0").whatsNew(), QString()); // once
    }

    void theHeadingOnTopGoes()
    {
        AppState state(m_path);
        UpdateCheck old(&state, "2.0.0");
        old.handleReply(QJsonDocument(QJsonObject{{"tag_name", "v2.1.0"},
                                                  {"body", "## What's new in v2.1.0\r\n\r\n**Better** mixer\n\n## Fixed\n\nA bug"}}).toJson());
        QCOMPARE(UpdateCheck(&state, "2.1.0").whatsNew(), QString("**Better** mixer\n\n## Fixed\n\nA bug"));
    }

    void aReleaseWithoutNotesHasNoCard()
    {
        AppState state(m_path);
        UpdateCheck old(&state, "2.0.0");
        old.handleReply(R"({"tag_name": "v2.1.0"})");
        QCOMPARE(UpdateCheck(&state, "2.1.0").whatsNew(), QString());
    }

    void longNotesAreCut()
    {
        AppState state(m_path);
        UpdateCheck old(&state, "2.0.0");
        old.handleReply(QJsonDocument(QJsonObject{{"tag_name", "v2.1.0"}, {"body", QString(100000, 'x')}}).toJson());
        QCOMPARE(UpdateCheck(&state, "2.1.0").whatsNew().size(), 4000);
    }

    void aSameOrOlderTagLeavesTheNotesAlone()
    {
        AppState state(m_path);
        state.setReleaseNotes("2.1.0", "Better mixer");
        UpdateCheck updated(&state, "2.1.0");
        updated.handleReply(R"({"tag_name": "v2.1.0", "body": "Other"})");
        QCOMPARE(updated.whatsNew(), QString("Better mixer"));
    }

    void aHotfixFoundRightAfterAnUpdateKeepsTheNotesUnseen()
    {
        AppState state(m_path);
        state.setReleaseNotes("2.1.0", "Better mixer"); // saved by 2.0.0, waiting for 2.1.0's card
        UpdateCheck updated(&state, "2.1.0");
        updated.handleReply(R"({"tag_name": "v2.1.1", "body": "Hotfix"})");
        QCOMPARE(updated.whatsNew(), QString("Better mixer"));
        updated.closeWhatsNew(); // seen: the next find may save
        updated.handleReply(R"({"tag_name": "v2.1.1", "body": "Hotfix"})");
        QCOMPARE(UpdateCheck(&state, "2.1.1").whatsNew(), QString("Hotfix"));
    }

    void laterReplyReplacesTheSetup()
    {
#ifndef Q_OS_WIN
        QSKIP("Windows only: off Windows the card opens the release page, there is no setup");
#endif
        AppState state(m_path);
        UpdateCheck check(&state, "2.0.0");
        check.handleReply(release("EchoPlay-Setup.exe", Download + "EchoPlay-Setup.exe", "sha256:" + Digest));
        QVERIFY(!check.setupUrl().isEmpty());
        check.handleReply(R"({"tag_name": "v2.2.0", "assets": []})");
        QCOMPARE(check.latestVersion(), QString("2.2.0"));
        QVERIFY(check.setupUrl().isEmpty());
        QVERIFY(check.setupSha256().isEmpty());
    }

private:
    QTemporaryDir m_dir;
    QString m_path;
    int m_n = 0;
};

QTEST_GUILESS_MAIN(TestUpdateCheck)
#include "tst_updatecheck.moc"
