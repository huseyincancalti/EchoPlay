#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQuickImageProvider>
#include <QQuickWindow>
#include <QStandardPaths>
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

// Real input still reaches the test window when it opens under the maintainer's cursor or takes the focus:
// a scroll there turned the volume down 13 steps before the test pressed a key. The test sends no wheel
// events, and its key events carry no native scan code, so real ones are dropped (and counted in the log).
class RealInputFilter : public QObject
{
public:
    int dropped = 0;

protected:
    bool eventFilter(QObject *, QEvent *e) override
    {
        switch (e->type()) {
        case QEvent::Wheel:
            break;
        case QEvent::ShortcutOverride: // accepted below, so a real key fires no shortcut
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
            if (static_cast<QKeyEvent *>(e)->nativeScanCode() == 0) return false;
            break;
        default:
            return false;
        }
        ++dropped;
        e->accept();
        return true;
    }
};

// One start of the player, as main.cpp does it (without the single-instance pipe and the update check).
struct Launch
{
    Launch(const QString &configDir, const QString &clip, const QString &log)
        : services(configDir, QCoreApplication::applicationDirPath(), "0.0.0-dev")
    {
        qApp->installEventFilter(&realInput);
        engine.setInitialProperties({{"file", clip}, {"benchSeconds", 0}, {"glass", true}, {"mix", false},
                                     {"logFile", log}, {"mixToggle", false}, {"thumbScrub", false}, {"tierCycle", false}, {"shotAt", -1.0},
                                     {"shotOut", QString()}, {"startup", QVariantMap{}},
                                     {"instance", QVariant::fromValue<QObject *>(nullptr)}});
        engine.loadFromModule("EchoPlay", "Main");
        window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
        if (window) mpv = window->findChild<MpvItem *>();
        // Read like the UI does: observed values, never a synchronous get that could stall the render loop.
        if (mpv) {
            for (const char *name : {"aid", "lavfi-complex", "pause", "volume", "speed", "time-pos", "scale", "dither",
                                     "duration", "keep-open", "loop-file", "screenshot-dir", "vo-configured"})
                mpv->observeProperty(name);
        }
    }

    // Waits until the clip plays with video (Main.qml opens it paused, audio first, turns video on once the
    // render context exists, and unpauses on the first frame on screen). "pause" alone is not enough: before
    // the file loads it reads the global "no".
    bool playing()
    {
        QSignalSpy restarted(mpv, &MpvItem::playbackRestarted);
        if (!restarted.wait(20000)) return false;
        mpv->setProperty("mute", true);
        return QTest::qWaitFor([this] { return prop("vo-configured") == "yes" && prop("pause") == "no" && number("time-pos") > 0.3; }, 10000);
    }

    // mpv reads "no" back as the flag false (aid=no, dither=no): compare as the strings that were set.
    QString prop(const char *name) const
    {
        const QVariant v = mpv->observed(name);
        return v.typeId() == QMetaType::Bool ? (v.toBool() ? "yes" : "no") : v.toString();
    }
    double number(const char *name) const { return mpv->observed(name).toDouble(); }
    void key(Qt::Key k, Qt::KeyboardModifiers mods = Qt::NoModifier) { QTest::keyClick(window, k, mods); }

    ~Launch()
    {
        if (realInput.dropped) qInfo("ignored %d real wheel/key events", realInput.dropped);
    }

    RealInputFilter realInput;
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
            QTRY_COMPARE(app.prop("aid"), QString("1"));
            QCOMPARE(app.prop("lavfi-complex"), QString());
            app.key(Qt::Key_2, Qt::ControlModifier); // mic on: both tracks mixed
            QTRY_COMPARE(app.prop("lavfi-complex"), Mix);
            QCOMPARE(app.prop("aid"), QString("no"));
            app.key(Qt::Key_1, Qt::ControlModifier); // game off: the mic alone needs no graph
            QTRY_COMPARE(app.prop("aid"), QString("2"));
            QCOMPARE(app.prop("lavfi-complex"), QString());
            app.key(Qt::Key_3, Qt::ControlModifier); // no third track: nothing changes
            QTest::qWait(300);
            QCOMPARE(app.prop("aid"), QString("2"));

            // Seek-bar preview: the hidden player follows the playing file; its picture reaches QML through
            // the image provider, and the video plays on (one VO init, checked after the window closes).
            Thumbnailer &thumbs = app.services.thumbnails;
            QTRY_COMPARE(QDir::fromNativeSeparators(thumbs.file()), m_clip); // the folder playlist's path
            thumbs.request(40);
            QTRY_COMPARE_WITH_TIMEOUT(thumbs.image().size(), QSize(Thumbnailer::MaxSize, 203), 10000); // 1280x720
            auto *provider = static_cast<QQuickImageProvider *>(app.engine.imageProvider("thumbnail"));
            QVERIFY(provider);
            QCOMPARE(provider->requestImage(thumbs.source().path().mid(1), nullptr, {}), thumbs.image());
            QCOMPARE(app.prop("pause"), QString("no"));

            // Silent-track check: it follows the playing file too; both tones are audible.
            QTRY_VERIFY_WITH_TIMEOUT(app.services.silence.done(), 15000);
            QCOMPARE(QDir::fromNativeSeparators(app.services.silence.file()), m_clip);
            QVERIFY(app.services.silence.silent().isEmpty());

            // Volume (Down x3) and its memory.
            for (int i = 0; i < 3; ++i) app.key(Qt::Key_Down);
            QTRY_COMPARE(app.number("volume"), 85.0);
            QTRY_COMPARE(app.services.state.volume(), 85.0);

