#include <QtTest>
#include <QCryptographicHash>
#include <QQuickItem>
#include <QQmlApplicationEngine>
#include <QPalette>
#include <QProcess>
#include <QQuickImageProvider>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtQml/qqmlextensionplugin.h>
#include <atomic>
#include <clocale>
#include <cmath>
#include <functional>
#include <thread>
#include "MpvItem.h"
#include "Services.h"
#include "WindowChrome.h"
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <shobjidl.h>
#include <uiautomation.h>
#include <wrl/client.h>
#endif

Q_IMPORT_QML_PLUGIN(EchoPlayPlugin)

// End-to-end: the real player (Main.qml, MpvItem, the singletons) on a real two-track clip, driven the way a
// user drives it, with key presses and clicks; checks what mpv does and what is saved. It opens a window, so
// tools/e2e.sh runs it, never tools/check.sh. Synthetic input goes to the test window only, not to the OS.
// The clip comes from ECHOPLAY_E2E_CLIP (tools/e2e.sh generates one: tracks "System sounds", "Microphone").

namespace {

const QString Mix = "[aid1]volume=1.00[ev1];[aid2]volume=1.00[ev2];[ev1][ev2]amix=inputs=2:normalize=0[ao]";

// A release with a setup. The asset does not exist, so a download of it comes to nothing.
// A plain string: moc reads a raw one as ordinary quotes, and the // of a URL then hides the rest of the file.
const char NextRelease[] = "{\"tag_name\": \"v99.0.0\", \"body\": \"Notes of the next version\", \"assets\": [{\"name\": \"EchoPlay-Setup.exe\","
    " \"digest\": \"sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\","
    " \"browser_download_url\": \"https://github.com/huseyincancalti/EchoPlay/releases/download/v99.0.0/EchoPlay-Setup.exe\"}]}";

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
        return QTest::qWaitFor([this] { return prop("vo-configured") == "yes" && prop("pause") == "no" && number("time-pos") > 0.3 && uiLoaded(); }, 10000);
    }
    // The player's UI loads asynchronously: its items are in the tree before the Loader hands it over, and a key that
    // reaches it through the Loader (L, I, PgUp) does nothing until then. Seen: L lost after "controlBar" was found.
    bool uiLoaded() const
    {
        QQuickItem *loader = item("playerUiLoader");
        return loader && loader->property("item").value<QObject *>();
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
    static QList<QQuickItem *> searchAllVisible(QQuickItem *from, const char *objectName)
    {
        QList<QQuickItem *> found;
        if (!from->isVisible()) return found;
        if (from->objectName() == QLatin1String(objectName)) found << from;
        const QList<QQuickItem *> children = from->childItems();
        for (QQuickItem *child : children) found << searchAllVisible(child, objectName);
        return found;
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
    // onTop false leaves Windows' own flyouts over the window in the picture (TOPMOST would hide them).
    void desktopShot(const QString &name, bool onTop = true)
    {
        const QString dir = qEnvironmentVariable("ECHOPLAY_E2E_SHOTS");
        if (dir.isEmpty()) return;
        const HWND hwnd = HWND(window->winId());
        const UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
        if (onTop) SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, flags);
        if (onTop) QTest::qWait(350);
        QDir().mkpath(dir);
        const QRect around = window->geometry().adjusted(-40, -40, 40, 40);
        window->screen()->grabWindow(0, around.x(), around.y(), around.width(), around.height()).save(QDir(dir).filePath(name + ".png"));
        if (onTop) SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, flags);
    }
    // A screen rectangle in physical pixels (what Win32 and UI Automation give), for what Windows draws outside the window.
    void screenShot(const QString &name, const QRect &physical)
    {
        const QString dir = qEnvironmentVariable("ECHOPLAY_E2E_SHOTS");
        if (dir.isEmpty() || physical.isEmpty()) return;
        QDir().mkpath(dir);
        const qreal ratio = window->devicePixelRatio();
        const QRect r(qFloor(physical.x() / ratio), qFloor(physical.y() / ratio), qCeil(physical.width() / ratio), qCeil(physical.height() / ratio));
        window->screen()->grabWindow(0, r.x(), r.y(), r.width(), r.height()).save(QDir(dir).filePath(name + ".png"));
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

#ifdef Q_OS_WIN
// Takes back out of the Recycle Bin, for good, what a test put there from inside folder, and counts it. Each file in
// the bin is a pair: $I<id> (from byte 28 on, its original path in UTF-16) and $R<id> (the file itself).
int removeFromTrash(const QString &folder)
{
    const QString prefix = QDir::toNativeSeparators(QDir(folder).absolutePath()) + '\\';
    const QDir bin(QStorageInfo(folder).rootPath() + "$Recycle.Bin");
    const QDir::Filters hidden = QDir::Hidden | QDir::System;
    int removed = 0;
    for (const QFileInfo &user : bin.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | hidden)) {
        const QDir dir(user.filePath());
        for (const QFileInfo &info : dir.entryInfoList({"$I*"}, QDir::Files | hidden)) {
            QFile f(info.filePath());
            if (!f.open(QIODevice::ReadOnly)) continue;
            const QByteArray data = f.readAll() + QByteArray(2, '\0');
            f.close();
            if (data.size() < 30) continue;
            const QString original = QString::fromUtf16(reinterpret_cast<const char16_t *>(data.constData() + 28));
            if (!original.startsWith(prefix, Qt::CaseInsensitive)) continue;
            QFile::remove(dir.filePath("$R" + info.fileName().mid(2)));
            QFile::remove(info.filePath());
            ++removed;
        }
    }
    return removed;
}

// Visible top-level windows of other processes over area (screen pixels), with their class names. Cloaked ones are
// left out: Windows keeps some of its flyouts "visible" and only cloaks them while they are away.
QHash<quintptr, QString> shownWindows(const QRect &area)
{
    struct Probe {
        RECT area;
        QHash<quintptr, QString> found;
    } probe{{area.left(), area.top(), area.right() + 1, area.bottom() + 1}, {}};
    EnumWindows([](HWND w, LPARAM data) -> BOOL {
        auto &probe = *reinterpret_cast<Probe *>(data);
        DWORD pid = 0, cloaked = 0;
        GetWindowThreadProcessId(w, &pid);
        DwmGetWindowAttribute(w, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
        RECT r, overlap;
        if (pid != GetCurrentProcessId() && IsWindowVisible(w) && !cloaked && GetWindowRect(w, &r) && IntersectRect(&overlap, &r, &probe.area)) {
            wchar_t name[128] = {};
            GetClassNameW(w, name, 127);
            probe.found.insert(quintptr(w), QString::fromWCharArray(name));
        }
        return TRUE;
    }, LPARAM(&probe));
    return probe.found;
}

// UI Automation, for what only Windows draws and nothing else locates: the taskbar's buttons, the thumbnail's buttons.
// The first button whose name passes match, in the top-level windows of pid (of those classes, when given), and the
// window it is in, in screen pixels; seen collects every name looked at. Call it off the GUI thread: a query that
// reaches the test window waits for that thread.
struct FoundButton {
    QRect button, window;
};
FoundButton findButton(DWORD pid, const QStringList &classes, const std::function<bool(const QString &)> &match, QStringList *seen = nullptr)
{
    using Microsoft::WRL::ComPtr;
    const auto qrect = [](const RECT &r) { return QRect(r.left, r.top, r.right - r.left, r.bottom - r.top); };
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const auto uninit = qScopeGuard([] { CoUninitialize(); });
    ComPtr<IUIAutomation> uia;
    ComPtr<IUIAutomationElement> root;
    ComPtr<IUIAutomationCondition> ofPid, isButton;
    ComPtr<IUIAutomationElementArray> windows;
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_I4;
    if (FAILED(CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IUIAutomation), reinterpret_cast<void **>(uia.GetAddressOf())))) return {};
    v.lVal = LONG(pid);
    uia->CreatePropertyCondition(UIA_ProcessIdPropertyId, v, ofPid.GetAddressOf());
    v.lVal = UIA_ButtonControlTypeId;
    uia->CreatePropertyCondition(UIA_ControlTypePropertyId, v, isButton.GetAddressOf());
    if (FAILED(uia->GetRootElement(root.GetAddressOf())) || !ofPid || !isButton || FAILED(root->FindAll(TreeScope_Children, ofPid.Get(), windows.GetAddressOf())) || !windows) return {};
    int count = 0;
    windows->get_Length(&count);
    for (int i = 0; i < count; ++i) {
        ComPtr<IUIAutomationElement> window;
        ComPtr<IUIAutomationElementArray> buttons;
        BSTR cls = nullptr;
        RECT windowRect{};
        if (FAILED(windows->GetElement(i, window.GetAddressOf())) || FAILED(window->get_CurrentClassName(&cls))) continue;
        const QString className = QString::fromWCharArray(cls ? cls : L"");
        SysFreeString(cls);
        if (!classes.isEmpty() && !classes.contains(className)) continue;
        window->get_CurrentBoundingRectangle(&windowRect);
        if (FAILED(window->FindAll(TreeScope_Descendants, isButton.Get(), buttons.GetAddressOf())) || !buttons) continue;
        int n = 0;
        buttons->get_Length(&n);
        for (int j = 0; j < n; ++j) {
            ComPtr<IUIAutomationElement> button;
            BSTR name = nullptr;
            RECT r{};
            if (FAILED(buttons->GetElement(j, button.GetAddressOf())) || FAILED(button->get_CurrentName(&name))) continue;
            const QString text = QString::fromWCharArray(name ? name : L"");
            SysFreeString(name);
            if (seen) *seen << text;
            if (match(text) && SUCCEEDED(button->get_CurrentBoundingRectangle(&r)) && r.right > r.left) return {qrect(r), qrect(windowRect)};
        }
    }
    return {};
}
#endif

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
            QTRY_VERIFY_WITH_TIMEOUT(!app.item("controlBar")->isVisible(), 8000); // a real pointer over the window wakes it too
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
            // A slider held down follows the pointer anywhere: dragged far past its end, off the popover, it is full.
            {
                QQuickItem *row = app.item("mixerRow-1"); // the rows are new after "all on"
                QVERIFY(row);
                QQuickItem *gain = Launch::search(row, "trackGain");
                QVERIFY(gain);
                const QPoint from = gain->mapToScene(QPointF(gain->width() / 2, gain->height() / 2)).toPoint();
                QTest::mousePress(app.window, Qt::LeftButton, Qt::NoModifier, from);
                for (int step = 1; step <= 6; ++step)
                    QTest::mouseMove(app.window, from + QPoint(step * 120, -step * 40));
                QTest::mouseRelease(app.window, Qt::LeftButton, Qt::NoModifier, from + QPoint(720, -240));
                QTRY_COMPARE(app.item("mixerRow-1")->property("shownGain").toDouble(), app.services.mixer.gainMax());
                app.services.mixer.setGain(1, 1);
                QTRY_COMPARE(app.item("mixerRow-1")->property("shownGain").toDouble(), 1.0);
            }
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
            const auto toast = [&] { return app.item("toastText")->property("text").toString(); };
            const auto text = [&](const char *key) { return app.services.i18n.strings().value(key).toString(); };
            QVERIFY(app.mpv->keepingAwake()); // playing: the display does not sleep
            app.key(Qt::Key_Space);
            QTRY_COMPARE(app.prop("pause"), QString("yes"));
            QTRY_VERIFY(!app.mpv->keepingAwake());
            app.key(Qt::Key_Space);
            QTRY_COMPARE(app.prop("pause"), QString("no"));
            QTRY_VERIFY(app.mpv->keepingAwake());
            app.key(Qt::Key_G);
            QTRY_COMPARE(app.number("speed"), 2.0);
            QTRY_COMPARE(toast(), text("speed_now").arg("2")); // with the bar down, nothing else says it
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
            QCOMPARE(QDir::fromNativeSeparators(app.prop("screenshot-dir")), QDir::cleanPath(m_path + "/Screenshots"));
            app.services.player.setEndMode("loop");
            QTRY_COMPARE(app.prop("loop-file"), QString("inf"));
            QCOMPARE(app.prop("keep-open"), QString("no"));
            // mpv makes the folder at the first screenshot: the default place in AppData needs no setup.
            const QString shots = QDir(m_path).filePath("shots-new");
            QVERIFY(!QDir(shots).exists());
            app.services.player.setScreenshotFolder("custom");
            app.services.player.setScreenshotCustomPath(shots);
            QTRY_COMPARE(QDir::fromNativeSeparators(app.prop("screenshot-dir")), QDir::cleanPath(shots));
            app.mpv->command({"screenshot", "video"});
            QTRY_VERIFY_WITH_TIMEOUT(!QDir(shots).entryList(QDir::Files).isEmpty(), 5000);
            app.key(Qt::Key_F1); // the key says it worked
            QTRY_COMPARE(toast(), text("shot_saved"));
            QTRY_COMPARE(QDir(shots).entryList(QDir::Files).size(), 2);
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

            // One frame at a time, and it stops there. A second at a time with Shift. 0-9 jump to that tenth of the clip.
            app.mpv->command({"seek", 10, "absolute+exact"});
            QTRY_VERIFY(std::abs(app.number("time-pos") - 10) < 0.5);
            app.key(Qt::Key_Period);
            QTRY_COMPARE(app.prop("pause"), QString("yes"));
            QTest::qWait(200); // the step has landed
            const double frame = app.number("time-pos");
            app.key(Qt::Key_Period);
            QTRY_VERIFY(app.number("time-pos") > frame);
            QVERIFY2(app.number("time-pos") - frame < 0.05, "more than one frame at 60 fps");
            const double nextFrame = app.number("time-pos");
            app.key(Qt::Key_Comma);
            QTRY_VERIFY(app.number("time-pos") < nextFrame);
            const double second = app.number("time-pos");
            app.key(Qt::Key_Right, Qt::ShiftModifier);
            QTRY_VERIFY(std::abs(app.number("time-pos") - second - 1) < 0.05);
            app.key(Qt::Key_Left, Qt::ShiftModifier);
            QTRY_VERIFY(std::abs(app.number("time-pos") - second) < 0.05);
            app.key(Qt::Key_5);
            QTRY_VERIFY(std::abs(app.number("time-pos") - 30) < 0.1);
            app.key(Qt::Key_0);
            QTRY_VERIFY(app.number("time-pos") < 0.1);

            // A part on a loop: its start, its end, off. The seek bar shows it, and it plays round.
            const auto loopSet = [&](const char *name) { return app.mpv->observed(name).typeId() == QMetaType::Double; };
            app.mpv->command({"seek", 10, "absolute+exact"});
            QTRY_VERIFY(std::abs(app.number("time-pos") - 10) < 0.1);
            app.key(Qt::Key_L);
            QTRY_VERIFY(loopSet("ab-loop-a"));
            QVERIFY2(toast().contains("0:10"), qPrintable(toast()));
            app.mpv->command({"seek", 11, "absolute+exact"});
            QTRY_VERIFY(std::abs(app.number("time-pos") - 11) < 0.1);
            app.key(Qt::Key_L);
            QTRY_VERIFY(loopSet("ab-loop-b"));
            app.wake();
            QTRY_VERIFY(Launch::searchVisible(app.window->contentItem(), "loopEndMark"));
            QVERIFY(Launch::searchVisible(app.window->contentItem(), "loopStartMark"));
            app.shot("loop");
            app.key(Qt::Key_Space);
            QTRY_COMPARE(app.prop("pause"), QString("no"));
            QTRY_VERIFY(app.number("time-pos") > 10.6);
            QTRY_VERIFY(app.number("time-pos") < 10.4); // back at its start
            app.key(Qt::Key_L);
            QTRY_VERIFY(!loopSet("ab-loop-a"));
            QTRY_VERIFY(!Launch::searchVisible(app.window->contentItem(), "loopEndMark"));

            // A click on the time: what is left, and back.
            app.wake();
            QTRY_VERIFY(app.item("timeDisplay")->isVisible());
            QVERIFY(app.click("timeDisplay"));
            QTRY_VERIFY(app.item("timeText")->property("text").toString().startsWith("-"));
            QVERIFY(app.click("timeDisplay"));
            QTRY_VERIFY(!app.item("timeText")->property("text").toString().startsWith("-"));

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
            // With the title bar gone in full screen, minimize stays in reach, alone (maintainer).
            QVERIFY(!app.item("fullScreenButtons")->isVisible());
            app.key(Qt::Key_F);
            QTRY_COMPARE(app.window->visibility(), QWindow::FullScreen);
            app.wake();
            QTRY_VERIFY(app.item("fullScreenButtons")->isVisible());
            QVERIFY(Launch::searchVisible(app.item("fullScreenButtons"), "fullScreenMinimize"));
            QVERIFY(!app.item("fullScreenExit") && !app.item("fullScreenClose"));
            app.shot("fullscreen-buttons");
            app.key(Qt::Key_Escape);
            QTRY_COMPARE(app.window->visibility(), QWindow::Windowed);
            const QPoint center(app.window->width() / 2, app.window->height() / 2);
            QTest::mouseDClick(app.window, Qt::LeftButton, Qt::NoModifier, center);
            QTRY_COMPARE(app.window->visibility(), QWindow::FullScreen);
            QTest::mouseDClick(app.window, Qt::LeftButton, Qt::NoModifier, center);
            QTRY_COMPARE(app.window->visibility(), QWindow::Windowed);
            QTest::qWait(100);
            QTRY_COMPARE(app.prop("pause"), QString("no")); // a double click's first click takes nothing with it

            // A click on the video pauses and plays. One that closes something does only that. It can be turned off.
            QTest::mouseClick(app.window, Qt::LeftButton, Qt::NoModifier, center);
            QTRY_COMPARE(app.prop("pause"), QString("yes"));
            QTest::mouseClick(app.window, Qt::LeftButton, Qt::NoModifier, center);
            QTRY_COMPARE(app.prop("pause"), QString("no"));
            app.wake();
            QTRY_VERIFY(app.item("gearButton")->isVisible());
            QVERIFY(app.click("gearButton"));
            QTRY_VERIFY(app.item("quickPopover"));
            QTest::mouseClick(app.window, Qt::LeftButton, Qt::NoModifier, center);
            QTRY_VERIFY(!app.item("quickPopover"));
            QTest::qWait(100);
            QCOMPARE(app.prop("pause"), QString("no"));
            app.services.player.setClickPauses(false);
            QTest::mouseClick(app.window, Qt::LeftButton, Qt::NoModifier, center);
            QTest::qWait(100);
            QCOMPARE(app.prop("pause"), QString("no"));
            app.services.player.setClickPauses(true);

            // Held down, the picture plays at the fixed speed (G's) until the button is up, as on YouTube. That release
            // is no click: the clip plays on, at the speed it had.
            app.mpv->command({"set", "speed", "1.25"});
            QTRY_COMPARE(app.number("speed"), 1.25);
            const double fast = app.services.player.speedFactor();
            QTest::mousePress(app.window, Qt::LeftButton, Qt::NoModifier, center);
            QTRY_COMPARE(app.number("speed"), fast);
            QCOMPARE(toast(), text("speed_now").arg(fast));
            QTest::mouseRelease(app.window, Qt::LeftButton, Qt::NoModifier, center);
            QTRY_COMPARE(app.number("speed"), 1.25);
            QTest::qWait(100);
            QCOMPARE(app.prop("pause"), QString("no"));
            app.mpv->command({"set", "speed", "1"});
            QTRY_COMPARE(app.number("speed"), 1.0);

