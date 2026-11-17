#include <QtTest>
#include "ResumeStore.h"

namespace {

void write(const QString &path, const QByteArray &content)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}

QJsonObject entriesOf(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object().value("entries").toObject();
}

QByteArray contentOf(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QString hashOf(const QString &plain)
{
    return QString::fromLatin1(QCryptographicHash::hash(plain.toUtf8(), QCryptographicHash::Sha256).toHex());
}

} // namespace

class TestResumeStore : public QObject
{
    Q_OBJECT

private slots:
    void init() { QVERIFY(m_dir.isValid()); m_path = m_dir.filePath(QString("resume-%1.json").arg(m_n++)); }

    void nothingToResumeBeforeFifteenSeconds()
    {
        ResumeStore s(m_path);
        s.update("a.mp4", 14.9, 276);
        QCOMPARE(s.position("a.mp4"), -1.0);
        s.update("a.mp4", 15, 276);
        QCOMPARE(s.position("a.mp4"), 15.0);
    }

    void finishedFilesAreForgotten()
    {
        ResumeStore s(m_path);
        s.update("a.mp4", 256, 276); // 20 s from the end still counts
        QCOMPARE(s.position("a.mp4"), 256.0);
        s.update("a.mp4", 256.1, 276); // past it removes the entry
        QCOMPARE(s.position("a.mp4"), -1.0);
    }

    void movingBackToTheStartForgetsToo()
    {
        ResumeStore s(m_path);
        s.update("a.mp4", 100, 276);
        s.update("a.mp4", 3, 276);
        QCOMPARE(s.position("a.mp4"), -1.0);
    }

    void passingByTheQuestionKeepsTheOldSpot()
    {
        ResumeStore s(m_path);
        s.update("a.mp4", 100, 276);
        s.update("a.mp4", 12, 276, true); // too early to be worth saving
        QCOMPARE(s.position("a.mp4"), 100.0);
        s.update("a.mp4", 20, 276, true); // this one is
        QCOMPARE(s.position("a.mp4"), 20.0);
        s.update("a.mp4", 260, 276, true); // finished still forgets
        QCOMPARE(s.position("a.mp4"), -1.0);
    }

    void unknownDurationIsIgnored()
    {
        ResumeStore s(m_path);
        s.update("a.mp4", 100, 0);
        s.update("a.mp4", 100, -1);
        QCOMPARE(s.position("a.mp4"), -1.0);
        s.update("a.mp4", 100, 276);
        s.update("a.mp4", 200, 0); // does not touch the saved spot either
        QCOMPARE(s.position("a.mp4"), 100.0);
    }

    void survivesSaveAndReload()
    {
        {
            ResumeStore s(m_path);
            s.update("a.mp4", 84.2, 276);
            s.update("b.mp4", 30, 600);
            s.save();
        }
        ResumeStore s(m_path);
        QCOMPARE(s.position("a.mp4"), 84.2);
        QCOMPARE(s.position("b.mp4"), 30.0);
        QCOMPARE(s.position("c.mp4"), -1.0);
    }

    void pathWithSpacesAndNonAsciiRoundTrips()
    {
        const QString path = QString::fromUtf8("C:/Videos/Çalışma klasörü/clip 1.mp4");
        {
            ResumeStore s(m_path);
            s.update(path, 60, 300);
            s.save();
        }
        QCOMPARE(ResumeStore(m_path).position(path), 60.0);
    }

    void destructorSavesPendingChanges()
    {
        {
            ResumeStore s(m_path);
            s.update("a.mp4", 90, 300);
        }
        QCOMPARE(ResumeStore(m_path).position("a.mp4"), 90.0);
    }

    void untouchedStoreWritesNothing()
    {
        { ResumeStore s(m_path); }
        QVERIFY(!QFile::exists(m_path));
    }

