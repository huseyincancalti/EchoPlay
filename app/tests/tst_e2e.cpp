#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QtQml/qqmlextensionplugin.h>
#include <clocale>
#include "MpvItem.h"
#include "Services.h"

Q_IMPORT_QML_PLUGIN(EchoPlayPlugin)

// End-to-end: the real player (Main.qml, MpvItem, the singletons) on a real two-track clip, driven the way a
// user drives it, with key presses and clicks; checks what mpv does and what is saved. It opens a window, so
// tools/e2e.sh runs it, never tools/check.sh. Synthetic input goes to the test window only, not to the OS.
// The clip comes from ECHOPLAY_E2E_CLIP (tools/e2e.sh generates one: tracks "System sounds", "Microphone").

namespace {

const QString Mix = "[aid1]volume=1.00[ev1];[aid2]volume=1.00[ev2];[ev1][ev2]amix=inputs=2:normalize=0[ao]";

// One start of the player, as main.cpp does it (without the single-instance pipe and the update check).
struct Launch
{
    Launch(const QString &configDir, const QString &clip, const QString &log)
        : services(configDir, QCoreApplication::applicationDirPath(), "0.0.0-dev")
    {
        engine.setInitialProperties({{"file", clip}, {"benchSeconds", 0}, {"glass", true}, {"mix", false},
                                     {"logFile", log}, {"mixToggle", false}, {"shotAt", -1.0},
                                     {"shotOut", QString()}, {"startup", QVariantMap{}},
                                     {"instance", QVariant::fromValue<QObject *>(nullptr)}});
        engine.loadFromModule("EchoPlay", "Main");
        window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
        if (window) mpv = window->findChild<MpvItem *>();
    }

    // Waits until the clip plays (Main.qml opens it paused and unpauses on the first frame on screen).
    bool playing()
    {
        QSignalSpy restarted(mpv, &MpvItem::playbackRestarted);
        if (!restarted.wait(20000)) return false;
        mpv->setProperty("mute", true);
        return QTest::qWaitFor([this] { return !mpv->getProperty("pause").toBool(); }, 5000);
    }

    QString prop(const char *name) const { return mpv->getProperty(name).toString(); }
    // mpv reads aid back as a number, or as the flag false for "no".
    QString aid() const
    {
        const QVariant v = mpv->getProperty("aid");
        return v.typeId() == QMetaType::Bool ? (v.toBool() ? "yes" : "no") : v.toString();
    }
    double number(const char *name) const { return mpv->getProperty(name).toDouble(); }
    void key(Qt::Key k, Qt::KeyboardModifiers mods = Qt::NoModifier) { QTest::keyClick(window, k, mods); }

    Services services; // destroyed after the engine: its destructors write pending state
    QQmlApplicationEngine engine;
    QQuickWindow *window = nullptr;
    MpvItem *mpv = nullptr;
};

int voInits(const QString &log)
{
    QFile f(log);
    if (!f.open(QIODevice::ReadOnly)) return -1;
    return QString::fromUtf8(f.readAll()).count("][i][cplayer] VO: [");
}

} // namespace