#ifdef Q_OS_WIN
            // Always on top: T, and the next run comes back with it.
            const auto onTop = [&] { return (GetWindowLongPtrW(HWND(app.window->winId()), GWL_EXSTYLE) & WS_EX_TOPMOST) != 0; };
            QVERIFY(!onTop());
            app.key(Qt::Key_T);
            QTRY_VERIFY(onTop());
            app.key(Qt::Key_T);
            QTRY_VERIFY(!onTop());
            app.key(Qt::Key_T);
            QTRY_VERIFY(onTop());
            QTRY_COMPARE(toast(), text("on_top_on"));
#endif

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
        // Picture and sound start together, at the start of the clip: nothing played while the window was coming up.
        QFile startLog(log);
        QVERIFY(startLog.open(QIODevice::ReadOnly));
        const QRegularExpressionMatch firstFrame = QRegularExpression("playback restart complete @ ([0-9.]+), audio=\\w+, video=playing")
                                                       .match(QString::fromUtf8(startLog.readAll()));
        QVERIFY(firstFrame.hasMatch());
        QVERIFY2(firstFrame.captured(1).toDouble() < 0.1, qPrintable("the first frame shown is at " + firstFrame.captured(1)));
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
        const QByteArray saved = resume.readAll();
        QVERIFY2(!saved.contains(QFileInfo(m_clip).baseName().toUtf8()), "resume.json holds a file name");
        const QString key = QString::fromLatin1(QCryptographicHash::hash(QDir::cleanPath(QDir::fromNativeSeparators(m_clip)).toUtf8(),
                                                                         QCryptographicHash::Sha256).toHex());
        const QJsonObject entry = QJsonDocument::fromJson(saved).object().value("entries").toObject().value(key).toObject();
        QVERIFY2(entry.value("pos").toDouble() >= 25 && entry.value("pos").toDouble() < 30, "resume.json lacks 0:25");
    }

    // No file on the command line: the start screen right away, and a dropped file plays.
    // A new version with nothing open: the card in the corner of the start screen. Not answered, so nothing downloads.
    void updateCardOnStartScreen()
    {
        const QString state = QDir(m_path).filePath("start-update");
        QVERIFY(QDir().mkpath(state));
        Launch app(state, QString(), QDir(m_path).filePath("start-update.log"), "2.0.0");
        QVERIFY(app.mpv);
        QTRY_VERIFY(app.item("startScreen"));
        app.services.updates.handleReply(NextRelease);
        QTRY_VERIFY(app.item("updateNotice"));
        QVERIFY(app.item("updateInstall"));
        app.shot("update-start");
    }

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
        // A name that may not say enough explains itself when the pointer rests on it.
        {
            QQuickItem *heading = app.item("audioDelayHeading");
            QVERIFY(heading);
            QVERIFY(!Launch::searchVisible(app.window->contentItem(), "hoverTip"));
            QTest::mouseMove(app.window, heading->mapToScene(QPointF(40, heading->height() / 2)).toPoint());
            QTRY_VERIFY(Launch::searchVisible(app.window->contentItem(), "hoverTip"));
            app.shot("quick-tip");
            QTest::mouseMove(app.window, QPoint(app.window->width() / 2, app.window->height() / 3));
            QTRY_VERIFY(!Launch::searchVisible(app.window->contentItem(), "hoverTip"));
        }
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
        QVERIFY(app.click("quickSubtitles")); // the clip has none: it opens anyway, to add a file
        QTRY_VERIFY(app.item("subtitlesPopover"));
        QVERIFY(!app.item("quickPopover"));
        QVERIFY(Launch::searchVisible(app.item("subtitlesPopover"), "subtitleAdd"));
        QVERIFY(app.click("gearButton"));
        QTRY_VERIFY(app.item("quickPopover"));

        // Opening settings over a playing clip: the longest time between two frames on screen (a stall shows there).
        std::atomic<qint64> lastFrame{0}, longestGap{0};
        QElapsedTimer frameClock;
        frameClock.start();
        const auto frames = QObject::connect(app.window, &QQuickWindow::frameSwapped, app.window, [&] {
            const qint64 now = frameClock.elapsed(), last = lastFrame.exchange(now);
            if (last > 0 && now - last > longestGap) longestGap = now - last;
        }, Qt::DirectConnection);
        QTest::qWait(300);
        longestGap = 0;
        QElapsedTimer opening;
        opening.start();
        QVERIFY(app.click("allSettings"));
        QTRY_VERIFY(app.item("settingsOverlay") && app.item("settingsOverlay")->isVisible());
        const qint64 openedIn = opening.elapsed();
        QTest::qWait(300);
        qInfo("settings: on screen after %lld ms, longest frame gap while opening %lld ms", openedIn, qint64(longestGap));
        QObject::disconnect(frames);
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
            if (QByteArray(tab) == "playback") // not clicked: it opens Windows' Settings on the screen
                QVERIFY(Launch::searchVisible(app.window->contentItem(), "defaultPlayerButton"));
            // No words run out of their row: this test's own screenshot folder is a long path with no spaces.
            const QList<QQuickItem *> words = Launch::searchAllVisible(app.window->contentItem(), "settingDescription");
            for (QQuickItem *text : words)
                QVERIFY2(text->property("contentWidth").toDouble() <= text->width(), qPrintable(text->property("text").toString()));
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
        // Every shipped language: its words fit their rows, and the shot shows its script is drawn (CJK, Cyrillic).
        for (const QVariant &entry : app.services.i18n.languages()) {
            const QString code = entry.toMap().value("code").toString();
            app.services.i18n.setLanguage(code);
            QTRY_COMPARE(app.services.i18n.language(), code);
            QTest::qWait(50);
            for (QQuickItem *text : Launch::searchAllVisible(app.window->contentItem(), "settingDescription"))
                QVERIFY2(text->property("contentWidth").toDouble() <= text->width(), qPrintable(code + ": " + text->property("text").toString()));
            app.shot("lang-" + code);
        }
        app.services.i18n.setLanguage("tr");
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

        // A new key for an action; a key another action uses is refused and the row being edited says why, until the
        // next step; reset.
        const auto keyMessage = [&](const char *row) { return Launch::search(app.item(row), "settingDescription"); };
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
        QVERIFY2(keyMessage("keyRow-mute")->property("text").toString().contains("Play or pause"), "the row does not say why");
        QCOMPARE(keyMessage("keyRow-mute")->property("color").value<QColor>(), QColor(app.services.theme.colors().value("danger").toString()));
        QVERIFY(keyMessage("keyRow-pause")->property("text").toString().isEmpty());
        app.key(Qt::Key_K); // works at once
        QTRY_VERIFY(app.mpv->observed("mute").toBool() == false);
        QVERIFY(app.click("key-mute")); // the next step: the message goes
        QTRY_VERIFY(keyMessage("keyRow-mute")->property("text").toString().isEmpty());
        app.key(Qt::Key_Escape);

        // Far down the list, with its first row out of sight, the message is on screen with the row.
        QQuickItem *rows = app.item("settingsRows");
        rows->setProperty("contentY", rows->property("contentHeight").toReal() - rows->height());
        QVERIFY(app.click("key-fullscreen"));
        app.key(Qt::Key_Space);
        QTRY_VERIFY(keyMessage("keyRow-fullscreen")->property("text").toString().contains("Play or pause"));
        QVERIFY(keyMessage("keyRow-fullscreen")->isVisible());
        const qreal messageTop = keyMessage("keyRow-fullscreen")->mapToItem(rows, QPointF(0, 0)).y();
        QVERIFY(messageTop >= 0 && messageTop + keyMessage("keyRow-fullscreen")->height() <= rows->height());
        app.shot("settings-key-refused");
        QVERIFY(app.click("settingsTab-appearance")); // another tab: it goes too
        QTRY_VERIFY(keyMessage("keyRow-fullscreen")->property("text").toString().isEmpty());
        QVERIFY(app.click("settingsTab-shortcuts"));

        QVERIFY(app.click("key-mute"));
        app.key(Qt::Key_Space);
        QTRY_VERIFY(!keyMessage("keyRow-mute")->property("text").toString().isEmpty());
        QVERIFY(app.click("resetShortcuts"));
        QTRY_VERIFY(app.services.player.shortcuts().isEmpty());
        QVERIFY(keyMessage("keyRow-mute")->property("text").toString().isEmpty());

        // Light mode, and the panel in the smallest window.
        app.services.theme.setMode("light");
        QVERIFY(app.click("settingsTab-appearance"));
        app.shot("settings-light");
        // Windows' accent color, made readable; a swatch takes over from it.
        QVERIFY(app.click("accentWindowsSwitch"));
        QTRY_COMPARE(app.services.theme.accentChoice(), QString("system"));
        QCOMPARE(app.services.theme.accent(), Theme::readableAccent(QGuiApplication::palette().color(QPalette::Accent)));
        app.shot("settings-accent-windows");
        QVERIFY(app.click("swatch-"));
        QTRY_COMPARE(app.services.theme.accentChoice(), QString());
        QVERIFY(!app.item("accentWindowsSwitch")->property("checked").toBool());
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
            // The chrome goes and the card stays: the glass behind it keeps following the video.
            QTRY_VERIFY(!app.item("controlBar")->isVisible());
            QVERIFY(app.item("resumeCard"));
            QVERIFY(app.item("glassBackdrop")->property("active").toBool());
            QVERIFY(app.click("resumeButton"));
            QTRY_VERIFY(app.number("time-pos") >= 25);
            QTRY_VERIFY(!app.item("resumeCard"));

            // A release with a setup: Install on close.
            app.services.updates.handleReply(NextRelease);
            QTRY_VERIFY(app.item("updateNotice"));
            QVERIFY(app.item("updateClose"));
            QCOMPARE(app.item("updateLater")->property("text").toString(), app.services.i18n.strings().value("install_on_close").toString());
            app.wake();
            app.shot("update");
            QTRY_VERIFY(!app.item("controlBar")->isVisible()); // this card outlasts the chrome too
            QVERIFY(app.item("updateNotice"));
            QVERIFY(app.item("glassBackdrop")->property("active").toBool());
            QSignalSpy installStarted(&app.services.updater, &Updater::changed);
            QVERIFY(app.click("updateLater"));
            QTRY_VERIFY(!app.item("updateNotice"));
            QVERIFY(installStarted.count() >= 1); // the click started the quiet download (which may have failed since)
            QCOMPARE(app.services.state.notesVersion(), QString("99.0.0")); // kept for when that version runs
            QVERIFY(!app.item("whatsNewCard")); // this version is not that one

            // After an update: this version's notes, once. The cross ends them.
            app.services.state.setReleaseNotes(app.services.updates.currentVersion(),
                "**No more stutter when picture quality switches tiers.** Auto mode's live switching used to freeze for a second or two the first time it happened in a session.\n\n"
                "**Your folder stays your folder.** A subfolder next to the video you opened no longer joins the playlist.\n\n"
                "- The mixer remembers more\n- Install on close\n- [Everything else](https://example.com)\n\n"
                "**When video ends actually sticks now.** Picking Play next, Repeat or Stop at end used to revert on the next file.\n\n"
                "**Full Changelog**: https://github.com/huseyincancalti/EchoPlay/compare/v1.4.0...v2.0.0"); // as GitHub writes it
            emit app.services.updates.changed();
            QTRY_VERIFY(app.item("whatsNewCard"));
            app.wake();
            app.shot("whats-new");
            QVERIFY(app.item("whatsNewText")->property("contentWidth").toDouble() <= app.item("whatsNewText")->width());
            QVERIFY(app.click("whatsNewClose"));
            QTRY_VERIFY(!app.item("whatsNewCard"));
            QCOMPARE(app.services.updates.whatsNew(), QString());

            // Over a clip it also ends by itself, for good, unless the pointer rests on it.
            app.services.state.setReleaseNotes(app.services.updates.currentVersion(), "Short notes");
            emit app.services.updates.changed();
            QTRY_VERIFY(app.item("whatsNewCard"));
            app.item("whatsNewCard")->setProperty("closeAfter", 200);
            QTRY_VERIFY(!app.item("whatsNewCard"));
            QCOMPARE(app.services.updates.whatsNew(), QString());

            // About while the quiet download runs, and once it waits for the close. From a server in this test; never started.
            app.services.updater.cancel();
            const QByteArray setup(3 * 1048576, 'x');
            QTcpServer server;
            QVERIFY(server.listen(QHostAddress::LocalHost));
            QTcpSocket *download = nullptr;
            connect(&server, &QTcpServer::newConnection, this, [&] {
                download = server.nextPendingConnection();
                connect(download, &QTcpSocket::readyRead, this, [&] {
                    download->readAll();
                    download->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(setup.size()) + "\r\n\r\n" + setup.left(1048576));
                }, Qt::SingleShotConnection);
            });
            app.services.updater.installOnClose(QString("http://127.0.0.1:%1/setup").arg(server.serverPort()),
                                                QString::fromLatin1(QCryptographicHash::hash(setup, QCryptographicHash::Sha256).toHex()), "99.0.0");
            QVERIFY(QMetaObject::invokeMethod(app.item("controlBar")->parentItem(), "openSettings"));
            QTRY_VERIFY(app.item("settingsOverlay"));
            QTRY_VERIFY(app.click("settingsTab-about")); // the overlay is built in steps (asynchronous)
            QTRY_COMPARE(app.services.updater.received(), qint64(1048576));
            QCOMPARE(app.services.updater.total(), qint64(setup.size())); // known from the first bytes, or the text says "1 / 0 MB"
            app.shot("about-quiet-download");
            download->write(setup.mid(1048576));
            QTRY_COMPARE(app.services.updater.state(), QString("ready"));
            app.shot("about-waiting-for-close");
            QVERIFY(QFile::remove(app.services.updater.setupOnClose()));
            app.key(Qt::Key_Escape);
            QTRY_VERIFY(!app.item("settingsOverlay"));

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

            QTRY_VERIFY(app.item("controlBar"));
            app.key(Qt::Key_L); // a part on a loop means nothing in the next file
            QTRY_VERIFY(app.mpv->observed("ab-loop-a").typeId() == QMetaType::Double);

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
            QTRY_VERIFY(app.mpv->observed("ab-loop-a").typeId() != QMetaType::Double);
            QTest::qWait(300);
            QVERIFY(app.item("errorScreen")); // mpv did not go on to the next clip by itself
            app.shot("error");
            QVERIFY(Launch::searchVisible(app.window->contentItem(), "errorNext")); // b-good.mp4 comes after it
            QVERIFY(Launch::searchVisible(app.window->contentItem(), "errorReport"));
            QVERIFY(Launch::searchVisible(app.window->contentItem(), "errorCopy"));
            QVERIFY(Launch::searchVisible(app.window->contentItem(), "errorOpen"));
            // The whole report is on the card from the start. Copy details would overwrite the clipboard of whoever sits
            // at the PC: the report is read from the screen.
            const QString text = app.item("errorText")->property("text").toString();
            QVERIFY2(text.contains("Cause, from the mpv engine: unrecognized file format."), qPrintable(text));
            QVERIFY2(text.contains("Failed to recognize file format"), qPrintable(text)); // mpv's last line, after the failure
            QVERIFY2(text.contains("a-broken.mp4"), qPrintable(text));
            QVERIFY2(text.contains("Last log lines:"), qPrintable(text));
            QVERIFY2(!text.contains(QDir::home().dirName(), Qt::CaseInsensitive), qPrintable(text));

            // Next clip works from the plain view.
            QVERIFY(QMetaObject::invokeMethod(app.window, "playNextClip"));
            QTRY_VERIFY(!app.item("errorScreen"));
            QTRY_COMPARE(app.window->title(), QString("b-good.mp4"));
        }
    }

    // "Resume from 0:25?": it goes after 10 s or as soon as the person does something else, and the saved spot stays
    // until there is a new one worth saving or they choose Start over.
    void resumeQuestion()
    {
        const QString state = QDir(m_path).filePath("resume");
        QVERIFY(QDir().mkpath(state));
        double spot = 0;
        {
            Launch app(state, m_clip, QDir(m_path).filePath("resume1.log"));
            QVERIFY(app.mpv);
            QVERIFY2(app.playing(), "the clip did not start playing");
            app.mpv->command({"seek", 25, "absolute"});
            QTRY_VERIFY(app.number("time-pos") >= 25);
            app.window->close();
            spot = app.services.resume.position(m_clip);
            QVERIFY(spot >= 25);
        }
        {
            Launch app(state, m_clip, QDir(m_path).filePath("resume2.log"));
            QVERIFY(app.mpv);
            QVERIFY2(app.playing(), "the clip did not start playing");
            QTRY_VERIFY(app.item("resumeCard"));
            QElapsedTimer shown;
            shown.start();

            // None of these ends it: the pause and volume keys, the pointer moving, a click on the card itself.
            app.key(Qt::Key_Space);
            QTRY_COMPARE(app.prop("pause"), QString("yes"));
            const double volume = app.number("volume");
            app.key(Qt::Key_Down);
            QTRY_COMPARE(app.number("volume"), volume - 5);
            app.wake();
            QQuickItem *card = app.item("resumeCard");
            QTest::mouseClick(app.window, Qt::LeftButton, Qt::NoModifier, card->mapToScene(QPointF(card->width() - 8, 8)).toPoint());
            QTest::qWait(100);
            QVERIFY(app.item("resumeCard"));

            // Left alone it goes after 10 s; closing the clip this early, under 15 s, keeps the spot.
            QTRY_VERIFY_WITH_TIMEOUT(!app.item("resumeCard"), 12000);
            QVERIFY2(shown.elapsed() > 8000, "the card went too soon");
            QVERIFY(app.number("time-pos") < 15);
            app.window->close();
            QCOMPARE(app.services.resume.position(m_clip), spot);
        }
        {
            Launch app(state, m_clip, QDir(m_path).filePath("resume3.log"));
            QVERIFY(app.mpv);
            QVERIFY2(app.playing(), "the clip did not start playing");
            QCOMPARE(app.services.resume.position(m_clip), spot); // it came through the restart
            QTRY_VERIFY(app.item("resumeCard"));
            app.key(Qt::Key_Space); // paused: the clip stays where the checks put it, the bar stays up
            QTRY_COMPARE(app.prop("pause"), QString("yes"));
            // The question again, as this clip's first restart raised it.
            const auto ask = [&] {
                app.window->setProperty("resumePending", true);
                QMetaObject::invokeMethod(app.window, "applyResume");
                return QTest::qWaitFor([&] { return app.item("resumeCard") != nullptr; }, 3000);
            };

            // Clicks outside the card end it, and still do what they do.
            QTest::mouseClick(app.window, Qt::LeftButton, Qt::NoModifier, QPoint(app.window->width() / 2, app.window->height() / 2));
            QTRY_VERIFY(!app.item("resumeCard"));
            QTest::qWait(100);
            QCOMPARE(app.prop("pause"), QString("yes")); // on the video, that click only ended the question
            QVERIFY(ask());
            QVERIFY(app.click("seekBar"));
            QTRY_VERIFY(!app.item("resumeCard"));
            QTRY_VERIFY(app.number("time-pos") > 25 && app.number("time-pos") < 35); // the middle of 60 s
            QVERIFY(ask());
            QVERIFY(app.click("playButton"));
            QTRY_VERIFY(!app.item("resumeCard"));
            QTRY_COMPARE(app.prop("pause"), QString("no"));
            app.key(Qt::Key_Space);
            QTRY_COMPARE(app.prop("pause"), QString("yes"));

            // A seek by arrow key.
            QVERIFY(ask());
            app.key(Qt::Key_Left);
            QTRY_VERIFY(!app.item("resumeCard"));

            // A popover, the playlist and settings, opened without a click. It does not come back when they close.
            QVERIFY(ask());
            app.item("mixerChip")->forceActiveFocus();
            app.key(Qt::Key_Return);
            QTRY_VERIFY(app.item("mixerPopover"));
            QTRY_VERIFY(!app.item("resumeCard"));
            app.key(Qt::Key_Escape);
            QTRY_VERIFY(!app.item("mixerPopover"));
            QVERIFY(ask());
            app.item("playlistButton")->forceActiveFocus();
            app.key(Qt::Key_Return);
            QTRY_VERIFY(app.item("playlistPanel"));
            QTRY_VERIFY(!app.item("resumeCard"));
            app.key(Qt::Key_Escape);
            QTRY_VERIFY(!app.item("playlistPanel"));
            QVERIFY(ask());
            QVERIFY(QMetaObject::invokeMethod(app.item("controlBar")->parentItem(), "openSettings"));
            QTRY_VERIFY(app.item("settingsOverlay"));
            QTRY_VERIFY(!app.item("resumeCard"));
            app.key(Qt::Key_Escape);
            QTRY_VERIFY(!app.item("settingsOverlay"));
            QTest::qWait(100);
            QVERIFY(!app.item("resumeCard"));

            // Passed by, the spot stays under 15 s and a new one from 15 s on replaces it.
            app.mpv->command({"seek", 5, "absolute"});
            QTRY_VERIFY(app.number("time-pos") < 6);
            QVERIFY(QMetaObject::invokeMethod(app.window, "rememberPosition"));
            QVERIFY(app.services.resume.position(m_clip) >= 15);
            app.mpv->command({"seek", 20, "absolute"});
            QTRY_VERIFY(app.number("time-pos") >= 20 && app.number("time-pos") < 25);
            QVERIFY(QMetaObject::invokeMethod(app.window, "rememberPosition"));
            QVERIFY(app.services.resume.position(m_clip) >= 20 && app.services.resume.position(m_clip) < 25);

            // Start over: under 15 s the spot goes.
            QVERIFY(ask());
            QVERIFY(app.click("startOverButton"));
            QTRY_VERIFY(!app.item("resumeCard"));
            app.mpv->command({"seek", 5, "absolute"});
            QTRY_VERIFY(app.number("time-pos") < 6);
            app.window->close();
            QCOMPARE(app.services.resume.position(m_clip), -1.0);
        }
    }

    // The clips of the opened file's folder: thumbnails and track counts arrive, a click plays one.
    void playlist()
    {
        const QString state = QDir(m_path).filePath("playlist");
        const QString clips = QDir(m_path).filePath("clips");
        QVERIFY(QDir().mkpath(state));
        QVERIFY(QDir().mkpath(clips));
        const QString edge = QFileInfo(m_clip).absolutePath() + "/edge/video-only.mp4";
        QFile::copy(m_clip, clips + "/clip 1.mp4");
        QFile::copy(edge, clips + "/clip 2.mp4");
        QFile::copy(m_clip, clips + "/clip 10.mp4");
        Launch app(state, clips + "/clip 1.mp4", QDir(m_path).filePath("playlist.log"));
        QVERIFY(app.mpv);
        QVERIFY2(app.playing(), "the clip did not start playing");
        QTRY_VERIFY(app.item("controlBar"));
        app.wake();
        QTRY_VERIFY(app.item("playlistButton")->isVisible());
        QVERIFY(app.click("playlistButton"));
        QTRY_VERIFY(app.item("playlistPanel"));
        QTRY_VERIFY(app.item("playlistRow-2"));
        // As tall as its three clips (title 60, rows 70 each, margin 10), standing on the bar.
        QQuickItem *panel = app.item("playlistPanel");
        QTRY_COMPARE(panel->height(), 60.0 + 3 * 70 + 10);
        QTRY_VERIFY(panel->mapToScene(QPointF(0, panel->height())).y() <= app.item("controlBar")->mapToScene(QPointF()).y());
        // Natural order, from mpv: 1, 2, 10. The rows on screen get read in the background.
        const auto info = [&](const char *name) { // mpv's playlist has the native path
            return app.services.clipInfo.info(QDir::toNativeSeparators(clips + "/" + name));
        };
        QTRY_VERIFY_WITH_TIMEOUT(!info("clip 10.mp4").isEmpty(), 10000);
        QCOMPARE(info("clip 1.mp4").value("audioTracks").toInt(), 2);
        QCOMPARE(info("clip 2.mp4").value("audioTracks").toInt(), 0);
        // What the rows themselves got: the C++ side alone passed while every row stayed empty.
        const auto rowInfo = [&](const char *row) { return app.item(row)->property("info").toMap(); };
        QTRY_COMPARE(rowInfo("playlistRow-0").value("audioTracks").toInt(), 2);
        QVERIFY(rowInfo("playlistRow-0").value("duration").toDouble() > 50);
        QVERIFY(rowInfo("playlistRow-0").value("thumbnail").toString().startsWith("image://clipinfo/"));
        QTRY_COMPARE(rowInfo("playlistRow-1").value("audioTracks").toInt(), 0);
        QTest::qWait(100);
        app.shot("playlist");
        QVERIFY(app.click("playlistRow-1"));
        QTRY_COMPARE(app.window->title(), QString("clip 2.mp4"));
        // The mixer's popover and the panel share the bar's right end: one closes the other.
        app.wake();
        QVERIFY(app.click("mixerChip"));
        QTRY_VERIFY(!app.item("playlistPanel"));
        app.key(Qt::Key_Escape);
        QTRY_VERIFY(!app.item("mixerPopover"));
    }

    // The right-click menu, on the video and on playlist rows. Delete is real: hard links to the test clips go to the
    // Recycle Bin, and the test takes them out of it again. Show in folder and Open file are not clicked: they open
    // Explorer or the open dialog on the screen, outside the test window.
    void contextMenu()
    {
#ifndef Q_OS_WIN
        QSKIP("the Recycle Bin check reads Windows' own bin");
#else
        const QString state = QDir(m_path).filePath("menu");
        const QString clips = QDir(m_path).filePath("menu-clips");
        QVERIFY(QDir().mkpath(state));
        QVERIFY(QDir().mkpath(clips));
        const QString edge = QFileInfo(m_clip).absolutePath() + "/edge/video-only.mp4";
        const auto link = [&](const QString &from, const char *name) { // no copies of a 100 MB clip
            return CreateHardLinkW(reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(clips + "/" + name).utf16()),
                                   reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(from).utf16()), nullptr) != 0;
        };
        QVERIFY(link(m_clip, "a.mp4") && link(m_clip, "b.mp4") && link(m_clip, "c.mp4") && link(edge, "d.mp4"));
        const auto takeBack = qScopeGuard([&] { removeFromTrash(clips); });
        Launch app(state, clips + "/a.mp4", QDir(m_path).filePath("menu.log"));
        QVERIFY(app.mpv);
        QVERIFY2(app.playing(), "the clip did not start playing");
        QTRY_VERIFY(app.item("controlBar"));
        const QPoint middle(app.window->width() / 2, app.window->height() / 2);
        const auto rightClick = [&](QPoint at) {
            QTest::qWait(60);
            QTest::mouseClick(app.window, Qt::RightButton, Qt::NoModifier, at);
        };
        const auto menu = [&] { return app.item("contextMenu"); };
        const auto playlistSize = [&] { return app.window->property("playlist").toList().size(); };

        // At the pointer, with every row. A click outside closes it, and only that: no pause.
        rightClick(middle);
        QTRY_VERIFY(menu());
        const QPoint at = menu()->mapToScene(QPointF()).toPoint();
        QCOMPARE(at.x(), middle.x()); // at the pointer, moved up only as far as the window needs
        QVERIFY(at.y() <= middle.y() && (at.y() == middle.y() || qRound(at.y() + menu()->height()) == app.window->height() - 8));
        for (const char *row : {"menuOpen", "menuShowInFolder", "menuDelete", "menuScreenshot", "menuCopyFrame", "menuOnTop", "menuSettings"})
            QVERIFY2(Launch::searchVisible(menu(), row), row);
        app.shot("menu");
        QTest::mouseClick(app.window, Qt::LeftButton, Qt::NoModifier, QPoint(40, middle.y()));
        QTRY_VERIFY(!menu());
        QTest::qWait(300);
        QCOMPARE(app.prop("pause"), QString("no"));
        // Near the corner it opens up and to the left, inside the window. Esc closes it.
        rightClick(QPoint(app.window->width() - 20, app.item("controlBar")->mapToScene(QPointF()).y() - 10));
        QTRY_VERIFY(menu());
        const QRectF placed(menu()->mapToScene(QPointF()), menu()->size());
        QVERIFY(QRectF(0, 0, app.window->width(), app.window->height()).contains(placed));
        app.key(Qt::Key_Escape);
        QTRY_VERIFY(!menu());

        // Always on top, both ways; the switch shows the state.
        const auto onTop = [&] { return (GetWindowLongPtrW(HWND(app.window->winId()), GWL_EXSTYLE) & WS_EX_TOPMOST) != 0; };
        QVERIFY(!onTop());
        rightClick(middle);
        QVERIFY(app.click("menuOnTop"));
        QTRY_VERIFY(app.services.player.alwaysOnTop() && onTop());
        QTRY_VERIFY(!menu());
        rightClick(middle);
        QTRY_VERIFY(menu());
        app.shot("menu-on-top");
        QVERIFY(app.click("menuOnTop"));
        QTRY_VERIFY(!app.services.player.alwaysOnTop() && !onTop());

        // Screenshot, as F1 does: one more picture in the folder.
        const QDir shots(app.prop("screenshot-dir"));
        const qsizetype before = shots.entryList({"*.png"}, QDir::Files).size();
        rightClick(middle);
        QVERIFY(app.click("menuScreenshot"));
        QTRY_COMPARE_WITH_TIMEOUT(shots.entryList({"*.png"}, QDir::Files).size(), before + 1, 5000);

        // All settings.
        rightClick(middle);
        QVERIFY(app.click("menuSettings"));
        QTRY_VERIFY(app.item("settingsOverlay"));
        app.key(Qt::Key_Escape);
        QTRY_VERIFY(!app.item("settingsOverlay"));

        // Delete asks first, in the menu's place; Cancel keeps the file.
        rightClick(middle);
        QVERIFY(app.click("menuDelete"));
        QTRY_VERIFY(app.item("menuConfirm")->isVisible());
        app.shot("menu-delete");
        QVERIFY(app.click("menuCancel"));
        QTRY_VERIFY(!menu());
        QVERIFY(QFile::exists(clips + "/a.mp4"));

        const auto deleteFromMenu = [&](QPoint at) {
            rightClick(at);
            QTRY_VERIFY(menu());
            QVERIFY(app.click("menuDelete"));
            QVERIFY(app.click("menuConfirmDelete"));
        };
        // The playing clip, while mpv reads it: it goes, and the next one plays.
        deleteFromMenu(middle);
        QTRY_VERIFY(!QFile::exists(clips + "/a.mp4"));
        QTRY_COMPARE(app.window->title(), QString("b.mp4"));
        QTRY_COMPARE(playlistSize(), 3);
        // A playlist row that is not playing: only its two rows; it leaves the list, b keeps playing.
        app.wake();
        QTRY_VERIFY(app.item("playlistButton")->isVisible());
        QVERIFY(app.click("playlistButton"));
        QTRY_VERIFY(app.item("playlistRow-2"));
        const auto rowCenter = [&](const char *row) {
            QQuickItem *item = app.item(row);
            return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
        };
        rightClick(rowCenter("playlistRow-2"));
        QTRY_VERIFY(menu());
        QVERIFY(!Launch::searchVisible(menu(), "menuOpen") && !Launch::searchVisible(menu(), "menuSettings"));
        app.shot("menu-row");
        QVERIFY(app.click("menuDelete"));
        QVERIFY(app.click("menuConfirmDelete"));
        QTRY_VERIFY(!QFile::exists(clips + "/d.mp4"));
        QTRY_COMPARE(playlistSize(), 2);
        QVERIFY(app.item("playlistPanel"));
        QCOMPARE(app.window->title(), QString("b.mp4"));
        // The last clip of the list, playing: the one before it plays.
        QVERIFY(app.click("playlistRow-1"));
        QTRY_COMPARE(app.window->title(), QString("c.mp4"));
        app.key(Qt::Key_Escape);
        QTRY_VERIFY(!app.item("playlistPanel"));
        deleteFromMenu(middle);
        QTRY_VERIFY(!QFile::exists(clips + "/c.mp4"));
        QTRY_COMPARE(app.window->title(), QString("b.mp4"));
        QTRY_COMPARE(playlistSize(), 1);
        // The only clip: the start screen comes back.
        deleteFromMenu(middle);
        QTRY_VERIFY(!QFile::exists(clips + "/b.mp4"));
        QTRY_VERIFY(app.item("startScreen"));
        QTRY_VERIFY(!app.item("controlBar"));
        app.shot("menu-nothing-left");
        // All four are in the Recycle Bin, not deleted for good.
        QCOMPARE(removeFromTrash(clips), 4);