            // Pause, speed, seek.
            QVERIFY(app.mpv->keepingAwake()); // playing: the display does not sleep
            app.key(Qt::Key_Space);
            QTRY_COMPARE(app.prop("pause"), QString("yes"));
            QTRY_VERIFY(!app.mpv->keepingAwake());
            app.key(Qt::Key_Space);
            QTRY_COMPARE(app.prop("pause"), QString("no"));
            QTRY_VERIFY(app.mpv->keepingAwake());
            app.key(Qt::Key_G);
            QTRY_COMPARE(app.number("speed"), 2.0);
            app.key(Qt::Key_G);
            QTRY_COMPARE(app.number("speed"), 1.0);
            app.key(Qt::Key_D);
            QTRY_COMPARE(app.number("speed"), 1.1);
            app.key(Qt::Key_S);
            QTRY_COMPARE(app.number("speed"), 1.0);
            app.services.player.setSpeedFactor(1.5); // the settings UI will set these
            app.services.player.setSpeedStep(0.25);
            app.key(Qt::Key_G);
            QTRY_COMPARE(app.number("speed"), 1.5);
            app.key(Qt::Key_G);
            QTRY_COMPARE(app.number("speed"), 1.0);
            app.key(Qt::Key_D);
            QTRY_COMPARE(app.number("speed"), 1.25);
            app.key(Qt::Key_S);
            QTRY_COMPARE(app.number("speed"), 1.0);

            // End of video and screenshot folder: v1's defaults, then a change reaches mpv at once.
            QCOMPARE(app.prop("keep-open"), QString("always"));
            QCOMPARE(app.prop("loop-file"), QString("no"));
            QCOMPARE(app.prop("screenshot-dir"), QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
            app.services.player.setEndMode("loop");
            QTRY_COMPARE(app.prop("loop-file"), QString("inf"));
            QCOMPARE(app.prop("keep-open"), QString("no"));
            app.services.player.setScreenshotFolder("video_folder");
            QTRY_COMPARE(QDir::fromNativeSeparators(app.prop("screenshot-dir")), QFileInfo(m_clip).absolutePath());
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

            // Picture quality: a forced tier reaches mpv at once (the settings UI will set pref).
            QCOMPARE(app.prop("scale"), QString("lanczos")); // Auto starts at Normal
            app.services.quality.setPref("ultra");
            QTRY_COMPARE(app.prop("scale"), QString("ewa_lanczossharp"));
            app.services.quality.setPref("low");
            QTRY_COMPARE(app.prop("scale"), QString("bilinear"));
            QCOMPARE(app.prop("dither"), QString("no"));

            // Resume position: closing the window at 0:25 of the 60 s clip remembers it.
            app.mpv->command({"seek", 25, "absolute"});
            QTRY_VERIFY(app.number("time-pos") >= 25);
            app.window->close();
        } // closed: pending state is written now

        QCOMPARE(voInits(log), 1); // every mixer switch kept the video running (no VO re-init)
        QFile f(QDir(m_path).filePath("state.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonObject state = QJsonDocument::fromJson(f.readAll()).object();
        QCOMPARE(state.value("mixer").toObject().value("default_on").toString(), QString("2"));
        QCOMPARE(state.value("volume").toDouble(), 85.0);
        QCOMPARE(state.value("quality").toString(), QString("low"));
        QCOMPARE(state.value("playback").toObject().value("end").toString(), QString("loop"));
        QCOMPARE(state.value("screenshots").toObject().value("folder").toString(), QString("video_folder"));
        QFile resume(QDir(m_path).filePath("resume.json"));
        QVERIFY(resume.open(QIODevice::ReadOnly));
        const QJsonObject entry =
            QJsonDocument::fromJson(resume.readAll()).object().value("entries").toObject().value(QDir::cleanPath(QDir::fromNativeSeparators(m_clip))).toObject();
        QVERIFY2(entry.value("pos").toDouble() >= 25 && entry.value("pos").toDouble() < 30, "resume.json lacks 0:25");
    }

    void secondRunRestoresMixerAndVolume()
    {
        Launch app(m_path, m_clip, QDir(m_path).filePath("run2.log"));
        QVERIFY(app.mpv);
        QVERIFY2(app.playing(), "the clip did not start playing");
        QTRY_COMPARE(app.prop("aid"), QString("2"));
        QCOMPARE(app.prop("lavfi-complex"), QString());
        QCOMPARE(app.number("volume"), 85.0);
        QCOMPARE(app.prop("scale"), QString("bilinear")); // the forced tier from the first run
        QCOMPARE(app.prop("loop-file"), QString("inf"));   // end of video: loop, set before the file opened
        QTRY_COMPARE(QDir::fromNativeSeparators(app.prop("screenshot-dir")), QFileInfo(m_clip).absolutePath());
        QCOMPARE(app.services.player.speedFactor(), 1.5);

        // A file that is not a video: the end-file reason and mpv's words for it (the error state's detail).
        const QString broken = QDir(m_path).filePath("not-a-video.mp4");
        QFile junk(broken);
        QVERIFY(junk.open(QIODevice::WriteOnly));
        junk.write(QByteArray(4096, 'x'));
        junk.close();
        QSignalSpy ended(app.mpv, &MpvItem::fileEnded);
        app.mpv->command({"loadfile", broken, "replace"});
        QTRY_VERIFY(std::any_of(ended.cbegin(), ended.cend(), [](const QList<QVariant> &args) {
            return args.value(0).toString() == "error" && !args.value(1).toString().isEmpty();
        }));
        QVERIFY(app.services.resume.position(m_clip) >= 25); // ready for the "Resume from 0:25?" prompt
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
