#include <QtTest>
#include <QQuickItem>
#include <QQmlApplicationEngine>
#include <QQuickImageProvider>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QtQml/qqmlextensionplugin.h>
#include <clocale>
#include "MpvItem.h"
#include "Services.h"
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

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
    Launch(const QString &configDir, const QString &clip, const QString &log, const QString &version = "0.0.0-dev")
        : services(configDir, QCoreApplication::applicationDirPath(), version)
    {
        qApp->installEventFilter(&realInput);
        services.player.setFirstRunHintDone(true); // it keeps the chrome up: only notices() wants it
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
                                     "duration", "keep-open", "loop-file", "screenshot-dir", "vo-configured", "af"})
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
    // By the visual tree: a Repeater's items are not QObject children of the window.
    static QQuickItem *search(QQuickItem *from, const char *objectName)
    {
        if (from->objectName() == QLatin1String(objectName)) return from;
        const QList<QQuickItem *> children = from->childItems();
        for (QQuickItem *child : children)
            if (QQuickItem *found = search(child, objectName)) return found;
        return nullptr;
    }
    QQuickItem *item(const char *objectName) const { return search(window->contentItem(), objectName); }
    // The one a user can reach: rows of other settings tabs exist too, hidden.
    static QQuickItem *searchVisible(QQuickItem *from, const char *objectName)
    {
        if (!from->isVisible()) return nullptr;
        if (from->objectName() == QLatin1String(objectName)) return from;
        const QList<QQuickItem *> children = from->childItems();
        for (QQuickItem *child : children)
            if (QQuickItem *found = searchVisible(child, objectName)) return found;
        return nullptr;
    }
    // Clicks the middle of a named item, as a user does: the event goes to the window, not to the item.
    bool click(const char *objectName)
    {
        QTest::qWait(60); // what the last click changed is laid out by the next frame
        QQuickItem *target = searchVisible(window->contentItem(), objectName);
        if (!target) return false;
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          target->mapToScene(QPointF(target->width() / 2, target->height() / 2)).toPoint());
        return true;
    }
    // ECHOPLAY_E2E_SHOTS=<dir>: the window as a PNG per state, to compare by eye with the design boards.
    void shot(const QString &name)
    {
        const QString dir = qEnvironmentVariable("ECHOPLAY_E2E_SHOTS");
        if (dir.isEmpty()) return;
        QTest::qWait(350); // fades end
        QDir().mkpath(dir);
        window->grabWindow().save(QDir(dir).filePath(name + ".png"));
    }
#ifdef Q_OS_WIN
    // The screen around the window, not the window: only this shows the shadow and the rounded corners Windows adds.
    // On top for the moment of the grab, without taking the focus: otherwise this catches whatever else is open.
    void desktopShot(const QString &name)
    {
        const QString dir = qEnvironmentVariable("ECHOPLAY_E2E_SHOTS");
        if (dir.isEmpty()) return;
        const HWND hwnd = HWND(window->winId());
        const UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, flags);
        QTest::qWait(350);
        QDir().mkpath(dir);
        const QRect around = window->geometry().adjusted(-40, -40, 40, 40);
        window->screen()->grabWindow(0, around.x(), around.y(), around.width(), around.height()).save(QDir(dir).filePath(name + ".png"));
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, flags);
    }
