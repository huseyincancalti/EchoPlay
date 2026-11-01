#include <QtTest>
#include "AppState.h"
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
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_user = m_dir.filePath(QString("user-%1.json").arg(m_n));
        m_statePath = m_dir.filePath(QString("state-%1.json").arg(m_n++));
        m_state = std::make_unique<AppState>(m_statePath);
    }

    void noUserFileGivesTheDefaults()
    {
        QTest::failOnWarning(); // a missing user file is the normal case, not a warning
        QVERIFY(!shippedDefaults().isEmpty());
        QCOMPARE(tokensFor(m_user), shippedDefaults());
        QCOMPARE(tokensFor(QString()), shippedDefaults());
    }

    void partialUserFileChangesOnlyItsKeys()
    {
        QTest::failOnWarning();
        // Not a raw string: moc loses the class after a raw string that contains '#'.
        write(m_user, "{\"accent\": \"#00FF00\", \"radius\": {\"lg\": 20}}");
        const QVariantMap defaults = shippedDefaults();
        const QVariantMap t = tokensFor(m_user);

        QCOMPARE(t.value("accent").toString(), QString("#00FF00"));
        QCOMPARE(tokenAt(t, "radius.lg").toInt(), 20);
        QCOMPARE(tokenAt(t, "radius.sm"), tokenAt(defaults, "radius.sm"));
        QCOMPARE(tokenAt(t, "radius.xl"), tokenAt(defaults, "radius.xl"));
        QCOMPARE(t.value("space"), defaults.value("space"));
        QCOMPARE(t.value("dark"), defaults.value("dark"));
    }

    void aTokenOfTheWrongTypeIsIgnored()
    {
        write(m_user, R"({"radius": "big", "accent": 5, "blur": {"panel": "wide"}, "glass": {"bar": 0.5}})");
        const QVariantMap defaults = shippedDefaults();
        const QVariantMap t = tokensFor(m_user);

        QCOMPARE(t.value("radius"), defaults.value("radius")); // still the number map
        QCOMPARE(t.value("accent"), defaults.value("accent"));
        QCOMPARE(tokenAt(t, "blur.panel"), tokenAt(defaults, "blur.panel"));
        QCOMPARE(tokenAt(t, "glass.bar").toDouble(), 0.5); // the valid sibling still applies
        QCOMPARE(tokenAt(t, "glass.fill"), tokenAt(defaults, "glass.fill"));
    }

    void unknownTokensAreIgnored()
    {
        write(m_user, R"({"nope": 1, "radius": {"huge": 99}})");
        QCOMPARE(tokensFor(m_user), shippedDefaults());
    }

    void brokenJsonAndNonObjectRootGiveTheDefaults()
    {
        const QStringList bodies = {"{broken", "[1, 2]", ""};
        for (int i = 0; i < bodies.size(); ++i) {
            const QString path = m_dir.filePath(QString("bad-%1.json").arg(i));
            write(path, bodies.at(i).toUtf8());
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QString("bad-%1\\.json").arg(i)));
            QCOMPARE(tokensFor(path), shippedDefaults());
        }
    }

    void shippedFileHasEveryToken()
    {
        const QVariantMap t = tokensFor(QString());
        const QStringList colors = {"bg", "surface", "panel", "panelOpacity", "border", "hairline", "text", "textMuted",
                                    "control", "controlStrong", "selected", "track", "success", "warning", "danger"};
        QStringList paths = {
            "glass.fill", "glass.bar", "glass.popover", "glass.strengthStep", "glass.border", "glass.hairline",
            "glass.text", "glass.textMuted", "glass.control", "glass.controlSubtle", "glass.track", "glass.trackOff",
            "glass.trackSoft", "glass.trackHover", "glass.mark", "glass.line",
            "glass.success", "glass.warning", "glass.danger",
            "accent", "onAccent.dark", "onAccent.light",
            "radius.sm", "radius.md", "radius.lg", "radius.xl", "radius.pill", "blur.panel", "blur.overlay",
            "space.xs", "space.sm", "space.md", "space.lg", "space.xl", "space.xxl", "space.xxxl",
            "type.caption", "type.small", "type.body", "type.label", "type.title", "type.heading", "type.display",
            "icon.sm", "icon.md", "icon.lg", "motion.fast", "motion.base", "motion.sweep", "motion.jolt",
            "shadow.y", "shadow.blur", "shadow.opacity", "bar.style", "bar.inset", "font.family", "font.file",
        };
        for (const QString &color : colors) paths << "dark." + color << "light." + color;
        for (const QString &path : paths) QVERIFY2(tokenAt(t, path).isValid(), qPrintable("missing token " + path));
    }

    void modeSwitchesTheColors()
    {
        Theme theme(m_state.get(), ECHOPLAY_THEME_JSON, QString(), {});
        const QVariantMap defaults = shippedDefaults();
        QCOMPARE(theme.mode(), QString("dark")); // the first run, before anyone chose
        QCOMPARE(theme.colors(), defaults.value("dark").toMap());

        QSignalSpy changed(&theme, &Theme::modeChanged);
        theme.setMode("light");
        QVERIFY(!theme.dark());
        QCOMPARE(theme.colors(), defaults.value("light").toMap());
        theme.setMode("blue"); // not a mode: ignored
        QCOMPARE(theme.mode(), QString("light"));
        QCOMPARE(changed.count(), 1);

        theme.setSystemDark(true); // no effect while a mode is forced
        QCOMPARE(changed.count(), 1);
        theme.setMode("system");
        QVERIFY(theme.dark());
        theme.setSystemDark(false);
        QVERIFY(!theme.dark());
        QCOMPARE(changed.count(), 3);
    }

    void accentAndItsTextByContrast()
    {
        Theme theme(m_state.get(), ECHOPLAY_THEME_JSON, QString(), {});
        const QColor darkText(tokenAt(theme.tokens(), "onAccent.dark").toString());
        const QColor lightText(tokenAt(theme.tokens(), "onAccent.light").toString());

        QCOMPARE(theme.accent(), QColor("#FF7D26")); // the token
        QCOMPARE(theme.onAccent(), darkText);         // 7.5:1; white would be 2.6:1

        QSignalSpy changed(&theme, &Theme::accentChanged);
        theme.setAccentChoice("#14213D"); // a dark accent
        QCOMPARE(theme.accent(), QColor("#14213D"));
        QCOMPARE(theme.onAccent(), lightText);
        theme.setAccentChoice("not a color");
        QCOMPARE(theme.accent(), QColor("#14213D"));
        QCOMPARE(changed.count(), 1);
        theme.setAccentChoice(QString()); // back to the token
        QCOMPARE(theme.accent(), QColor("#FF7D26"));
    }

    void glassStrengthMovesTheOpacity()
    {
        Theme theme(m_state.get(), ECHOPLAY_THEME_JSON, QString(), {});
        QCOMPARE(theme.glassStrength(), 1);
        QCOMPARE(theme.barOpacity(), 0.78);
        QCOMPARE(theme.popoverOpacity(), 0.84);
        theme.setGlassStrength(2); // more of the video shows through
        QCOMPARE(theme.barOpacity(), 0.66);
        theme.setGlassStrength(0);
        QCOMPARE(theme.popoverOpacity(), 0.96);
        theme.setGlassStrength(9);
        QCOMPARE(theme.glassStrength(), 2);
    }

    void choicesWinOverTokensAndAreSaved()
    {
        write(m_user, R"({"bar": {"style": "full"}, "font": {"family": "Manrope"}})");
        {
            Theme theme(m_state.get(), ECHOPLAY_THEME_JSON, m_user, {});
            QCOMPARE(theme.barStyle(), QString("full"));       // the user's theme file
            QCOMPARE(theme.fontFamily(), QString("Manrope"));
            theme.setBarStyle("island");
            theme.setBarStyle("floating"); // not a style: ignored
            theme.setFontFamily("Plus Jakarta Sans");
            theme.setFontFamily("No Such Font");
            theme.setMode("light");
            theme.setAccentChoice("#3E7BFA");
            theme.setGlassStrength(2);
            m_state->save();
        }
        AppState again(m_statePath);
        Theme theme(&again, ECHOPLAY_THEME_JSON, m_user, {});
        QCOMPARE(theme.barStyle(), QString("island"));
        QCOMPARE(theme.fontFamily(), QString("Plus Jakarta Sans"));
        QCOMPARE(theme.mode(), QString("light"));
        QCOMPARE(theme.accent(), QColor("#3E7BFA"));
        QCOMPARE(theme.glassStrength(), 2);
    }

    void damagedChoicesKeepDefaults()
    {
        write(m_statePath, R"({"schema":1,"appearance":{"mode":"neon","accent":"nope","bar":7,"font":["x"],"glass":12}})");
        AppState damaged(m_statePath);
        Theme theme(&damaged, ECHOPLAY_THEME_JSON, QString(), {});
        QCOMPARE(theme.mode(), QString("dark"));
        QCOMPARE(theme.accent(), QColor("#FF7D26"));
        QCOMPARE(theme.barStyle(), QString("island"));
        QCOMPARE(theme.fontFamily(), QString("Geist"));
        QCOMPARE(theme.glassStrength(), 1);
    }

private:
    QVariantMap tokensFor(const QString &user) { return Theme(m_state.get(), ECHOPLAY_THEME_JSON, user, {}).tokens(); }

    QTemporaryDir m_dir;
    QString m_user;
    QString m_statePath;
    std::unique_ptr<AppState> m_state;
    int m_n = 0;
};

QTEST_GUILESS_MAIN(TestTheme)
#include "tst_theme.moc"