#endif
    }

    // Picture: brightness, contrast, saturation and gamma are kept; zoom, rotation and aspect ratio last one clip.
    void pictureAdjust()
    {
        const QString state = QDir(m_path).filePath("picture");
        QVERIFY(QDir().mkpath(state));
        const auto observe = [](Launch &app) {
            for (const char *name : {"brightness", "contrast", "video-zoom", "video-rotate", "video-aspect-override"})
                app.mpv->observeProperty(name);
        };
        // A click on a -100..100 slider, at a fraction of its length.
        const auto slide = [](Launch &app, const char *name, double fraction) {
            QQuickItem *slider = app.item(name);
            QTest::mouseClick(app.window, Qt::LeftButton, Qt::NoModifier,
                              slider->mapToScene(QPointF(slider->width() * fraction, slider->height() / 2)).toPoint());
        };
        {
            Launch app(state, m_clip, QDir(m_path).filePath("picture.log"));
            QVERIFY(app.mpv);
            observe(app);
            QVERIFY2(app.playing(), "the clip did not start playing");
            app.wake();
            QTRY_VERIFY(app.item("gearButton") && app.item("gearButton")->isVisible());
            QVERIFY(app.click("gearButton"));
            QTRY_VERIFY(app.item("quickPicture"));
            QCOMPARE(app.item("quickPicture")->property("note").toString(), QString());
            QVERIFY(app.click("quickPicture"));
            QTRY_VERIFY(app.item("picturePopover"));
            QVERIFY(!app.item("pictureReset")->isVisible());
            app.shot("picture");
            slide(app, "picture-brightness", 0.75);
            QTRY_COMPARE(app.services.player.picture().value("brightness").toInt(), 50);
            QTRY_COMPARE(app.number("brightness"), 50.0);
            QVERIFY(app.item("pictureReset")->isVisible());
            QVERIFY(app.click("pictureZoomMore")); // 110%
            QVERIFY(app.click("pictureZoomMore")); // 125%
            QTRY_VERIFY(std::abs(app.number("video-zoom") - std::log2(1.25)) < 0.001);
            QVERIFY(app.click("segment-90"));
            QTRY_COMPARE(app.number("video-rotate"), 90.0);
            QVERIFY(app.click("segment-4:3"));
            QTRY_VERIFY(std::abs(app.number("video-aspect-override") - 4.0 / 3) < 0.01);
            app.shot("picture-changed");
            // Reset takes all of it back.
            QVERIFY(app.click("pictureReset"));
            QTRY_COMPARE(app.number("brightness"), 0.0);
            QTRY_COMPARE(app.number("video-zoom"), 0.0);
            QTRY_COMPARE(app.number("video-rotate"), 0.0);
            QTRY_VERIFY(app.number("video-aspect-override") <= 0); // "no": the clip's own
            QTRY_VERIFY(!app.item("pictureReset")->isVisible());
            // The next file: the rotation is gone, the contrast stays. The quick settings say something changed.
            QVERIFY(app.click("segment-180"));
            QTRY_COMPARE(app.number("video-rotate"), 180.0);
            slide(app, "picture-contrast", 0.25);
            QTRY_COMPARE(app.number("contrast"), -50.0);
            app.mpv->command(QVariantList{"loadfile", m_clip});
            QTRY_COMPARE(app.number("video-rotate"), 0.0);
            QCOMPARE(app.number("contrast"), -50.0);
            app.key(Qt::Key_Escape);
            QTRY_VERIFY(!app.item("picturePopover"));
            app.wake();
            QVERIFY(app.click("gearButton"));
            QTRY_VERIFY(app.item("quickPicture"));
            QVERIFY(!app.item("quickPicture")->property("note").toString().isEmpty());
        }
        // And the next start.
        Launch app(state, m_clip, QDir(m_path).filePath("picture-2.log"));
        QVERIFY(app.mpv);
        observe(app);
        QVERIFY2(app.playing(), "the clip did not start playing");
        QTRY_COMPARE(app.number("contrast"), -50.0);
        QCOMPARE(app.number("video-rotate"), 0.0);
    }

    // Subtitles from a file (a Turkish one in cp1254, the old Windows code page, must read right), their delay,
    // size and height, and the audio delay. Size and height are kept; both delays last one clip.
    void subtitlesAndSync()
    {
        const QString state = QDir(m_path).filePath("subs");
        const QString folder = QDir(m_path).filePath("subs-clip");
        QVERIFY(QDir().mkpath(state));
        QVERIFY(QDir().mkpath(folder));
        const QString srt = folder + "/turkish.srt";
        QFile file(srt);
        QVERIFY(file.open(QIODevice::WriteOnly));
        // "Şimdi ığdır çöğüş İstanbul'da güneşli" in cp1254, split where a hex escape would eat the next letter.
        file.write("1\r\n00:00:00,000 --> 00:01:00,000\r\n"
                   "\xDE" "imdi \xFD\xF0" "d\xFDr \xE7\xF6\xF0\xFC\xFE \xDD" "stanbul'da g\xFC" "ne\xFE" "li\r\n");
        file.close();
        Launch app(state, m_clip, QDir(m_path).filePath("subs.log"));
        QVERIFY(app.mpv);
        for (const char *name : {"sid", "sub-text", "sub-delay", "audio-delay", "sub-scale", "sub-pos", "sub-margin-y"})
            app.mpv->observeProperty(name);
        QVERIFY2(app.playing(), "the clip did not start playing");
        QTRY_VERIFY(app.item("controlBar"));
        // What the open dialog and a dropped file both end in.
        QVERIFY(QMetaObject::invokeMethod(app.window, "addSubtitle", Q_ARG(QVariant, QUrl::fromLocalFile(srt))));
        QTRY_VERIFY(app.number("sid") > 0);
        QTRY_COMPARE_WITH_TIMEOUT(app.prop("sub-text"), QString("Şimdi ığdır çöğüş İstanbul'da güneşli"), 5000);
        app.wake();
        QTRY_VERIFY(app.item("subtitlesButton")->isVisible());
        QVERIFY(app.click("subtitlesButton"));
        QTRY_VERIFY(app.item("subtitlesPopover"));
        QVERIFY(app.click("subtitleDelayMore"));
        QVERIFY(app.click("subtitleDelayMore"));
        QTRY_VERIFY(std::abs(app.number("sub-delay") - 0.2) < 0.001);
        QVERIFY(app.click("subtitleSizeMore"));
        QTRY_VERIFY(std::abs(app.number("sub-scale") - 1.1) < 0.001);
        QVERIFY(app.click("subtitleHeightMore"));
        QTRY_COMPARE(app.number("sub-pos"), 95.0);
        app.shot("subtitles");
        QVERIFY(app.number("sub-margin-y") > 100); // above the bar while it shows
        // Off and on again from the list.
        QVERIFY(app.click("subtitleRow-0"));
        QTRY_COMPARE(app.prop("sid"), QString("no"));
        QVERIFY(!Launch::searchVisible(app.item("subtitlesPopover"), "subtitleDelay"));
        QVERIFY(app.click("subtitleRow-1"));
        QTRY_COMPARE(app.number("sid"), 1.0);
        app.key(Qt::Key_Escape);
        QTRY_VERIFY(!app.item("subtitlesPopover"));
        QTRY_VERIFY_WITH_TIMEOUT(!app.item("controlBar")->isVisible(), 4000);
        QTRY_COMPARE(app.number("sub-margin-y"), 22.0); // the bar is gone: back down
        app.shot("subtitles-idle");
        // Audio delay, in the gear's popover.
        app.wake();
        QVERIFY(app.click("gearButton"));
        QTRY_VERIFY(app.item("quickPopover"));
        QVERIFY(app.click("audioDelayMore"));
        QTRY_VERIFY(std::abs(app.number("audio-delay") - 0.05) < 0.001);
        app.shot("quick-audio-delay");
        app.key(Qt::Key_Escape);
        // The next file: both delays are gone, size and height stay.
        app.mpv->command(QVariantList{"loadfile", m_clip});
        QTRY_COMPARE(app.number("sub-delay"), 0.0);
        QTRY_COMPARE(app.number("audio-delay"), 0.0);
        QVERIFY(std::abs(app.number("sub-scale") - 1.1) < 0.001);
        QCOMPARE(app.number("sub-pos"), 95.0);
        QVERIFY(std::abs(app.services.player.subtitleScale() - 1.1) < 0.001);
        QCOMPARE(app.services.player.subtitlePosition(), 95);
    }

    // Clip info: the file and each track, as the container reports them; Copy gives the same as text.
    void mediaInfo()
    {
        const QString state = QDir(m_path).filePath("info");
        QVERIFY(QDir().mkpath(state));
        Launch app(state, m_clip, QDir(m_path).filePath("info.log"));
        QVERIFY(app.mpv);
        QVERIFY2(app.playing(), "the clip did not start playing");
        QTRY_VERIFY(app.item("controlBar"));
        app.key(Qt::Key_I);
        QTRY_VERIFY(app.item("mediaInfo"));
        const auto text = [&](const char *name) {
            QQuickItem *item = app.item(name);
            return item ? item->property("details").toString() : QString("(missing)");
        };
        QTRY_VERIFY(app.item("mediaInfoFile")->property("text").toString().contains(" MB · 1:00 · "));
        const QString video = text("mediaInfoTrack-video-1");
        QVERIFY2(video.contains("H.264") && video.contains("1280 × 720") && video.contains("60 fps"), qPrintable(video));
        for (const char *audio : {"mediaInfoTrack-audio-1", "mediaInfoTrack-audio-2"})
            QVERIFY2(text(audio).contains("AAC") && text(audio).contains("kHz"), qPrintable(text(audio)));
        app.shot("media-info");
        // What Copy puts on the clipboard, read without clicking it: that would overwrite the clipboard of whoever sits
        // at the PC.
        QVariant copied;
        QVERIFY(QMetaObject::invokeMethod(app.item("mediaInfo"), "asText", Q_RETURN_ARG(QVariant, copied)));
        QVERIFY2(copied.toString().contains("Microphone") && copied.toString().contains("H.264"), qPrintable(copied.toString()));
        app.key(Qt::Key_Escape);
        QTRY_VERIFY(!app.item("mediaInfo"));
        // From the right-click menu too.
        QTest::mouseClick(app.window, Qt::RightButton, Qt::NoModifier, QPoint(app.window->width() / 2, app.window->height() / 2));
        QTRY_VERIFY(app.item("contextMenu"));
        QVERIFY(app.click("menuInfo"));
        QTRY_VERIFY(app.item("mediaInfo"));
        QVERIFY(app.click("mediaInfoClose"));
        QTRY_VERIFY(!app.item("mediaInfo"));
    }

    // A video without sound, opened the real way. With nothing to play before the picture is on, the clip ends at once
    // and is played again; loading its path a second time tripped mpv's playlist loop guard and nothing ever played.
    // EchoPlay.exe's bench mode (tools/e2e.sh's edge clips) builds no folder playlist and never saw it. Alone in its
    // folder, and as the first of two: there mpv goes on to the second clip before the first one gets its turn again.
    void videoWithoutSound()
    {
        for (const bool alone : {true, false}) {
            const QString name = alone ? "silent-alone" : "silent-first";
            const QString folder = QDir(m_path).filePath(name + "-clips");
            QVERIFY(QDir().mkpath(QDir(m_path).filePath(name)));
            QVERIFY(QDir().mkpath(folder));
            const QString clip = folder + "/a-no-sound.mp4";
            QProcess ffmpeg;
            ffmpeg.start("ffmpeg", {"-v", "error", "-y", "-i", m_clip, "-t", "20", "-an", "-c", "copy", clip});
            QVERIFY2(ffmpeg.waitForFinished(30000) && ffmpeg.exitCode() == 0, ffmpeg.readAllStandardError().constData());
            if (!alone) QVERIFY(QFile::copy(m_clip, folder + "/b-with-sound.mp4"));

            Launch app(QDir(m_path).filePath(name), clip, QDir(m_path).filePath(name + ".log"));
            QVERIFY(app.mpv);
            QVERIFY2(app.playing(), qPrintable(name + ": the clip without sound did not start playing"));
            QCOMPARE(QFileInfo(app.prop("path")).fileName(), QString("a-no-sound.mp4"));
            QVERIFY(!app.item("errorScreen"));
            app.window->close();
        }
    }

    // A clip with no picture at all, the real way too: the sound plays (Launch::playing would wait for a picture).
    void soundWithoutPicture()
    {
        const QString folder = QDir(m_path).filePath("sound-only-clips");
        QVERIFY(QDir().mkpath(QDir(m_path).filePath("sound-only")));
        QVERIFY(QDir().mkpath(folder));
        const QString clip = folder + "/sound.m4a";
        QProcess ffmpeg;
        ffmpeg.start("ffmpeg", {"-v", "error", "-y", "-i", m_clip, "-t", "20", "-map", "0:a:0", "-c", "copy", clip});
        QVERIFY2(ffmpeg.waitForFinished(30000) && ffmpeg.exitCode() == 0, ffmpeg.readAllStandardError().constData());

        Launch app(QDir(m_path).filePath("sound-only"), clip, QDir(m_path).filePath("sound-only.log"));
        QVERIFY(app.mpv);
        app.mpv->setProperty("mute", true);
        QTRY_VERIFY_WITH_TIMEOUT(app.prop("pause") == "no" && app.number("time-pos") > 0.3 && app.uiLoaded(), 30000);
        QCOMPARE(QFileInfo(app.prop("path")).fileName(), QString("sound.m4a"));
        QVERIFY(!app.item("errorScreen"));
        app.shot("sound-only");
        app.window->close();
    }

    // Chapters, as an MP4 carries them (OBS's hybrid MP4 writes chapter markers): gaps in the seek bar, the hovered
    // one's title next to the time, PgUp/PgDown step through them with a toast.
    // Tracks say what they are: a title and a language ("Commentary", "English"), read the way people write them,
    // in the mixer and in the subtitles list. Untitled ones (NVIDIA App writes none) go by number: "Track 1 + 2".
    void trackLabels()
    {
        const QString folder = QDir(m_path).filePath("labels-clip");
        QVERIFY(QDir().mkpath(folder));
        QFile srt(folder + "/line.srt");
        QVERIFY(srt.open(QIODevice::WriteOnly));
        srt.write("1\n00:00:00,000 --> 00:00:09,000\nHello\n");
        srt.close();
        const QString labelled = folder + "/labelled.mkv", plain = folder + "/plain.mp4";
        QProcess ffmpeg;
        ffmpeg.start("ffmpeg", {"-v", "error", "-y", "-f", "lavfi", "-i", "testsrc2=size=640x360:rate=30:duration=10",
                                "-f", "lavfi", "-i", "sine=frequency=300:duration=10", "-f", "lavfi", "-i", "sine=frequency=500:duration=10",
                                "-i", srt.fileName(), "-i", srt.fileName(),
                                "-map", "0", "-map", "1", "-map", "2", "-map", "3", "-map", "4", "-c:v", "libx264", "-c:a", "aac", "-c:s", "srt",
                                "-metadata:s:a:0", "title=Commentary", "-metadata:s:a:0", "language=eng", "-metadata:s:a:1", "language=tur",
                                "-metadata:s:s:0", "title=Signs", "-metadata:s:s:0", "language=eng", "-metadata:s:s:1", "language=jpn", labelled});
        QVERIFY2(ffmpeg.waitForFinished(60000) && ffmpeg.exitCode() == 0, ffmpeg.readAllStandardError().constData());
        ffmpeg.start("ffmpeg", {"-v", "error", "-y", "-i", labelled, "-map", "0:v", "-map", "0:a", "-c", "copy", "-map_metadata:s:a", "-1", plain});
        QVERIFY2(ffmpeg.waitForFinished(60000) && ffmpeg.exitCode() == 0, ffmpeg.readAllStandardError().constData());
        {
            Launch app(QDir(m_path).filePath("labels"), labelled, QDir(m_path).filePath("labels.log"));
            QVERIFY2(app.playing(), "the clip did not start playing");
            app.services.i18n.setLanguage("en");
            app.wake();
            QTRY_VERIFY(app.item("mixerChip") && app.item("mixerChip")->isVisible());
            QVERIFY(app.click("mixerChip"));
            QTRY_VERIFY(app.item("mixerRow-2"));
            QCOMPARE(app.item("mixerRow-1")->property("name").toString(), QString("Commentary"));
            QCOMPARE(app.item("mixerRow-2")->property("name").toString(), QString("Türkçe"));
            QVERIFY(Launch::searchVisible(app.item("mixerRow-1"), "mixerRow-1"));
            app.services.mixer.setAllEnabled(true);
            QTRY_COMPARE(app.item("mixerChipText")->property("text").toString(), QString("Commentary + Türkçe"));
            app.shot("labels-mixer");
            app.key(Qt::Key_Escape);
            QTRY_VERIFY(!app.item("mixerPopover"));
            app.wake();
            QVERIFY(app.click("subtitlesButton"));
            QTRY_VERIFY(app.item("subtitleRow-2"));
            QCOMPARE(app.item("subtitleRow-1")->property("modelData").toMap().value("label").toString(), QString("Signs · English"));
            QCOMPARE(app.item("subtitleRow-2")->property("modelData").toMap().value("label").toString(), QString("日本語"));
            app.shot("labels-subtitles");
        }
        {
            Launch app(QDir(m_path).filePath("labels-plain"), plain, QDir(m_path).filePath("labels-plain.log"));
            QVERIFY2(app.playing(), "the clip did not start playing");
            app.services.i18n.setLanguage("en");
            app.services.mixer.setAllEnabled(true);
            app.wake();
            QTRY_COMPARE(app.item("mixerChipText")->property("text").toString(), QString("Track 1 + 2"));
        }
        // ECHOPLAY_NVIDIA_CLIP=<an NVIDIA App recording>: the names it keeps where mpv does not look (Mp4TrackNames).
        const QString nvidia = qEnvironmentVariable("ECHOPLAY_NVIDIA_CLIP");
        if (!nvidia.isEmpty()) {
            Launch app(QDir(m_path).filePath("labels-nvidia"), nvidia, QDir(m_path).filePath("labels-nvidia.log"));
            QVERIFY2(app.playing(), "the clip did not start playing");
            app.services.i18n.setLanguage("en");
            app.services.mixer.setAllEnabled(true);
            app.wake();
            QTRY_COMPARE(app.item("mixerChipText")->property("text").toString(), QString("System + Mic"));
            QVERIFY(app.click("mixerChip"));
            QTRY_VERIFY(app.item("mixerRow-2"));
            QCOMPARE(app.item("mixerRow-1")->property("name").toString(), QString("System sounds"));
            QCOMPARE(app.item("mixerRow-2")->property("name").toString(), QString("Microphone"));
            app.shot("labels-nvidia");
        }
    }

    void chapters()
    {
        const QString state = QDir(m_path).filePath("chapters");
        const QString folder = QDir(m_path).filePath("chapters-clip");
        QVERIFY(QDir().mkpath(state));
        QVERIFY(QDir().mkpath(folder));
        const QString meta = folder + "/chapters.txt"; // ffmpeg's metadata file: four parts of 15 s
        QFile file(meta);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(";FFMETADATA1\n");
        const QStringList titles{"Warm-up", "Round one", "Clutch", "Outro"};
        for (int i = 0; i < titles.size(); ++i)
            file.write(QString("[CHAPTER]\nTIMEBASE=1/1000\nSTART=%1\nEND=%2\ntitle=%3\n").arg(i * 15000).arg((i + 1) * 15000).arg(titles[i]).toUtf8());
        file.close();
        const QString clip = folder + "/chapters.mp4";
        QProcess ffmpeg;
        ffmpeg.start("ffmpeg", {"-v", "error", "-y", "-i", m_clip, "-i", meta, "-map", "0", "-map_chapters", "1", "-c", "copy", clip});
        QVERIFY2(ffmpeg.waitForFinished(30000) && ffmpeg.exitCode() == 0, ffmpeg.readAllStandardError().constData());

        Launch app(state, clip, QDir(m_path).filePath("chapters.log"));
        QVERIFY(app.mpv);
        for (const char *name : {"chapter", "chapter-list", "track-list"})
            app.mpv->observeProperty(name);
        QVERIFY2(app.playing(), "the clip did not start playing");
        QTRY_COMPARE(app.mpv->observed("chapter-list").toList().size(), 4);
        // MP4's chapter text track is no subtitle track.
        for (const QVariant &track : app.mpv->observed("track-list").toList())
            QVERIFY(track.toMap().value("type").toString() != "sub");
        app.wake();
        QTRY_VERIFY(app.item("controlBar")->isVisible());
        QQuickItem *seekBar = app.item("seekBar");
        QVERIFY(seekBar);
        QVERIFY(!Launch::searchVisible(seekBar, "chapterMark-0")); // the start is no gap
        for (int i = 1; i < 4; ++i) {
            QQuickItem *mark = Launch::searchVisible(seekBar, qPrintable(QString("chapterMark-%1").arg(i)));
            QVERIFY2(mark, qPrintable(QString("chapterMark-%1").arg(i)));
            QVERIFY(std::abs(mark->x() + 1 - seekBar->width() * i / 4) < 2);
        }
        // The pointer over the third part: its title next to the time.
        const QPoint onClutch = seekBar->mapToScene(QPointF(seekBar->width() * 0.6, seekBar->height() / 2)).toPoint();
        QTest::mouseMove(app.window, onClutch - QPoint(3, 0));
        QTest::mouseMove(app.window, onClutch);
        QTRY_COMPARE(app.item("seekChapter")->property("text").toString(), QString("Clutch"));
        QTest::qWait(800); // the preview picture
        app.shot("chapters");
        QTest::mouseMove(app.window, QPoint(app.window->width() / 2, app.window->height() / 2));
        // mpv's keys: PgUp next, PgDown back (within 5 s of a start: the part before).
        const auto toast = [&] { return app.item("toastText")->property("text").toString(); };
        app.key(Qt::Key_PageUp);
        QTRY_COMPARE(app.number("chapter"), 1.0);
        QTRY_COMPARE(toast(), QString("Round one"));
        QTRY_VERIFY(app.number("time-pos") >= 14.9);
        app.key(Qt::Key_PageUp);
        QTRY_COMPARE(app.number("chapter"), 2.0);
        QTRY_COMPARE(toast(), QString("Clutch"));
        app.key(Qt::Key_PageDown);
        QTRY_COMPARE(app.number("chapter"), 1.0);
        QTRY_COMPARE(toast(), QString("Round one"));
        QTRY_VERIFY(app.number("time-pos") < 20);
    }

    // Copy frame and the taskbar's thumbnail buttons. The frame is captured the way Copy frame takes it, without the
    // clipboard: that belongs to whoever sits at the PC. The buttons are clicked by the message the taskbar posts for them
    // (posted: Qt hands a WM_COMMAND sent straight to the window procedure to no native event filter).
    void copyFrameAndTaskbar()
    {
        const QString state = QDir(m_path).filePath("windows");
        QVERIFY(QDir().mkpath(state));
        Launch app(state, m_clip, QDir(m_path).filePath("windows.log"));
        QVERIFY(app.mpv);
        QVERIFY2(app.playing(), "the clip did not start playing");
        QSignalSpy captured(app.mpv, &MpvItem::frameCaptured);
        app.mpv->captureFrame();
        QVERIFY(captured.wait(5000));
        const QImage frame = captured.first().first().value<QImage>();
        if (const QString dir = qEnvironmentVariable("ECHOPLAY_E2E_SHOTS"); !dir.isEmpty()) frame.save(QDir(dir).filePath("copied-frame.png"));
        QCOMPARE(frame.size(), QSize(1280, 720));
        QVERIFY(frame.pixelColor(20, 20) != frame.pixelColor(1260, 700)); // a picture, not one color
        QTRY_VERIFY(app.item("controlBar"));
        QTest::mouseClick(app.window, Qt::RightButton, Qt::NoModifier, QPoint(app.window->width() / 2, app.window->height() / 2));
        QTRY_VERIFY(app.item("contextMenu"));
        QVERIFY(Launch::searchVisible(app.item("contextMenu"), "menuCopyFrame"));
        // Opened in the middle, it fits neither below nor above: as low as it fits, not under the title bar.
        QTRY_VERIFY(app.item("contextMenu")->mapToScene(QPointF(0, 0)).y() >= app.item("titleBar")->height());
        app.shot("menu-copy-frame");
        app.key(Qt::Key_Escape);
        QTRY_VERIFY(!app.item("contextMenu"));
#ifdef Q_OS_WIN
        QObject *taskbar = app.window->findChild<QObject *>("taskbarButtons");
        QVERIFY(taskbar);
        QTRY_VERIFY_WITH_TIMEOUT(taskbar->property("ready").toBool(), 10000);
        QVERIFY(!taskbar->property("hasPrevious").toBool()); // one clip in its folder
        QVERIFY(!taskbar->property("hasNext").toBool());
        QCOMPARE(taskbar->property("tips").toStringList().value(1), app.services.i18n.strings().value("pause").toString());
        const HWND hwnd = HWND(app.window->winId());
        QSignalSpy clicks(taskbar, SIGNAL(clicked(QString)));
        PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(1, THBN_CLICKED), 0);
        QTRY_COMPARE(clicks.count(), 1);
        QCOMPARE(clicks.first().first().toString(), QString("playPause"));
        QTRY_COMPARE(app.prop("pause"), QString("yes"));
        QTRY_VERIFY(taskbar->property("paused").toBool());
        QCOMPARE(taskbar->property("tips").toStringList().value(1), app.services.i18n.strings().value("play").toString());
        PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(1, THBN_CLICKED), 0);
        QTRY_COMPARE(app.prop("pause"), QString("no"));