class TestE2E : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_clip = qEnvironmentVariable("ECHOPLAY_E2E_CLIP");
        QVERIFY2(QFileInfo::exists(m_clip), "ECHOPLAY_E2E_CLIP must name a clip with two audio tracks (tools/e2e.sh)");
        // ECHOPLAY_E2E_DIR (tools/e2e.sh): keep state and mpv logs there to read after a failure.
        const QString keep = qEnvironmentVariable("ECHOPLAY_E2E_DIR");
        if (!keep.isEmpty()) {
            QDir(keep).removeRecursively();
            QVERIFY(QDir().mkpath(keep));
            m_path = keep;
        } else {
            QVERIFY(m_dir.isValid());
            m_path = m_dir.path();
        }
    }

    void firstRunAndKeys()
    {
        const QString log = QDir(m_path).filePath("run1.log");
        {
            Launch app(m_path, m_clip, log);
            QVERIFY(app.mpv);
            QVERIFY2(app.playing(), "the clip did not start playing");
            QTRY_COMPARE(app.window->title(), QFileInfo(m_clip).fileName()); // media-title

            // Mixer: first run plays track 1 alone (v1's default).
            QTRY_COMPARE(app.aid(), QString("1"));
            QCOMPARE(app.prop("lavfi-complex"), QString());
            app.key(Qt::Key_2, Qt::ControlModifier); // mic on: both tracks mixed
            QTRY_COMPARE(app.prop("lavfi-complex"), Mix);
            QCOMPARE(app.aid(), QString("no"));
            app.key(Qt::Key_1, Qt::ControlModifier); // game off: the mic alone needs no graph
            QTRY_COMPARE(app.aid(), QString("2"));
            QCOMPARE(app.prop("lavfi-complex"), QString());
            app.key(Qt::Key_3, Qt::ControlModifier); // no third track: nothing changes
            QTest::qWait(300);
            QCOMPARE(app.aid(), QString("2"));

            // Volume (Down x3) and its memory.
            for (int i = 0; i < 3; ++i) app.key(Qt::Key_Down);
            QTRY_COMPARE(app.number("volume"), 85.0);
            QTRY_COMPARE(app.services.state.volume(), 85.0);

            // Pause, speed, seek.
            app.key(Qt::Key_Space);
            QTRY_VERIFY(app.mpv->getProperty("pause").toBool());
            app.key(Qt::Key_Space);
            QTRY_VERIFY(!app.mpv->getProperty("pause").toBool());
            app.key(Qt::Key_G);
            QTRY_COMPARE(app.number("speed"), 2.0);
            app.key(Qt::Key_G);
            QTRY_COMPARE(app.number("speed"), 1.0);
            app.key(Qt::Key_D);
            QTRY_COMPARE(app.number("speed"), 1.1);
            app.key(Qt::Key_S);
            QTRY_COMPARE(app.number("speed"), 1.0);
            const double before = app.number("time-pos");
            app.key(Qt::Key_Right);
            QTRY_VERIFY(app.number("time-pos") > before + 8);
            app.key(Qt::Key_Left);
            QTRY_VERIFY(app.number("time-pos") < before + 5);

            // Full screen: F and Esc, then double-click twice.
            app.key(Qt::Key_F);
            QTRY_COMPARE(app.window->visibility(), QWindow::FullScreen);
            app.key(Qt::Key_Escape);
            QTRY_COMPARE(app.window->visibility(), QWindow::Windowed);
            const QPoint center(app.window->width() / 2, app.window->height() / 2);
            QTest::mouseDClick(app.window, Qt::LeftButton, Qt::NoModifier, center);
            QTRY_COMPARE(app.window->visibility(), QWindow::FullScreen);
            QTest::mouseDClick(app.window, Qt::LeftButton, Qt::NoModifier, center);
            QTRY_COMPARE(app.window->visibility(), QWindow::Windowed);
        } // closed: pending state is written now

        QCOMPARE(voInits(log), 1); // every mixer switch kept the video running (no VO re-init)
        QFile f(QDir(m_path).filePath("state.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonObject state = QJsonDocument::fromJson(f.readAll()).object();
        QCOMPARE(state.value("mixer").toObject().value("default_on").toString(), QString("2"));
        QCOMPARE(state.value("volume").toDouble(), 85.0);
    }

    void secondRunRestoresMixerAndVolume()
    {
        Launch app(m_path, m_clip, QDir(m_path).filePath("run2.log"));
        QVERIFY(app.mpv);
        QVERIFY2(app.playing(), "the clip did not start playing");
        QTRY_COMPARE(app.aid(), QString("2"));
        QCOMPARE(app.prop("lavfi-complex"), QString());
        QCOMPARE(app.number("volume"), 85.0);
    }

private:
    QString m_clip;
    QTemporaryDir m_dir;
    QString m_path; // state and logs
};

int main(int argc, char *argv[])
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11); // as main.cpp: MpvItem needs D3D11
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C"); // libmpv requires it
    TestE2E test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_e2e.moc"
