#include <QtTest>
#include "AppState.h"
#include "UpdateCheck.h"

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

    void devBuildIsNeverDue()
    {
        AppState state(m_path);
        UpdateCheck check(&state, "0.0.0-dev");
        QVERIFY(!check.due(2000000000));
    }

    void dueFollowsTheLastCheck()
    {
        const qint64 now = 2000000000;
        AppState state(m_path);
        UpdateCheck check(&state, "2.0.0");
        QVERIFY(check.due(now)); // never checked
        state.setUpdateLastChecked(now - 23 * 3600);
        QVERIFY(!check.due(now));
        state.setUpdateLastChecked(now - 24 * 3600);
        QVERIFY(check.due(now));
    }

    void newerTagIsAvailableAndAnnouncedOnce()
    {
        {
            AppState state(m_path);
            UpdateCheck check(&state, "2.0.0");
            QSignalSpy found(&check, &UpdateCheck::newVersionFound);

            check.handleReply(R"({"tag_name": "v2.1.0"})");
            QCOMPARE(check.latestVersion(), QString("2.1.0"));
            QVERIFY(check.updateAvailable());
            QCOMPARE(state.updateSeenVersion(), QString("2.1.0"));
            QCOMPARE(found.count(), 1);
            QCOMPARE(found.at(0).at(0).toString(), QString("2.1.0"));

            check.handleReply(R"({"tag_name": "v2.1.0"})");
            QVERIFY(check.updateAvailable()); // still newer, but already shown
            QCOMPARE(found.count(), 1);
            state.save();
        }
        QCOMPARE(AppState(m_path).updateSeenVersion(), QString("2.1.0")); // persisted
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
        QCOMPARE(state.updateSeenVersion(), QString());
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
        QCOMPARE(state.updateSeenVersion(), QString());
        QCOMPARE(found.count(), 0);
        QCOMPARE(changed.count(), 0);
    }

private:
    QTemporaryDir m_dir;
    QString m_path;
    int m_n = 0;
};

QTEST_GUILESS_MAIN(TestUpdateCheck)
#include "tst_updatecheck.moc"