#endif
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

        // The smallest window, as thin as a snap layout's slot: the bar and the popovers still fit.
        app.window->resize(app.window->minimumSize());
        QTRY_COMPARE(app.window->width(), 500);
        app.wake();
        QTRY_VERIFY(bar->isVisible());
        for (const char *name : {"playButton", "timeDisplay", "mixerChip", "fullscreenButton"}) {
            QQuickItem *item = Launch::searchVisible(bar, name);
            QVERIFY2(item, name);
            const QRectF box = item->mapRectToItem(bar, QRectF(0, 0, item->width(), item->height()));
            QVERIFY2(box.left() >= 0 && box.right() <= bar->width(), name);
        }
        app.shot("minimum");
        app.wake();
        QTRY_VERIFY(bar->isVisible());
        QVERIFY(app.click("mixerChip"));
        QTRY_VERIFY(app.item("mixerPopover"));
        app.shot("minimum-mixer");
        QVERIFY(app.click("gearButton"));
        QTRY_VERIFY(app.item("quickPopover"));
        app.shot("minimum-quick");
        app.key(Qt::Key_Escape);
    }

#ifdef Q_OS_WIN
    // The frameless window as Windows sees it. Messages go to this window only, so no real input is needed.
    void windowFrame()
    {
        // The snap layouts flyout wants Windows 10/11 support claimed in the manifest, in EchoPlay.exe and in this test.
        for (const QString &exe : {QCoreApplication::applicationDirPath() + "/EchoPlay.exe", QCoreApplication::applicationFilePath()}) {
            QFile file(exe);
            QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(exe));
            QVERIFY2(file.readAll().contains("{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"), qPrintable(exe));
        }
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
        QVERIFY2(limits.ptMinTrackSize.x >= qRound(500 * ratio) && limits.ptMinTrackSize.y >= qRound(360 * ratio), "minimum size is not enforced");
        app.window->close();
    }
