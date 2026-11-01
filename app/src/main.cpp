#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDateTime>
#include <QFileInfo>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <clocale>
#include <cstdio>
#include <memory>
#include <mpv/client.h>
#include <QtQml/qqmlextensionplugin.h>
#include <algorithm>
#include <cstring>
#include "MpvItem.h"
#include "Services.h"
#include "SetupLock.h"
#include "SingleInstance.h"

Q_IMPORT_QML_PLUGIN(EchoPlayPlugin) // the player's QML module is a static library (app/CMakeLists.txt)

// --smoke: libmpv loads and initializes, no window. Used by ctest.
static int smoke()
{
    mpv_handle *mpv = mpv_create();
    if (!mpv) return 1;
    mpv_set_option_string(mpv, "vo", "null");
    if (mpv_initialize(mpv) < 0) return 2;
    char *version = mpv_get_property_string(mpv, "mpv-version");
    std::printf("%s (client API %lu.%lu)\n", version ? version : "?", mpv_client_api_version() >> 16,
                mpv_client_api_version() & 0xffff);
    mpv_free(version);
    mpv_terminate_destroy(mpv);
    return 0;
}

// Started last of all, when everything has saved its state and let go of mpv: an update installed on close.
struct SetupOnClose
{
    QString path;
    ~SetupOnClose()
    {
        if (!path.isEmpty()) SetupLock::startSetup(path, false);
    }
};

int main(int argc, char *argv[])
{
    // --after-setup: the setup starting EchoPlay when the files are in place but its lock is still there, or a launch
    // coming back from comeBackLater.
    const bool afterSetup = std::any_of(argv + 1, argv + argc, [](const char *a) { return !strcmp(a, "--after-setup"); });
    if (!afterSetup && SetupLock::isHeld() && SetupLock::comeBackLater()) return 0;

    SetupOnClose setupOnClose;
    const qint64 tMain = MpvItem::processAgeMsNow();
    // MpvItem renders through libmpv's D3D11 render API on Qt's device. OpenGL was ~750 ms slower to start:
    // the NVIDIA OpenGL driver init alone took that long before the first frame (measured 2026-10-31).
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QGuiApplication app(argc, argv);
    const qint64 tApp = MpvItem::processAgeMsNow();
    QCoreApplication::setApplicationName("EchoPlay"); // config folder: %LOCALAPPDATA%/EchoPlay on Windows
    std::setlocale(LC_NUMERIC, "C"); // libmpv requires it

    QCommandLineParser cli;
    cli.addPositionalArgument("file", "Video to play.");
    QCommandLineOption bench("bench", "Log a BENCH line after <seconds>, then quit.", "seconds", "0");
    QCommandLineOption noGlass("no-glass", "Hide the glass panels.");
    QCommandLineOption mix("mix", "Mix audio tracks 1 and 2 through lavfi-complex.");
    QCommandLineOption logFile("log-file", "mpv log file.", "path");
    QCommandLineOption smokeOpt("smoke", "Initialize libmpv without a window and exit.");
    QCommandLineOption mixToggle("mix-toggle", "Switch between the mix and track 1 alone every 2 s while playing.");
    QCommandLineOption shotAt("shot-at", "Pause at <seconds>, save a window screenshot to --shot-out, quit.", "seconds", "-1");
    QCommandLineOption shotOut("shot-out", "Screenshot path for --shot-at.", "path");
    QCommandLineOption thumbScrub("thumb-scrub", "Request a seek-bar preview every 50 ms while playing, like a pointer on the seek bar.");
    QCommandLineOption tierCycle("tier-cycle", "Switch the picture tier every 2 s while playing (Ultra, Low, Normal, ...).");
    QCommandLineOption afterSetupOpt("after-setup", "The setup's lock does not apply (installer/windows/v2/EchoPlay.iss, SetupLock.cpp).");
    QCommandLineOption setupOnCloseOpt("setup-on-close", "Start this setup after closing, as an update installed on close does (tools/setup-live.sh).", "path");
    cli.addOptions({bench, noGlass, mix, logFile, smokeOpt, mixToggle, shotAt, shotOut, thumbScrub, tierCycle, afterSetupOpt, setupOnCloseOpt});
    cli.addHelpOption();
    cli.process(app);
    setupOnClose.path = cli.value(setupOnCloseOpt); // with --smoke: the whole close path, no window
    if (cli.isSet(smokeOpt)) return smoke();
    // Absolute: a running instance resolves paths against its own working folder, and resume positions are
    // keyed by path. URLs pass unchanged.
    const QString arg = cli.positionalArguments().value(0);
    const QString file = QFileInfo::exists(arg) ? QFileInfo(arg).absoluteFilePath() : arg;

    // One window per user: a second launch hands its file to the running one and exits. Bench and shot runs
    // stay independent (tools/bench.sh starts many in a row).
    std::unique_ptr<SingleInstance> instance;
    if (!cli.isSet(bench) && cli.value(shotAt).toDouble() < 0) {
        instance = std::make_unique<SingleInstance>(SingleInstance::defaultName());
        const qint64 age = MpvItem::processAgeMsNow(); // the hand-over time counts from this process's creation
        const qint64 created = QDateTime::currentMSecsSinceEpoch() - (age < 0 ? 0 : age);
        if (instance->sendToRunning(file, created)) return 0;
        instance->listen();
    }

    // Declared before the engine so it outlives it. QML reaches it through the singletons' create().
    Services services(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation),
                      QCoreApplication::applicationDirPath(), ECHOPLAY_VERSION);
    // 4 s after launch, so the TLS start-up never competes with the first frame. Bench and shot runs stay
    // offline (comparable measurements).
    if (!cli.isSet(bench) && cli.value(shotAt).toDouble() < 0)
        QTimer::singleShot(4000, &services.updates, &UpdateCheck::start);

    QQmlApplicationEngine engine;
    const qint64 tEngine = MpvItem::processAgeMsNow();
    engine.setInitialProperties({
        {"file", file},
        {"benchSeconds", cli.value(bench).toInt()},
        {"glass", !cli.isSet(noGlass)},
        {"mix", cli.isSet(mix)},
        {"logFile", cli.value(logFile)},
        {"mixToggle", cli.isSet(mixToggle)},
        {"thumbScrub", cli.isSet(thumbScrub)},
        {"tierCycle", cli.isSet(tierCycle)},
        {"shotAt", cli.value(shotAt).toDouble()},
        {"shotOut", cli.value(shotOut)},
        {"startup", QVariantMap{{"main_ms", tMain}, {"app_ms", tApp}, {"engine_ms", tEngine}}},
        {"instance", QVariant::fromValue<QObject *>(instance.get())},
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("EchoPlay", "Main");
    const int code = app.exec();
    if (setupOnClose.path.isEmpty()) setupOnClose.path = services.updater.setupOnClose();
    return code;
}
