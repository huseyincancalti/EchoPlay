#include <QtTest>
#include "Theme.h"

namespace {

void write(const QString &path, const QByteArray &content)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}

QVariantMap shippedDefaults()
{
    QFile f(ECHOPLAY_THEME_JSON);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object().toVariantMap();
}

// The token at a dotted path ("radius.lg"); invalid when any part is missing.
QVariant tokenAt(const QVariantMap &tokens, const QString &path)
{
    QVariant v = tokens;
    for (const QString &part : path.split('.')) {
        const QVariantMap map = v.toMap();
        if (!map.contains(part)) return {};
        v = map.value(part);
    }
    return v;
}

} // namespace

class TestTheme : public QObject
{
    Q_OBJECT

private slots:
    void init() { QVERIFY(m_dir.isValid()); m_user = m_dir.filePath(QString("user-%1.json").arg(m_n++)); }

    void noUserFileGivesTheDefaults()
    {
        QTest::failOnWarning(); // a missing user file is the normal case, not a warning
        QVERIFY(!shippedDefaults().isEmpty());
        QCOMPARE(Theme(ECHOPLAY_THEME_JSON, m_user).tokens(), shippedDefaults());
        QCOMPARE(Theme(ECHOPLAY_THEME_JSON, QString()).tokens(), shippedDefaults());
    }

    void partialUserFileChangesOnlyItsKeys()
    {
        QTest::failOnWarning();
        // Not a raw string: moc loses the class after a raw string that contains '#'.
        write(m_user, "{\"accent\": \"#00FF00\", \"radius\": {\"lg\": 20}}");
        const QVariantMap defaults = shippedDefaults();
        const QVariantMap t = Theme(ECHOPLAY_THEME_JSON, m_user).tokens();

        QCOMPARE(t.value("accent").toString(), QString("#00FF00"));
        QCOMPARE(tokenAt(t, "radius.lg").toInt(), 20);
        QCOMPARE(tokenAt(t, "radius.sm"), tokenAt(defaults, "radius.sm"));
        QCOMPARE(tokenAt(t, "radius.xl"), tokenAt(defaults, "radius.xl"));
        QCOMPARE(t.value("spacing"), defaults.value("spacing"));
        QCOMPARE(t.value("bg"), defaults.value("bg"));
    }

    void aTokenOfTheWrongTypeIsIgnored()
    {
        write(m_user, R"({"radius": "big", "accent": 5, "blur": {"panel": "wide"}, "glass": {"opacity": 0.5}})");
        const QVariantMap defaults = shippedDefaults();
        const QVariantMap t = Theme(ECHOPLAY_THEME_JSON, m_user).tokens();

        QCOMPARE(t.value("radius"), defaults.value("radius")); // still the number map
        QCOMPARE(t.value("accent"), defaults.value("accent"));
        QCOMPARE(tokenAt(t, "blur.panel"), tokenAt(defaults, "blur.panel"));
        QCOMPARE(tokenAt(t, "glass.opacity").toDouble(), 0.5); // the valid sibling still applies
        QCOMPARE(tokenAt(t, "glass.fill"), tokenAt(defaults, "glass.fill"));
    }

    void unknownTokensAreIgnored()
    {
        write(m_user, R"({"nope": 1, "radius": {"huge": 99}})");
        QCOMPARE(Theme(ECHOPLAY_THEME_JSON, m_user).tokens(), shippedDefaults());
    }

    void brokenJsonAndNonObjectRootGiveTheDefaults()
    {
        const QStringList bodies = {"{broken", "[1, 2]", ""};
        for (int i = 0; i < bodies.size(); ++i) {
            const QString path = m_dir.filePath(QString("bad-%1.json").arg(i));
            write(path, bodies.at(i).toUtf8());
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QString("bad-%1\\.json").arg(i)));
            QCOMPARE(Theme(ECHOPLAY_THEME_JSON, path).tokens(), shippedDefaults());
        }
    }

    void shippedFileHasEveryToken()
    {
        const QVariantMap t = Theme(ECHOPLAY_THEME_JSON, QString()).tokens();
        const QStringList paths = {
            "bg", "text", "textMuted", "accent", "onAccent", "success", "warning", "danger",
            "glass.fill", "glass.opacity", "glassBorder", "blur.panel",
            "radius.sm", "radius.md", "radius.lg", "radius.xl",
            "spacing.xs", "spacing.sm", "spacing.md", "spacing.lg", "spacing.xl",
            "font.family", "font.caption", "font.body", "font.title", "font.display",
            "icon.sm", "icon.md", "icon.lg", "motion.fast", "motion.base",
        };
        for (const QString &path : paths) QVERIFY2(tokenAt(t, path).isValid(), qPrintable("missing token " + path));
    }

private:
    QTemporaryDir m_dir;
    QString m_user;
    int m_n = 0;
};

QTEST_GUILESS_MAIN(TestTheme)
#include "tst_theme.moc"