#endif

#ifdef Q_OS_WIN
    // The real mouse and keyboard, for what only Windows does with the frame. tools/window-live.sh only: it takes them
    // from whoever sits at the PC. The input comes from another thread: Windows' move and resize loops run inside the
    // button press, on this thread, until the button is up. Every check runs and reports, so one run tells everything.
    // Pictures for the release page (tools/promo.sh): ECHOPLAY_PROMO_CLIP = a recording with game and mic tracks (the
    // mixer animation, as numbered frames), ECHOPLAY_PROMO_FILM = any film (stills of the panels over it).
    void promo()
    {
        const QString clip = qEnvironmentVariable("ECHOPLAY_PROMO_CLIP"), film = qEnvironmentVariable("ECHOPLAY_PROMO_FILM");
        const QString out = qEnvironmentVariable("ECHOPLAY_E2E_SHOTS");
        if (clip.isEmpty() || film.isEmpty() || out.isEmpty()) QSKIP("tools/promo.sh");
        {
            Launch app(QDir(m_path).filePath("promo-mix"), clip, QDir(m_path).filePath("promo-mix.log"));
            QVERIFY(app.playing());
            app.mpv->setProperty("loop-file", "inf");
            app.mpv->setProperty("mute", false); // the bar would show the muted icon
            QDir().mkpath(out + "/frames");
            QList<QImage> frames; // kept in memory: a PNG written per frame took ~0.5 s each
            QElapsedTimer clock;
            QTimer grab;
            QObject::connect(&grab, &QTimer::timeout, [&] {
                frames << app.window->grabWindow().scaledToWidth(960, Qt::SmoothTransformation);
            });
            const auto toggle = [&](int track) {
                QQuickItem *row = app.item(qPrintable(QString("mixerRow-%1").arg(track)));
                QQuickItem *on = row ? Launch::search(row, "trackSwitch") : nullptr;
                if (on) QTest::mouseClick(app.window, Qt::LeftButton, Qt::NoModifier,
                                          on->mapToScene(QPointF(on->width() / 2, on->height() / 2)).toPoint());
            };
            app.wake();
            clock.start();
            grab.start(66);
            QTest::qWait(1200);
            QVERIFY(app.click("mixerChip"));
            QTRY_VERIFY(app.item("mixerRow-1"));
            QTest::qWait(1300); // the game alone (a new config starts on track 1)
            toggle(2); // the mic joins: both mixed
            QTest::qWait(1700);
            toggle(1); // the game off: the mic alone
            QTest::qWait(1700);
            toggle(1); // both again
            QTest::qWait(1500);
            grab.stop();
            qInfo("promo: %lld frames in %lld ms", qint64(frames.size()), clock.elapsed());
            for (int i = 0; i < frames.size(); ++i) frames[i].save(QString("%1/frames/%2.png").arg(out).arg(i, 4, 10, QChar('0')));
            app.shot("promo-mixer");
            app.key(Qt::Key_Escape);
        }
        {
            Launch app(QDir(m_path).filePath("promo-film"), film, QDir(m_path).filePath("promo-film.log"));
            QVERIFY(app.playing());
            app.mpv->setProperty("mute", false);
            app.mpv->command(QVariantList{"seek", 36, "absolute"});
            QTest::qWait(800);
            app.mpv->setProperty("pause", true);
            app.wake();
            QVERIFY(app.click("gearButton"));
            QTRY_VERIFY(app.item("quickPopover"));
            app.shot("promo-quick");
            app.key(Qt::Key_Escape);
            QVERIFY(QMetaObject::invokeMethod(app.item("controlBar")->parentItem(), "openSettings"));
            QTRY_VERIFY(app.click("settingsTab-appearance"));
            app.shot("promo-settings");
            app.key(Qt::Key_Escape);
            QTRY_VERIFY(!app.item("settingsOverlay"));
            app.wake();
            app.shot("promo-player");
        }
        {
            Launch app(QDir(m_path).filePath("promo-start"), QString(), QDir(m_path).filePath("promo-start.log"));
            QTRY_VERIFY(app.item("startScreen"));
            app.shot("promo-start");
        }
    }

    void realWindowInput()
    {
        if (!qEnvironmentVariableIsSet("ECHOPLAY_E2E_REAL_INPUT")) QSKIP("takes the real mouse and keyboard: tools/window-live.sh");
        const QString state = QDir(m_path).filePath("live");
        QVERIFY(QDir().mkpath(state));
        Launch app(state, m_clip, QDir(m_path).filePath("live.log"));
        QVERIFY(app.mpv);
        QVERIFY2(app.playing(), "the clip did not start playing");
        QTRY_VERIFY(app.item("titleBar"));
        const HWND hwnd = HWND(app.window->winId());
        const qreal ratio = app.window->devicePixelRatio();
        POINT pointerBefore;
        GetCursorPos(&pointerBefore);
        const auto leaveInputClean = qScopeGuard([&] {
            if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            SetCursorPos(pointerBefore.x, pointerBefore.y);
        });

        const auto rect = [hwnd] {
            RECT r;
            GetWindowRect(hwnd, &r);
            return QRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
        };
        const auto work = [hwnd] {
            MONITORINFO info{};
            info.cbSize = sizeof(info);
            GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &info);
            return QRect(info.rcWork.left, info.rcWork.top, info.rcWork.right - info.rcWork.left, info.rcWork.bottom - info.rcWork.top);
        };
        const auto text = [](QRect r) { return QString("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height()); };
        QStringList failures;
        const auto check = [&](const char *what, const std::function<bool()> &ok) {
            const bool passed = QTest::qWaitFor(ok, 2500);
            qInfo("%s %s (window %s, work area %s)", passed ? "ok  " : "FAIL", what, qPrintable(text(rect())), qPrintable(text(work())));
            if (!passed) failures << what;
        };

        // What the other thread does. This thread stays in the event loop, also when a move loop runs inside it.
        const auto fromOutside = [](const std::function<void()> &script) {
            std::atomic<bool> done{false};
            std::thread worker([&] {
                script();
                done = true;
            });
            while (!done) QTest::qWait(10);
            worker.join();
        };
        const auto moveTo = [](QPoint p) {
            SetCursorPos(p.x(), p.y());
            QThread::msleep(40);
        };
        // A move through the input stack, as a real mouse sends it: what Windows' own hover flyouts may wait for.
        const auto glideTo = [](QPoint p) {
            const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN), vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
            const int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN), vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
            INPUT input{};
            input.type = INPUT_MOUSE;
            input.mi.dx = LONG((p.x() - vx) * 65535LL / (vw - 1));
            input.mi.dy = LONG((p.y() - vy) * 65535LL / (vh - 1));
            input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
            SendInput(1, &input, sizeof(input));
            QThread::msleep(40);
        };
        const auto button = [](DWORD flag) {
            INPUT input{};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = flag;
            SendInput(1, &input, sizeof(input));
            QThread::msleep(40);
        };
        const auto drag = [&](QPoint from, QPoint to) {
            fromOutside([&] {
                moveTo(from);
                button(MOUSEEVENTF_LEFTDOWN);
                for (int i = 1; i <= 8; ++i) moveTo(from + (to - from) * i / 8);
                QThread::msleep(250); // Aero Snap waits a moment at the screen's edge
                button(MOUSEEVENTF_LEFTUP);
            });
            QTest::qWait(350);
        };
        const auto click = [&](QPoint at, int times = 1) {
            fromOutside([&] {
                moveTo(at);
                for (int i = 0; i < times; ++i) {
                    button(MOUSEEVENTF_LEFTDOWN);
                    button(MOUSEEVENTF_LEFTUP);
                }
            });
            QTest::qWait(300);
        };
        const auto keys = [&](std::initializer_list<WORD> codes) {
            fromOutside([&] {
                for (WORD code : codes) keybd_event(BYTE(code), 0, 0, 0);
                QThread::msleep(40);
                for (auto it = std::rbegin(codes); it != std::rend(codes); ++it) keybd_event(BYTE(*it), 0, KEYEVENTF_KEYUP, 0);
            });
            QTest::qWait(500);
        };
        const auto caption = [&] { return QPoint(rect().center().x() - 100, rect().top() + 18); };

        click(caption()); // brings the window forward: Win+Left needs that

        // Sliders held down follow the real pointer anywhere: dragged far past the end, off the bar and off the window,
        // the volume is at its maximum; dragged far left, at 0. Then a mixer gain the same way.
        {
            const auto onScreen = [&](QQuickItem *item, qreal fx) {
                const QPointF scene = item->mapToScene(QPointF(item->width() * fx, item->height() / 2));
                return rect().topLeft() + (scene * ratio).toPoint();
            };
            const auto realDrag = [&](QPoint from, std::initializer_list<QPoint> path) {
                fromOutside([&] {
                    glideTo(from);
                    button(MOUSEEVENTF_LEFTDOWN);
                    QPoint last = from;
                    for (QPoint to : path) {
                        for (int i = 1; i <= 6; ++i) glideTo(last + (to - last) * i / 6);
                        last = to;
                    }
                    QThread::msleep(300);
                    button(MOUSEEVENTF_LEFTUP);
                });
                QTest::qWait(400);
            };
            app.mpv->setProperty("volume", 50);
            QTRY_COMPARE(app.number("volume"), 50.0);
            glideTo(rect().center());
            QTRY_VERIFY(app.item("volumeSlider") && app.item("volumeSlider")->isVisible());
            const QPoint volume = onScreen(app.item("volumeSlider"), 0.3);
            realDrag(volume, {volume + QPoint(150, -60), volume + QPoint(rect().width(), -300)});
            check("volume dragged far right, off the window: maximum", [&] { return app.number("volume") == app.services.player.volumeMax(); });
            qInfo("volume after the drag right: %g", app.number("volume"));
            realDrag(onScreen(app.item("volumeSlider"), 0.5), {volume + QPoint(-600, -120)});
            check("volume dragged far left: 0", [&] { return app.number("volume") == 0; });
            qInfo("volume after the drag left: %g", app.number("volume"));
            app.mpv->setProperty("volume", 100);

            QVERIFY(app.click("mixerChip"));
            QTRY_VERIFY(app.item("mixerRow-1"));
            QTest::qWait(400);
            QQuickItem *gain = Launch::search(app.item("mixerRow-1"), "trackGain");
            QVERIFY(gain);
            const QPoint at = onScreen(gain, 0.2);
            realDrag(at, {at + QPoint(200, 80), at + QPoint(900, 400)});
            check("mixer gain dragged far right, off the popover: maximum",
                  [&] { return app.item("mixerRow-1") && app.item("mixerRow-1")->property("shownGain").toDouble() == app.services.mixer.gainMax(); });
            qInfo("gain after the drag: %g", app.item("mixerRow-1") ? app.item("mixerRow-1")->property("shownGain").toDouble() : -1);
            app.services.mixer.setGain(1, 1);
            app.key(Qt::Key_Escape);
            QTRY_VERIFY(!app.item("mixerPopover"));
        }

        // The resize edges. The window follows the pointer; the picture follows the window.
        const QRect start = rect();
        drag(QPoint(start.right(), start.center().y()), QPoint(start.right() - 120, start.center().y()));
        check("right edge resizes", [&] { return qAbs(rect().width() - (start.width() - 120)) <= 4 && rect().left() == start.left(); });
        const QRect narrower = rect();
        drag(QPoint(narrower.right(), narrower.bottom()), QPoint(narrower.right() - 40, narrower.bottom() - 60));
        check("corner resizes", [&] { return qAbs(rect().height() - (narrower.height() - 60)) <= 4; });
        check("the picture follows", [&] { return qRound(app.mpv->width() * ratio) == rect().width(); });
        const QRect smaller = rect();
        drag(QPoint(smaller.left(), smaller.center().y()), QPoint(smaller.left() + 3000, smaller.center().y())); // far past the minimum
        check("the minimum width holds", [&] { return rect().width() >= qRound(500 * ratio) - 1 && rect().width() < smaller.width(); });

        // The caption moves it.
        const QRect before = rect();
        drag(caption(), caption() + QPoint(-30, 40));
        check("the caption moves it", [&] { return rect().topLeft() == before.topLeft() + QPoint(-30, 40) && rect().size() == before.size(); });

        // The picture moves it too, as in every player. The window starts a drag distance behind the pointer, and the
        // drag is not a click: the clip plays on. A click without a move still pauses.
        const QRect beforePicture = rect();
        drag(beforePicture.center(), beforePicture.center() + QPoint(120, -80));
        check("a drag on the picture moves it", [&] {
            const QPoint moved = rect().topLeft() - beforePicture.topLeft();
            return moved.x() >= 80 && moved.x() <= 120 && moved.y() <= -50 && moved.y() >= -80 && rect().size() == beforePicture.size();
        });
        check("that drag does not pause", [&] { return app.prop("pause") == "no"; });
        click(rect().center());
        check("a click on the picture still pauses", [&] { return app.prop("pause") == "yes"; });
        QTest::qWait(700); // not a double-click
        click(rect().center());
        check("and plays again", [&] { return app.prop("pause") == "no"; });

        // A double-click on the caption maximizes, and again restores.
        const QRect normal = rect();
        click(caption(), 2);
        check("double-click maximizes", [&] { return IsZoomed(hwnd) && app.window->visibility() == QWindow::Maximized; });
        click(QPoint(work().center().x() - 100, work().top() + 18), 2);
        check("double-click restores", [&] { return !IsZoomed(hwnd) && rect() == normal; });

        // Windows 11's snap layouts on the maximize button: the hover reaches our button, the flyout is for the eye.
        WindowChrome *chrome = app.window->findChild<WindowChrome *>();
        QVERIFY(chrome);
        const int buttonWidth = qRound(46 * ratio);
        const QPoint maximize(rect().right() - buttonWidth - buttonWidth / 2, rect().top() + 18);
        // The flyout is a window of Windows' own under the button. Settings > System > Multitasking can turn it off.
        const QRect underButton(maximize.x() - qRound(160 * ratio), rect().top() + qRound(32 * ratio), qRound(320 * ratio), qRound(160 * ratio));
        const QHash<quintptr, QString> shownBefore = shownWindows(underButton);
        const auto newIn = [](const QRect &area, const QHash<quintptr, QString> &before) { // windows not there before
            QStringList classes;
            const QHash<quintptr, QString> now = shownWindows(area);
            for (auto it = now.cbegin(); it != now.cend(); ++it)
                if (!before.contains(it.key())) classes << it.value();
            return classes;
        };
        fromOutside([&] {
            for (int i = 4; i >= 0; --i) glideTo(maximize + QPoint(0, 3 * i)); // comes in from below, then rests
        });
        check("the maximize button's hover arrives", [&] { return chrome->maximizeHovered(); });
        QTest::qWait(1500);
        check("the title bar stays up while the pointer rests on it", [&] { return app.item("titleBar")->opacity() == 1; });
        const bool ourFlyout = QTest::qWaitFor([&] { return !newIn(underButton, shownBefore).isEmpty(); }, 2500);
        qInfo("new windows under the button: %s", qPrintable(newIn(underButton, shownBefore).join(", ")));
        app.desktopShot("frame-snap-layouts", false);
        // The same hover on a plain window with Windows' own frame tells whether the flyout opens for this input at all:
        // on this PC (2026-11-08, build 26300) it does not, for SetCursorPos nor SendInput, so it is left to the eye.
        bool plainFlyout = false;
        {
            const QRect r = rect();
            const HWND plain = CreateWindowExW(0, L"STATIC", L"snap probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE, r.left() - 200, r.top() + 60, 600, 400,
                                               nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
            QTest::qWait(500);
            TITLEBARINFOEX bar{};
            bar.cbSize = sizeof(bar);
            SendMessageW(plain, WM_GETTITLEBARINFOEX, 0, LPARAM(&bar));
            const QRect plainMax(bar.rgrect[3].left, bar.rgrect[3].top, bar.rgrect[3].right - bar.rgrect[3].left, bar.rgrect[3].bottom - bar.rgrect[3].top);
            const QRect underPlain(plainMax.center().x() - qRound(160 * ratio), plainMax.bottom(), qRound(320 * ratio), qRound(160 * ratio));
            const QHash<quintptr, QString> plainBefore = shownWindows(underPlain);
            fromOutside([&] {
                for (int i = 4; i >= 0; --i) glideTo(plainMax.center() + QPoint(0, 3 * i));
            });
            plainFlyout = QTest::qWaitFor([&] { return !newIn(underPlain, plainBefore).isEmpty(); }, 2500);
            DestroyWindow(plain);
        }
        const QSettings explorer("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", QSettings::NativeFormat);
        if (ourFlyout || plainFlyout) check("the snap layouts flyout opens (it does on a plain window)", [&] { return ourFlyout; });
        else qInfo("NOT CHECKED: no snap layouts flyout for synthetic input on a plain window either (EnableSnapAssistFlyout = %d)",
                   explorer.value("EnableSnapAssistFlyout", 1).toInt());
        click(maximize + QPoint(1, 0));
        check("a click on the maximize button maximizes", [&] { return IsZoomed(hwnd); });
        click(QPoint(work().right() - buttonWidth - buttonWidth / 2, work().top() + 18));
        check("a click on it restores", [&] { return !IsZoomed(hwnd); });
        fromOutside([&] { moveTo(work().center()); }); // away from the flyout
        QTest::qWait(300);

        // Aero Snap: to the top edge maximizes, Win+Left takes the left half.
        drag(caption(), QPoint(work().center().x(), work().top()));
        check("a drag to the top edge maximizes", [&] { return IsZoomed(hwnd); });
        if (IsZoomed(hwnd)) click(QPoint(work().center().x() - 100, work().top() + 18), 2);
        check("restored for Win+Left", [&] { return !IsZoomed(hwnd); });
        keys({VK_LWIN, VK_LEFT});
        check("Win+Left takes the left half", [&] {
            return qAbs(rect().left() - work().left()) <= 12 && qAbs(rect().width() - work().width() / 2) <= 24 && qAbs(rect().height() - work().height()) <= 24;
        });
        keys({VK_ESCAPE}); // the snap assist Windows offers beside it
        app.desktopShot("frame-snapped-left");

        // The thumbnail's buttons with the real pointer: hover our taskbar button, click Pause on the thumbnail. e2e
        // posts the message the taskbar posts; this shows the taskbar really sends it and Qt hands it to us.
        QObject *taskbar = app.window->findChild<QObject *>("taskbarButtons");
        QVERIFY(taskbar);
        check("the thumbnail buttons are ready", [&] { return taskbar->property("ready").toBool(); });
        const QString pauseTip = taskbar->property("tips").toStringList().value(1);
        const QString title = app.window->title();
        DWORD shell = 0;
        GetWindowThreadProcessId(FindWindowW(L"Shell_TrayWnd", nullptr), &shell);
        FoundButton taskButton, thumbButton;
        QStringList seen;
        fromOutside([&] {
            // Windows 11 names it after the exe ("tst_e2e.exe - 1 running window"); older taskbars use the title.
            const QString exe = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
            const auto ours = [&](const QString &name) { return name.startsWith(exe, Qt::CaseInsensitive) || name == title; };
            taskButton = findButton(shell, {"Shell_TrayWnd", "Shell_SecondaryTrayWnd"}, ours, &seen);
        });
        check("UI Automation finds our taskbar button", [&] { return !taskButton.button.isNull(); });
        if (taskButton.button.isNull()) qInfo("taskbar buttons seen: %s", qPrintable(seen.join(" | ")));
        if (!taskButton.button.isNull()) {
            seen.clear();
            const QRect screen(GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN), GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));
            const QHash<quintptr, QString> anywhereBefore = shownWindows(screen);
            fromOutside([&] {
                for (int i = 4; i >= 0; --i) glideTo(taskButton.button.center() + QPoint(2 * i, 0));
                QThread::msleep(2500); // the thumbnail fades in
                thumbButton = findButton(shell, {}, [&](const QString &name) { return name == pauseTip; }, &seen);
            });
            QStringList appeared;
            const QHash<quintptr, QString> anywhere = shownWindows(screen);
            for (auto it = anywhere.cbegin(); it != anywhere.cend(); ++it)
                if (!anywhereBefore.contains(it.key())) appeared << it.value();
            qInfo("windows that appeared on the hover: %s", qPrintable(appeared.join(", ")));
            // Like the flyout, no thumbnail opened for synthetic hover on this PC (2026-11-08): nothing to click then.
            if (appeared.isEmpty() && thumbButton.button.isNull()) qInfo("NOT CHECKED: no thumbnail opened for synthetic hover");
            else check("UI Automation finds Pause on our thumbnail", [&] { return !thumbButton.button.isNull(); });
            if (thumbButton.button.isNull() && !appeared.isEmpty()) qInfo("explorer buttons seen: %s", qPrintable(seen.mid(0, 60).join(" | ")));
            app.screenShot("taskbar-thumbnail", thumbButton.window.isNull() ? taskButton.button.adjusted(-200, -320, 200, 40) : thumbButton.window);
        }
        if (!thumbButton.button.isNull()) {
            const QPoint from = taskButton.button.center(), to = thumbButton.button.center();
            fromOutside([&] {
                for (int i = 1; i <= 6; ++i) moveTo(from + (to - from) * i / 6);
                button(MOUSEEVENTF_LEFTDOWN);
                button(MOUSEEVENTF_LEFTUP);
            });
            check("a real click on the thumbnail's Pause pauses", [&] { return app.prop("pause") == "yes" && taskbar->property("paused").toBool(); });
            fromOutside([&] {
                button(MOUSEEVENTF_LEFTDOWN);
                button(MOUSEEVENTF_LEFTUP);
            });
            check("a second click plays again", [&] { return app.prop("pause") == "no"; });
        }
        fromOutside([&] { moveTo(work().center()); }); // the thumbnail closes
        app.window->close();
        QVERIFY2(failures.isEmpty(), qPrintable(failures.join(", ")));
    }