#endif
    // The pointer moves over the window: the chrome comes up. It stays up for a second after that.
    void wake() { QTest::mouseMove(window, QPoint(window->width() / 2 + (m_wiggle ^= 1), window->height() / 2)); }
    int m_wiggle = 0;

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

            // The chrome: it comes up with the pointer and hides after a second without movement.
            QTRY_VERIFY(app.item("controlBar"));
            app.wake();
            QTRY_VERIFY(app.item("controlBar")->isVisible());
            app.shot("bar");
            QTRY_VERIFY_WITH_TIMEOUT(!app.item("controlBar")->isVisible(), 3000);
            app.shot("idle");

            // The mixer: the chip opens it, a switch in a row changes what mpv plays.
            app.wake();
            QTRY_VERIFY(app.item("mixerChip")->isVisible());
            QVERIFY(app.click("mixerChip"));
            QTRY_VERIFY(app.item("mixerPopover"));
            app.shot("mixer");
            const QString beforeRows = app.prop("aid"); // 2: the mic alone, from the keys above
            QCOMPARE(beforeRows, QString("2"));
            QQuickItem *row1 = app.item("mixerRow-1");
            QVERIFY(row1);
            QQuickItem *switch1 = Launch::search(row1, "trackSwitch");
            QVERIFY(switch1);
            QTest::mouseClick(app.window, Qt::LeftButton, Qt::NoModifier,
                              switch1->mapToScene(QPointF(switch1->width() / 2, switch1->height() / 2)).toPoint());
            QTRY_COMPARE(app.prop("lavfi-complex"), Mix);
            QVERIFY(app.click("muteAllButton")); // the chip says "No sound"
            QTRY_COMPARE(app.prop("aid"), QString("no"));
            QTRY_COMPARE(app.prop("lavfi-complex"), QString());
            app.shot("mixer-none");
            QVERIFY(app.click("allOnButton"));
            QTRY_COMPARE(app.prop("lavfi-complex"), Mix);
            app.key(Qt::Key_Escape); // the popover closes first
            QTRY_VERIFY(!app.item("mixerPopover"));
            QCOMPARE(app.window->visibility(), QWindow::Windowed);
            app.key(Qt::Key_1, Qt::ControlModifier); // game off again: the state the checks below expect
            QTRY_COMPARE(app.prop("aid"), QString("2"));

            // Volume (Down x3) and its memory, with the flash at the top.
            for (int i = 0; i < 3; ++i) app.key(Qt::Key_Down);
            QTRY_COMPARE(app.number("volume"), 85.0);
            QTRY_COMPARE(app.services.state.volume(), 85.0);
            QTRY_VERIFY(app.item("volumeFlash")->isVisible());
            app.shot("flash");
            app.mpv->setProperty("volume", 245);
            QTRY_COMPARE(app.number("volume"), 245.0);
            app.key(Qt::Key_Up);
            QTRY_COMPARE(app.number("volume"), 250.0);
            app.shot("flash-boost");
            app.wake();
            QTRY_VERIFY(app.item("controlBar")->isVisible());
            app.shot("bar-boost");
            app.mpv->setProperty("volume", 85);
            QTRY_COMPARE(app.number("volume"), 85.0);

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
            app.services.player.setSeekStep(30); // the arrow step reaches the keys at once
            const double beforeStep = app.number("time-pos");
            app.key(Qt::Key_Right);
            QTRY_VERIFY(app.number("time-pos") > beforeStep + 28);
            app.services.player.setSeekStep(10);
            app.mpv->command({"seek", 0, "absolute"});
            QTRY_VERIFY(app.number("time-pos") < 5);

            // Loudness boost: v1's levelling by default; off removes the filter while the video plays on.
            QCOMPARE(app.mpv->observed("af").toList().size(), 1);
            app.services.player.setLoudnessBoost(false);
            QTRY_COMPARE(app.mpv->observed("af").toList().size(), 0);
            app.services.player.setLoudnessBoost(true);
            QTRY_COMPARE(app.mpv->observed("af").toList().size(), 1);

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

    // No file on the command line: the start screen right away, and a dropped file plays.
    void startScreen()
    {
        const QString state = QDir(m_path).filePath("start");
        QVERIFY(QDir().mkpath(state));
        Launch app(state, QString(), QDir(m_path).filePath("start.log"));
        QVERIFY(app.mpv);
        QTRY_VERIFY(app.window->isVisible());
        QTRY_VERIFY(app.item("startScreen"));
        QVERIFY(app.item("startScreen")->isVisible());
        QVERIFY(app.item("openFileButton"));
        QVERIFY(app.item("openFolderButton"));
        QVERIFY(!app.item("controlBar"));
        app.shot("start");

        app.services.theme.setMode("light");
        app.shot("start-light");
        app.services.theme.setMode("dark");

#ifdef Q_OS_WIN
        // ECHOPLAY_E2E_DIALOGS=1: Windows' open dialogs really appear. Off by default, they take the focus from whoever
        // sits at the PC, and tools/e2e.sh runs after every change.
        if (qEnvironmentVariableIsSet("ECHOPLAY_E2E_DIALOGS")) {
            const auto openDialog = [] {
                HWND found = nullptr;
                EnumWindows([](HWND w, LPARAM out) {
                    DWORD pid = 0;
                    GetWindowThreadProcessId(w, &pid);
                    wchar_t name[16] = {};
                    GetClassNameW(w, name, 16);
                    if (pid != GetCurrentProcessId() || !IsWindowVisible(w) || wcscmp(name, L"#32770") != 0) return TRUE;
                    *reinterpret_cast<HWND *>(out) = w;
                    return FALSE;
                }, LPARAM(&found));
                return found;
            };
            for (const char *pick : {"pickFile", "pickFolder"}) {
                QVERIFY(QMetaObject::invokeMethod(app.window, pick));
                QTRY_VERIFY2(openDialog(), pick);
                PostMessageW(openDialog(), WM_CLOSE, 0, 0);
                QTRY_VERIFY(!openDialog());
            }
        }
#endif

        // With nothing open, keys do nothing: a pause here would still hold when the file opens.
        app.key(Qt::Key_Space);
        QTest::qWait(100);
        QVERIFY(!app.mpv->observed("pause").toBool());

        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(m_clip)});
        const QPoint center(app.window->width() / 2, app.window->height() / 2);
        QDragEnterEvent enter(center, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(app.window, &enter);
        QTRY_VERIFY(app.item("dropOverlay"));
        app.shot("drop");
        QDropEvent dropped(QPointF(center), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(app.window, &dropped);
        QVERIFY2(app.playing(), "the dropped clip did not start playing");
        QTRY_VERIFY(!app.item("dropOverlay"));
        QTRY_VERIFY(!app.item("startScreen"));
        QTRY_VERIFY(app.item("controlBar"));
        QTRY_COMPARE(app.window->title(), QFileInfo(m_clip).fileName());

        // A file dropped over a playing video replaces it.
        const QString second = QFileInfo(m_clip).absolutePath() + "/edge/video-only.mp4";
        QVERIFY(QFileInfo::exists(second));
        mime.setUrls({QUrl::fromLocalFile(second)});
        QDragEnterEvent enterAgain(center, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(app.window, &enterAgain);
        QTRY_VERIFY(app.item("dropOverlay"));
        QDropEvent droppedAgain(QPointF(center), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(app.window, &droppedAgain);
        QTRY_COMPARE(app.window->title(), QString("video-only.mp4"));
    }

    // The gear's popover and Settings, clicked through as a user does.
    void popoversAndSettings()
    {
        const QString state = QDir(m_path).filePath("panels");
        QVERIFY(QDir().mkpath(state));
        Launch app(state, m_clip, QDir(m_path).filePath("panels.log"));
        QVERIFY(app.mpv);
        QVERIFY2(app.playing(), "the clip did not start playing");
        QTRY_VERIFY(app.item("controlBar"));
        app.wake();
        QTRY_VERIFY(app.item("gearButton") && app.item("gearButton")->isVisible());

        QVERIFY(app.click("gearButton"));
        QTRY_VERIFY(app.item("quickPopover"));
        app.shot("quick");
        QVERIFY(app.click("speedMore"));
        QTRY_COMPARE(app.number("speed"), 1.1);
        QVERIFY(app.click("speedLess"));
        QTRY_COMPARE(app.number("speed"), 1.0);
        QVERIFY(app.click("segment-low"));
        QTRY_COMPARE(app.prop("scale"), QString("bilinear"));
        QVERIFY(app.click("segment-auto"));
        QTRY_COMPARE(app.services.quality.pref(), QString("auto"));
        QVERIFY(app.click("segment-loop"));
        QTRY_COMPARE(app.prop("loop-file"), QString("inf"));
        QVERIFY(app.click("segment-stop"));
        QTRY_COMPARE(app.prop("loop-file"), QString("no"));
        QVERIFY(app.click("quickSubtitles")); // the clip has none: nothing opens
        QTest::qWait(100);
        QVERIFY(app.item("quickPopover"));
        QVERIFY(!app.item("subtitlesPopover"));

        QVERIFY(app.click("allSettings"));
        QTRY_VERIFY(app.item("settingsOverlay"));
        QVERIFY(!app.item("quickPopover"));
        QTRY_VERIFY(!app.item("controlBar")->isVisible());
        app.shot("settings-audio");
        QVERIFY(app.click("loudnessSwitch"));
        QTRY_COMPARE(app.mpv->observed("af").toList().size(), 0);
        QVERIFY(app.click("loudnessSwitch"));
        QTRY_COMPARE(app.mpv->observed("af").toList().size(), 1);

        for (const char *tab : {"playback", "picture", "appearance", "language", "screenshots", "shortcuts", "about"}) {
            QVERIFY2(app.click(QByteArray("settingsTab-") + tab), tab);
            app.shot(QString("settings-") + tab);
        }

        // The arrow step from Playback reaches the keys; a dropdown's list; the language changes everything.
        QVERIFY(app.click("settingsTab-playback"));
        QVERIFY(app.click("segment-30"));
        QTRY_COMPARE(app.services.player.seekStep(), 30);
        QVERIFY(app.click("segment-10"));
        QVERIFY(app.click("settingsTab-language"));
        QVERIFY(app.click("languageList"));
        QTRY_VERIFY(app.item("option-tr"));
        app.shot("settings-list");
        QVERIFY(app.click("option-tr"));
        QTRY_COMPARE(app.services.i18n.language(), QString("tr"));
        QTest::qWait(50);
        app.shot("settings-tr");
        QVERIFY(app.click("languageList"));
        QTRY_VERIFY(app.item("option-en"));
        app.key(Qt::Key_Escape); // the list closes first, settings stay
        QTRY_VERIFY(!app.item("option-en"));
        QVERIFY(app.item("settingsOverlay"));
        QVERIFY(app.click("languageList"));
        QTRY_VERIFY(app.item("option-en"));
        QVERIFY(app.click("option-en"));
        QTRY_COMPARE(app.services.i18n.language(), QString("en"));

        // Search finds rows on any tab. Typing fires no shortcut (S would slow the video).
        QVERIFY(app.click("settingsSearch"));
        for (const char c : QByteArray("strength")) QTest::keyClick(app.window, c);
        QTest::qWait(50);
        app.shot("settings-search");
        QCOMPARE(app.number("speed"), 1.0);
        QTRY_COMPARE(app.item("settingsSearch")->property("text").toString(), QString("strength"));
        app.key(Qt::Key_A, Qt::ControlModifier);
        app.key(Qt::Key_Backspace);
        QVERIFY(app.click("settingsTab-appearance")); // takes the focus off the field below

        // Any color, then back to the theme's own.
        QVERIFY(app.click("anyColor"));
        QTRY_VERIFY(app.item("colorPicker"));
        app.shot("settings-color");
        app.key(Qt::Key_Escape);
        QTRY_VERIFY(!app.item("colorPicker")->isVisible());
        QVERIFY(app.click("swatch-#3E7BFA"));
        QTRY_COMPARE(app.services.theme.accent(), QColor("#3E7BFA"));
        QVERIFY(app.click("swatch-"));
        QTRY_COMPARE(app.services.theme.accentChoice(), QString());

        // A new key for an action; a key another action uses is refused; reset.
        QVERIFY(app.click("settingsTab-shortcuts"));
        QVERIFY(app.click("key-mute"));
        QTest::qWait(50);
        app.shot("settings-key");
        app.key(Qt::Key_K);
        QTRY_COMPARE(app.services.player.shortcuts().value("mute").toString(), QString("K"));
        QVERIFY(app.click("key-mute"));
        app.key(Qt::Key_Space); // the pause key: refused, and it pauses nothing
        QTest::qWait(100);
        QCOMPARE(app.services.player.shortcuts().value("mute").toString(), QString("K"));
        QCOMPARE(app.prop("pause"), QString("no"));
        app.key(Qt::Key_K); // works at once
        QTRY_VERIFY(app.mpv->observed("mute").toBool() == false);
        QVERIFY(app.click("resetShortcuts"));
        QTRY_VERIFY(app.services.player.shortcuts().isEmpty());

        // Light mode, and the panel in the smallest window.
        app.services.theme.setMode("light");
        QVERIFY(app.click("settingsTab-appearance"));
        app.shot("settings-light");
        app.services.theme.setMode("dark");
        app.window->resize(app.window->minimumSize());
        QTRY_COMPARE(app.window->width(), app.window->minimumWidth());
        QVERIFY(app.click("settingsTab-audio"));
        app.shot("settings-narrow");

        app.key(Qt::Key_Escape);
        QTRY_VERIFY(!app.item("settingsOverlay"));
    }

    // What the player tells on its own: the first-run hint, no sound, resume per setting, a new version, a bad file.
    void notices()
    {
        const QString state = QDir(m_path).filePath("notices");
        QVERIFY(QDir().mkpath(state));
        {
            Launch app(state, m_clip, QDir(m_path).filePath("notices1.log"));
            app.services.player.setFirstRunHintDone(false);
            QVERIFY(app.mpv);
            QVERIFY2(app.playing(), "the clip did not start playing");
            QTRY_VERIFY(app.item("hintCard"));
            QTest::qWait(1300); // it holds the chrome up while it waits
            QVERIFY(app.item("controlBar")->isVisible());
            app.shot("hint");
            QVERIFY(app.click("hintGotIt"));
            QTRY_VERIFY(!app.item("hintCard"));
            QVERIFY(app.services.player.firstRunHintDone());

            app.wake();
            app.key(Qt::Key_1, Qt::ControlModifier); // the only track on goes off
            QTRY_VERIFY(app.item("noSoundCard"));
            app.shot("no-sound");
            QVERIFY(app.click("turnAllOn"));
            QTRY_VERIFY(!app.item("noSoundCard"));
            QTRY_COMPARE(app.prop("lavfi-complex"), Mix);

            // A new version: the card, Later hides it for this run.
            app.mpv->command({"seek", 25, "absolute"});
            QTRY_VERIFY(app.number("time-pos") >= 25);
            app.window->close();
        }
        {
            Launch app(state, m_clip, QDir(m_path).filePath("notices2.log"), "2.0.0");
            QVERIFY(app.mpv);
            QVERIFY2(app.playing(), "the clip did not start playing");
            QVERIFY(!app.item("hintCard")); // once, never again
            QTRY_VERIFY(app.item("resumeCard"));
            app.wake();
            app.shot("resume");
            QVERIFY(app.click("resumeButton"));
            QTRY_VERIFY(app.number("time-pos") >= 25);
            QTRY_VERIFY(!app.item("resumeCard"));

            app.services.updates.handleReply(R"({"tag_name": "v2.1.0", "assets": []})");
            QTRY_VERIFY(app.item("updateNotice"));
            app.wake();
            app.shot("update");
            QVERIFY(app.click("updateLater"));
            QTRY_VERIFY(!app.item("updateNotice"));

            app.services.player.setResumeMode("always");
            app.mpv->command({"seek", 30, "absolute"});
            QTRY_VERIFY(app.number("time-pos") >= 30);
            app.window->close();
        }
        {
            Launch app(state, m_clip, QDir(m_path).filePath("notices3.log"));
            QVERIFY(app.mpv);
            QVERIFY2(app.playing(), "the clip did not start playing");
            QTRY_VERIFY(app.number("time-pos") >= 30); // "always": no question
            QVERIFY(!app.item("resumeCard"));

            // A bad file: the error screen, its way out and the report. The playlist stays for the next clip.
            const QString badDir = QDir(m_path).filePath("bad");
            QVERIFY(QDir().mkpath(badDir));
            QFile junk(badDir + "/a-broken.mp4");
            QVERIFY(junk.open(QIODevice::WriteOnly));
            junk.write(QByteArray(4096, 'x'));
            junk.close();
            QFile::remove(badDir + "/b-good.mp4");
            QVERIFY(QFile::copy(m_clip, badDir + "/b-good.mp4"));
            QVERIFY(QMetaObject::invokeMethod(app.window, "openPath", Q_ARG(QVariant, badDir + "/a-broken.mp4")));
            QTRY_VERIFY(app.item("errorScreen"));
            QTest::qWait(300);
            QVERIFY(app.item("errorScreen")); // mpv did not go on to the next clip by itself
            app.shot("error");
            QVERIFY(Launch::searchVisible(app.window->contentItem(), "errorNext")); // b-good.mp4 comes after it
            // The button would overwrite the clipboard of whoever sits at the PC: the report is read from the screen.
            QVERIFY(QMetaObject::invokeMethod(app.item("errorScreen"), "details"));
            QTest::qWait(60);
            app.shot("error-details");
            const QString text = app.item("errorText")->property("text").toString();
            QVERIFY2(text.contains("Cause, from the mpv engine: unrecognized file format."), qPrintable(text));
            QVERIFY2(text.contains("a-broken.mp4"), qPrintable(text));
            QVERIFY2(text.contains("Last log lines:"), qPrintable(text));
            QVERIFY2(!text.contains(QDir::home().dirName(), Qt::CaseInsensitive), qPrintable(text));

            // Next clip works from the plain view.
            QVERIFY(QMetaObject::invokeMethod(app.window, "playNextClip"));
            QTRY_VERIFY(!app.item("errorScreen"));
            QTRY_COMPARE(app.window->title(), QString("b-good.mp4"));
        }
    }

    // Looks with no behavior of their own, only for ECHOPLAY_E2E_SHOTS: compare them by eye with the design boards.
    void looks()
    {
        if (!qEnvironmentVariableIsSet("ECHOPLAY_E2E_SHOTS")) QSKIP("no screenshots asked for");
        const QString state = QDir(m_path).filePath("looks");
        QVERIFY(QDir().mkpath(state));
        Launch app(state, m_clip, QDir(m_path).filePath("looks.log"));
        QVERIFY(app.mpv);
        QVERIFY2(app.playing(), "the clip did not start playing");
        QTRY_VERIFY(app.item("controlBar"));

        // The arrow sweep lasts 360 ms: grabbed in the middle, not after the usual wait.
        app.key(Qt::Key_Right);
        QTest::qWait(150);
        app.window->grabWindow().save(QDir(qEnvironmentVariable("ECHOPLAY_E2E_SHOTS")).filePath("sweep.png"));

        // The preview over the seek bar: the pointer rests on it.
        app.wake();
        QTRY_VERIFY(app.item("controlBar")->isVisible());
        QQuickItem *bar = app.item("controlBar");
        const QPoint onSeekBar = bar->mapToScene(QPointF(bar->width() * 0.6, 19)).toPoint();
        QTest::mouseMove(app.window, onSeekBar - QPoint(3, 0));
        QTest::mouseMove(app.window, onSeekBar);
        QTest::qWait(1200);
        app.shot("seek-preview");

        app.services.theme.setBarStyle("full");
        app.wake();
        app.shot("bar-full");
        app.services.theme.setBarStyle("island");

        app.key(Qt::Key_F);
        QTRY_COMPARE(app.window->visibility(), QWindow::FullScreen);
        app.wake();
        app.shot("fullscreen");
        app.key(Qt::Key_Escape);
        QTRY_COMPARE(app.window->visibility(), QWindow::Windowed);

        app.window->resize(760, 420); // below 820: the volume and the chip's text collapse
        QTRY_COMPARE(app.window->width(), 760);
        app.wake();
        app.shot("narrow");
    }

#ifdef Q_OS_WIN
    // The frameless window as Windows sees it. Messages go to this window only, so no real input is needed.
    void windowFrame()
    {
        const QString state = QDir(m_path).filePath("frame"); // its own: closing must not change what the second run reads
        QVERIFY(QDir().mkpath(state));
        Launch app(state, m_clip, QDir(m_path).filePath("frame.log"));
        QVERIFY(app.mpv);
        QVERIFY2(app.playing(), "the clip did not start playing");
        const HWND hwnd = HWND(app.window->winId());
        const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        QVERIFY(style & WS_THICKFRAME);
        QVERIFY(style & WS_CAPTION);
        QVERIFY(style & WS_MAXIMIZEBOX);

        // The picture on the screen, in physical pixels: maximized, part of the window itself is off the screen.
        const auto pictureOnScreen = [&] {
            RECT w;
            GetWindowRect(hwnd, &w);
            const qreal ratio = app.window->devicePixelRatio();
            const QPointF at = app.mpv->mapToScene(QPointF(0, 0)) * ratio;
            return QRect(w.left + qRound(at.x()), w.top + qRound(at.y()), qRound(app.mpv->width() * ratio), qRound(app.mpv->height() * ratio));
        };
        const auto workArea = [hwnd] {
            MONITORINFO info{};
            info.cbSize = sizeof(info);
            GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &info);
            return QRect(info.rcWork.left, info.rcWork.top, info.rcWork.right - info.rcWork.left, info.rcWork.bottom - info.rcWork.top);
        };
        // Hit test in physical pixels, relative to the client area's top left.
        const auto hit = [&](int x, int y) {
            const QPoint p = pictureOnScreen().topLeft() + QPoint(x, y);
            return int(SendMessageW(hwnd, WM_NCHITTEST, 0, MAKELPARAM(p.x(), p.y())));
        };

        const auto checkHits = [&](bool edges) {
            const qreal ratio = app.window->devicePixelRatio();
            const QRect c = pictureOnScreen();
            const int w = c.width(), h = c.height();
            const int mid = h / 2, caption = qRound(20 * ratio);
            const int button = qRound(46 * ratio);
            QCOMPARE(hit(w / 2, caption), int(HTCAPTION));
            QCOMPARE(hit(w / 2, mid), int(HTCLIENT));
            QCOMPARE(hit(w - button / 2, caption), int(HTCLIENT));         // close
            QCOMPARE(hit(w - button - button / 2, caption), int(HTMAXBUTTON)); // snap layouts
            QCOMPARE(hit(w - 2 * button - button / 2, caption), int(HTCLIENT)); // minimize
            QCOMPARE(hit(1, 1), edges ? int(HTTOPLEFT) : int(HTCAPTION));
            QCOMPARE(hit(w - 2, h - 2), edges ? int(HTBOTTOMRIGHT) : int(HTCLIENT));
            QCOMPARE(hit(1, mid), edges ? int(HTLEFT) : int(HTCLIENT));
            QCOMPARE(hit(w - 2, mid), edges ? int(HTRIGHT) : int(HTCLIENT));
            QCOMPARE(hit(w / 2, h - 2), edges ? int(HTBOTTOM) : int(HTCLIENT));
            QCOMPARE(hit(w / 2, 1), edges ? int(HTTOP) : int(HTCAPTION));
        };
        checkHits(true);
        if (QTest::currentTestFailed()) return;
        const QRect normal = pictureOnScreen();
        QVERIFY(normal.width() > 0);
        app.desktopShot("frame-normal");

        // Maximized is always Windows' own (caption double-click, Aero Snap, the button): exactly the work area on the
        // screen, the taskbar free, nothing cut. Qt's imitation for frameless windows (showMaximized) is not used.
        const auto checkMaximized = [&](const char *how) {
            QTRY_COMPARE(app.window->visibility(), QWindow::Maximized);
            QTRY_VERIFY(IsZoomed(hwnd));
            QTRY_COMPARE(pictureOnScreen(), workArea());
            qInfo("%s: picture %d,%d %dx%d", how, pictureOnScreen().x(), pictureOnScreen().y(), pictureOnScreen().width(), pictureOnScreen().height());
            checkHits(false);
        };
        const auto checkNormal = [&] {
            QTRY_COMPARE(app.window->visibility(), QWindow::Windowed);
            QTRY_COMPARE(pictureOnScreen(), normal);
            checkHits(true);
        };
        SendMessageW(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
        checkMaximized("SC_MAXIMIZE");
        if (QTest::currentTestFailed()) return;
        app.desktopShot("frame-maximized");
        SendMessageW(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
        checkNormal();
        if (QTest::currentTestFailed()) return;

        // The maximize button, as Windows reports a click on it: posted, since Qt runs its filters on input messages
        // only from the queue, and DefWindowProc would wait for a real mouse release.
        const auto clickMaximize = [hwnd] {
            PostMessageW(hwnd, WM_NCLBUTTONDOWN, HTMAXBUTTON, 0);
            PostMessageW(hwnd, WM_NCLBUTTONUP, HTMAXBUTTON, 0);
        };
        clickMaximize();
        checkMaximized("maximize button");
        if (QTest::currentTestFailed()) return;

        // Minimized from maximized (the title bar's button), and back as the taskbar does it.
        app.wake();
        QTRY_VERIFY(app.item("minimizeButton") && app.item("minimizeButton")->isVisible());
        QVERIFY(app.click("minimizeButton"));
        QTRY_COMPARE(app.window->visibility(), QWindow::Minimized);
        SendMessageW(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
        checkMaximized("back from minimized");
        if (QTest::currentTestFailed()) return;

        // Full screen from maximized returns to maximized, and the normal size is still there after that.
        app.key(Qt::Key_F);
        QTRY_COMPARE(app.window->visibility(), QWindow::FullScreen);
        app.key(Qt::Key_Escape);
        checkMaximized("back from full screen");
        if (QTest::currentTestFailed()) return;
        clickMaximize();
        checkNormal();
        if (QTest::currentTestFailed()) return;

        // Full screen from a normal window covers the whole monitor, has no edges or caption, and returns.
        app.key(Qt::Key_F);
        QTRY_COMPARE(app.window->visibility(), QWindow::FullScreen);
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &info);
        const QRect monitor(info.rcMonitor.left, info.rcMonitor.top, info.rcMonitor.right - info.rcMonitor.left, info.rcMonitor.bottom - info.rcMonitor.top);
        QTRY_COMPARE(pictureOnScreen(), monitor);
        QCOMPARE(hit(1, 1), int(HTCLIENT));
        QCOMPARE(hit(monitor.width() / 2, 10), int(HTCLIENT));
        app.key(Qt::Key_Escape);
        checkNormal();
        if (QTest::currentTestFailed()) return;
        QVERIFY(GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_THICKFRAME); // the frame came back with it

        // Minimum size: Windows asks before it resizes.
        MINMAXINFO limits{};
        SendMessageW(hwnd, WM_GETMINMAXINFO, 0, LPARAM(&limits));
        const qreal ratio = app.window->devicePixelRatio();
        QVERIFY2(limits.ptMinTrackSize.x >= qRound(640 * ratio) && limits.ptMinTrackSize.y >= qRound(360 * ratio), "minimum size is not enforced");
        app.window->close();
    }
#endif

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

        // A lower maximum volume pulls a louder volume down with it, and the saved volume follows.
        app.services.player.setVolumeMax(120);
        QCOMPARE(app.number("volume"), 85.0);
        app.mpv->setProperty("volume", 110);
        QTRY_COMPARE(app.number("volume"), 110.0);
        app.services.player.setVolumeMax(100);
        QTRY_COMPARE(app.number("volume"), 100.0);
        QTRY_COMPARE(app.services.state.volume(), 100.0);

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
