#include <QtTest>
#include "I18n.h"

namespace {

void write(const QString &dir, const QString &name, const QByteArray &content)
{
    QFile f(dir + "/" + name);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}

QJsonObject readJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

QVariantMap lang(const QString &code, const QString &name)
{
    return {{"code", code}, {"name", name}};
}

} // namespace

class TestI18n : public QObject
{
    Q_OBJECT

private slots:
    void init() { QVERIFY(m_dir.isValid()); m_path = m_dir.filePath(QString("d%1").arg(m_n++)); QVERIFY(QDir().mkpath(m_path)); }

    void languagesAreSortedAndBrokenFilesSkipped()
    {
        write(m_path, "tr.json", R"({"lang_name": "Türkçe"})");
        write(m_path, "en.json", R"({"lang_name": "English"})");
        write(m_path, "de.json", R"({"lang_name": "Deutsch"})");
        write(m_path, "broken.json", "{not json");
        write(m_path, "noname.json", R"({"mixer": "x"})");
        write(m_path, "notes.txt", "not a language file");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("skipping broken\.json"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("skipping noname\.json"));

        I18n i18n(m_path);
        QCOMPARE(i18n.languages(), (QVariantList{lang("de", "Deutsch"), lang("en", "English"),
                                                  lang("tr", QString::fromUtf8("Türkçe"))}));
    }

    void missingFolderHasNoLanguages()
    {
        I18n i18n(m_path + "/nope");
        QVERIFY(i18n.languages().isEmpty());
    }

    void missingKeyFallsBackToEnglish()
    {
        write(m_path, "en.json", R"({"lang_name": "English", "a": "A", "b": "B"})");
        write(m_path, "tr.json", R"({"lang_name": "Turkce", "a": "Aa"})");
        I18n i18n(m_path);

        i18n.setLanguage("tr");
        QCOMPARE(i18n.language(), QString("tr"));
        QCOMPARE(i18n.strings().value("a").toString(), QString("Aa"));
        QCOMPARE(i18n.strings().value("b").toString(), QString("B")); // not in tr: English, not empty
        QCOMPARE(i18n.strings().value("lang_name").toString(), QString("Turkce"));

        i18n.setLanguage("en");
        QCOMPARE(i18n.strings().value("a").toString(), QString("A"));
    }

    void onlyStringValuesAreKept()
    {
        write(m_path, "en.json", R"({"lang_name": "English", "a": "A", "n": 5, "o": {"x": "y"}})");
        I18n i18n(m_path);
        i18n.setLanguage("en");
        QVERIFY(i18n.strings().contains("a"));
        QVERIFY(!i18n.strings().contains("n"));
        QVERIFY(!i18n.strings().contains("o"));
    }

    void unknownOrEmptyCodeFallsBackToSystemThenEnglish()
    {
        const QString system = QLocale::system().name().left(2);
        write(m_path, "en.json", R"({"lang_name": "English"})");
        write(m_path, system + ".json", R"({"lang_name": "System"})");
        I18n withSystem(m_path);
        withSystem.setLanguage("zz");
        QCOMPARE(withSystem.language(), system);
        withSystem.setLanguage({});
        QCOMPARE(withSystem.language(), system);

        const QString other = m_dir.filePath("other");
        QVERIFY(QDir().mkpath(other));
        write(other, "en.json", R"({"lang_name": "English"})");
        write(other, "xx.json", R"({"lang_name": "Other"})"); // not the system language
        I18n noSystem(other);
        noSystem.setLanguage("zz");
        QCOMPARE(noSystem.language(), QString("en"));
        noSystem.setLanguage({});
        QCOMPARE(noSystem.language(), QString("en"));
    }

    void languageChangedOnlyWhenTheLanguageChanges()
    {
        write(m_path, "en.json", R"({"lang_name": "English"})");
        write(m_path, "xx.json", R"({"lang_name": "Other"})");
        I18n i18n(m_path);
        i18n.setLanguage("en"); // the system language may already be "en", so start from a known state
        QSignalSpy spy(&i18n, &I18n::languageChanged);

        i18n.setLanguage("xx");
        QCOMPARE(spy.count(), 1);
        i18n.setLanguage("xx");
        QCOMPARE(spy.count(), 1);
        i18n.setLanguage("en");
        QCOMPARE(spy.count(), 2);
        i18n.setLanguage("zz"); // unknown, resolves to English again
        QCOMPARE(spy.count(), 2);
    }

    void trackLanguagesReadAsTheirOwnNames()
    {
        QCOMPARE(languageName("eng"), QString("English"));
        QCOMPARE(languageName("en-US"), QString("English"));
        QCOMPARE(languageName("tur"), QString("Türkçe"));
        QCOMPARE(languageName("tr"), QString("Türkçe"));
        QCOMPARE(languageName("jpn"), QString("日本語"));
        QCOMPARE(languageName("ger"), languageName("deu"));
        QCOMPARE(languageName("spa"), QString("Español"));
        QCOMPARE(languageName("chi"), QString("中文"));
        QCOMPARE(languageName("rus"), QString("Русский"));
        QCOMPARE(languageName("nor"), QString("Norsk"));
        QCOMPARE(languageName("und"), QString());
        QCOMPARE(languageName(""), QString());
        QCOMPARE(languageName("qqx"), QString("QQX"));
    }

    void shippedFilesParseAndHaveTheSameKeys()
    {
        const QJsonObject en = readJson(QString(ECHOPLAY_LANG_DIR) + "/en.json");
        const QJsonObject tr = readJson(QString(ECHOPLAY_LANG_DIR) + "/tr.json");
        QVERIFY(!en.isEmpty());
        QVERIFY(!tr.isEmpty());
        QVERIFY(!en.value("lang_name").toString().isEmpty());
        QVERIFY(!tr.value("lang_name").toString().isEmpty());
        QStringList enKeys = en.keys(), trKeys = tr.keys();
        enKeys.sort();
        trKeys.sort();
        QCOMPARE(trKeys, enKeys);
    }

private:
    QTemporaryDir m_dir;
    QString m_path;
    int m_n = 0;
};

QTEST_GUILESS_MAIN(TestI18n)
#include "tst_i18n.moc"
