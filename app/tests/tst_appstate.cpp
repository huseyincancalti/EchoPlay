#include <QtTest>
#include "AppState.h"

namespace {

void write(const QString &path, const QByteArray &content)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}

QJsonObject read(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

MixerPrefs somePrefs()
{
    MixerPrefs p;
    p.defaultOn = "1,2";
    p.gains = {{2, 1.5}, {3, 0.25}};
    p.mono = {2, 1};
    return p;
}

} // namespace

class TestAppState : public QObject
{
    Q_OBJECT

private slots:
    void init() { QVERIFY(m_dir.isValid()); m_path = m_dir.filePath(QString("state-%1.json").arg(m_n++)); }

    void firstRunUsesDefaultsAndWritesNothing()
    {
        {
            AppState s(m_path);
            QCOMPARE(s.language(), QString());
            QCOMPARE(s.mixer(), MixerPrefs{});
        }
        QVERIFY(!QFile::exists(m_path));
    }

    void roundTrip()
    {
        {
            AppState s(m_path);
            s.setLanguage("tr");
            s.setMixer(somePrefs());
            s.setVolume(65);
            s.setQuality("low");
            s.setUpdateLastChecked(1761930000);
            s.save();
        }
        const QJsonObject json = read(m_path);
        QCOMPARE(json.value("schema").toInt(), AppState::Schema);
        QCOMPARE(json.value("mixer").toObject().value("mono").toArray(), (QJsonArray{1, 2})); // sorted
        AppState again(m_path);
        QCOMPARE(again.language(), QString("tr"));
        QCOMPARE(again.mixer(), somePrefs());
        QCOMPARE(again.volume(), 65.0);
        QCOMPARE(again.quality(), QString("low"));
        QCOMPARE(again.updateLastChecked(), 1761930000);
    }

    void changeSavesShortlyAfter()
    {
        AppState s(m_path);
        QSignalSpy spy(&s, &AppState::languageChanged);
        s.setLanguage("tr");
        s.setLanguage("tr"); // same value: no second signal
        QCOMPARE(spy.count(), 1);
        QVERIFY(!QFile::exists(m_path)); // not on every change, a moment later
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(m_path), 3000);
        QCOMPARE(read(m_path).value("language").toString(), QString("tr"));
    }

    void pendingSaveIsWrittenOnExit()
    {
        {
            AppState s(m_path);
            s.setMixer(somePrefs());
        } // destroyed before the save timer fires
        QCOMPARE(AppState(m_path).mixer(), somePrefs());
    }

    void createsTheConfigFolder()
    {
        const QString nested = m_dir.filePath("EchoPlay/sub/state.json");
        {
            AppState s(nested);
            s.setLanguage("en");
            s.save();
        }
        QCOMPARE(AppState(nested).language(), QString("en"));
    }

    void damagedFileUsesDefaults_data()
    {
        QTest::addColumn<QByteArray>("content");
        QTest::newRow("not json") << QByteArray("{\"language\": \"tr\",");
        QTest::newRow("array root") << QByteArray("[1, 2]");
        QTest::newRow("empty") << QByteArray("");
    }
    void damagedFileUsesDefaults()
    {
        QFETCH(QByteArray, content);
        write(m_path, content);
        AppState s(m_path);
        QCOMPARE(s.language(), QString());
        QCOMPARE(s.mixer(), MixerPrefs{});
    }

    void badFieldsAreSkippedOneByOne()
    {
        write(m_path, R"({"schema": 1, "language": 5, "volume": 900, "quality": "max", "update": {"last_checked": "yesterday"},
            "mixer": {"default_on": "1,x", "gains": {"x": 2, "2": "loud", "3": 9, "4": 0.5}, "mono": ["a", 2]}})");
        AppState s(m_path);
        QCOMPARE(s.language(), QString());
        QCOMPARE(s.mixer().defaultOn, QString("1"));
        QCOMPARE(s.mixer().gains, (QMap<int, double>{{4, 0.5}}));
        QCOMPARE(s.mixer().mono, QSet<int>{2});
        QCOMPARE(s.volume(), 100.0);
        QCOMPARE(s.quality(), QString("auto"));
        QCOMPARE(s.updateLastChecked(), 0);
    }

    void newerSchemaKeepsKnownFields()
    {
        write(m_path, R"({"schema": 99, "language": "tr", "mixer": {"default_on": "all"}, "future": true})");
        AppState s(m_path);
        QCOMPARE(s.language(), QString("tr"));
        QCOMPARE(s.mixer().defaultOn, QString("all"));
    }

    void mixerPrefsComeBackIntoTheMixer()
    {
        {
            AppState s(m_path);
            s.setMixer(somePrefs());
        }
        Mixer m;
        m.setTracks({QVariantMap{{"type", "audio"}, {"id", 1}}, QVariantMap{{"type", "audio"}, {"id", 2}},
                     QVariantMap{{"type", "audio"}, {"id", 3}}});
        m.applyPrefs(AppState(m_path).mixer());
        QCOMPARE(m.enabledTracks(), (QList<int>{1, 2}));
        QCOMPARE(m.gain(2), 1.5);
        QVERIFY(m.isMono(1));
    }

private:
    QTemporaryDir m_dir;
    QString m_path;
    int m_n = 0;
};

QTEST_GUILESS_MAIN(TestAppState)
#include "tst_appstate.moc"