    void keepsTheNewestThreeHundred()
    {
        QJsonObject entries;
        for (int i = 0; i < 305; ++i)
            entries.insert(QString("f%1.mp4").arg(i), QJsonObject{{"pos", 50}, {"dur", 300}, {"at", 1000 + i}});
        write(m_path, QJsonDocument(QJsonObject{{"schema", 1}, {"entries", entries}}).toJson());

        ResumeStore s(m_path);
        QCOMPARE(s.position("f0.mp4"), 50.0); // all loaded; the cut happens on save
        s.save();

        const QJsonObject saved = entriesOf(m_path);
        QCOMPARE(saved.size(), 300);
        for (int i = 0; i < 5; ++i) QVERIFY(!saved.contains(hashOf(QString("f%1.mp4").arg(i))));
        QVERIFY(saved.contains(hashOf("f5.mp4")));
        QVERIFY(saved.contains(hashOf("f304.mp4")));
        QCOMPARE(s.position("f0.mp4"), -1.0);
    }

    void theFileHoldsNoFileNames()
    {
        {
            ResumeStore s(m_path);
            s.update("C:/Videos/secret clip.mp4", 84, 276);
            s.update("https://example.com/secret.mp4", 30, 276);
        }
        const QByteArray saved = contentOf(m_path);
        QVERIFY(!saved.isEmpty());
        QVERIFY(!saved.contains("secret"));
        QVERIFY(!saved.contains("Videos"));
        QVERIFY(!saved.contains("example"));
        ResumeStore s(m_path);
        QCOMPARE(s.position("C:/Videos/secret clip.mp4"), 84.0);
        QCOMPARE(s.position("https://example.com/secret.mp4"), 30.0);
    }

    void pathsFromAnOlderBuildTurnIntoHashes()
    {
#ifndef Q_OS_WIN
        QSKIP("A backslash is a file name character off Windows");
#endif
        write(m_path, R"({"schema": 1, "entries": {"C:/Videos/old clip.mp4": {"pos": 84, "dur": 276, "at": 1761930000}}})");
        {
            ResumeStore s(m_path);
            QCOMPARE(s.position("C:\\Videos\\old clip.mp4"), 84.0);
        } // nothing changed by hand, and it still saves: the path must not stay on disk
        QVERIFY(!contentOf(m_path).contains("old clip"));
        QCOMPARE(ResumeStore(m_path).position("C:/Videos/old clip.mp4"), 84.0);
    }

    void damagedFileGivesAnEmptyStore()
    {
        for (const char *content : {"{not json", "[]"}) {
            write(m_path, content);
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression("ResumeStore"));
            ResumeStore s(m_path);
            QCOMPARE(s.position("a.mp4"), -1.0);
            s.update("a.mp4", 100, 276); // and it recovers on the next save
            s.save();
            QCOMPARE(ResumeStore(m_path).position("a.mp4"), 100.0);
        }
    }

    void badEntryIsSkippedTheOthersLoad()
    {
        write(m_path, R"({"schema": 1, "entries": {
            "good.mp4": {"pos": 84.2, "dur": 276, "at": 1761930000},
            "bad.mp4": {"pos": "x", "dur": 276, "at": 1761930000},
            "missing.mp4": {"pos": 50},
            "notobject.mp4": 5,
            "alsogood.mp4": {"pos": 20, "dur": 100, "at": 1761930001}}})");
        ResumeStore s(m_path);
        QCOMPARE(s.position("good.mp4"), 84.2);
        QCOMPARE(s.position("alsogood.mp4"), 20.0);
        QCOMPARE(s.position("bad.mp4"), -1.0);
        QCOMPARE(s.position("missing.mp4"), -1.0);
        QCOMPARE(s.position("notobject.mp4"), -1.0);
    }

    void slashesDoNotSplitAFile()
    {
#ifndef Q_OS_WIN
        QSKIP("A backslash is a file name character off Windows");
#endif
        ResumeStore store(m_dir.filePath("slashes.json"));
        store.update("C:\\Videos\\clip.mp4", 84, 276);
        QCOMPARE(store.position("C:/Videos/clip.mp4"), 84.0);
        QCOMPARE(store.position("https://example.com/a.mp4"), -1.0);
    }

private:
    QTemporaryDir m_dir;
    QString m_path;
    int m_n = 0;
};

QTEST_GUILESS_MAIN(TestResumeStore)
#include "tst_resumestore.moc"