#endif

    // The window comes back where it was left, maximized or not. Full screen comes back as what it came from.
    void windowPlaceIsRemembered()
    {
        const QString state = QDir(m_path).filePath("place");
        QVERIFY(QDir().mkpath(state));
        QRect normal;
        {
            Launch app(state, m_clip, QDir(m_path).filePath("place1.log"));
            QVERIFY(app.mpv);
            QVERIFY2(app.playing(), "the clip did not start playing");
            app.window->setGeometry(70, 90, 900, 520);
            QTRY_COMPARE(app.window->width(), 900);
            normal = app.window->geometry();
            app.window->findChild<WindowChrome *>()->toggleMaximized();
            QTRY_COMPARE(app.window->visibility(), QWindow::Maximized);
            app.window->close();
        }
        {
            Launch app(state, m_clip, QDir(m_path).filePath("place2.log"));
            QVERIFY(app.mpv);
            QVERIFY2(app.playing(), "the clip did not start playing");
            QTRY_COMPARE(app.window->visibility(), QWindow::Maximized);
            app.window->findChild<WindowChrome *>()->toggleMaximized();
            QTRY_COMPARE(app.window->geometry(), normal);
            app.key(Qt::Key_F);
            QTRY_COMPARE(app.window->visibility(), QWindow::FullScreen);
            app.window->close();
        }
        {
            Launch app(state, m_clip, QDir(m_path).filePath("place3.log"));
            QVERIFY(app.mpv);
            QVERIFY2(app.playing(), "the clip did not start playing");
            QCOMPARE(app.window->visibility(), QWindow::Windowed);
            QCOMPARE(app.window->geometry(), normal);

            // A clip opened on purpose plays, even if the last one was paused.
            app.key(Qt::Key_Space);
            QTRY_COMPARE(app.prop("pause"), QString("yes"));
            QVERIFY(QMetaObject::invokeMethod(app.window, "openPath", Q_ARG(QVariant, m_clip)));
            QTRY_COMPARE(app.prop("pause"), QString("no"));
        }
    }

    void secondRunRestoresMixerAndVolume()
    {
        Launch app(m_path, m_clip, QDir(m_path).filePath("run2.log"));
        QVERIFY(app.mpv);
        QVERIFY2(app.playing(), "the clip did not start playing");
#ifdef Q_OS_WIN
        const auto onTop = [&] { return (GetWindowLongPtrW(HWND(app.window->winId()), GWL_EXSTYLE) & WS_EX_TOPMOST) != 0; };
        QVERIFY(onTop()); // the first run left it on
        app.key(Qt::Key_T);
        QTRY_VERIFY(!onTop());
#endif
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
