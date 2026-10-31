#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDateTime>
#include <QFileInfo>
#include <QQuickWindow>
#include <QStandardPaths>
#include <clocale>
#include <cstdio>
#include <memory>
#include <mpv/client.h>
#include "AppState.h"
#include "I18n.h"
#include "MpvItem.h"
#include "SingleInstance.h"
#include "Theme.h"

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

int main(int argc, char *argv[])
{
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
    cli.addOptions({bench, noGlass, mix, logFile, smokeOpt, mixToggle, shotAt, shotOut});
    cli.addHelpOption();
    cli.process(app);
    if (cli.isSet(smokeOpt)) return smoke();

    // One window per user: a second launch hands its file to the running one and exits. Bench and shot runs
    // stay independent (tools/bench.sh starts many in a row).
    std::unique_ptr<SingleInstance> instance;
    if (!cli.isSet(bench) && cli.value(shotAt).toDouble() < 0) {
        instance = std::make_unique<SingleInstance>(SingleInstance::defaultName());
        const qint64 age = MpvItem::processAgeMsNow(); // the hand-over time counts from this process's creation
        const qint64 created = QDateTime::currentMSecsSinceEpoch() - (age < 0 ? 0 : age);
        // Absolute: the running instance resolves paths against its own working folder. URLs pass unchanged.
        const QString file = cli.positionalArguments().value(0);
        const QString sent = QFileInfo::exists(file) ? QFileInfo(file).absoluteFilePath() : file;
        if (instance->sendToRunning(sent, created)) return 0;
        instance->listen();
    }

    // Declared before the engine so they outlive it. QML reaches them through the singletons' create().
    AppState state(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/state.json");
    AppState::setInstance(&state);
    I18n i18n(QCoreApplication::applicationDirPath() + "/lang");
    i18n.setLanguage(state.language()); // empty: the system language, and nothing is saved until the user picks one
    I18n::setInstance(&i18n);
    QObject::connect(&i18n, &I18n::languageChanged, &state, [&] { state.setLanguage(i18n.language()); });
    Theme theme(":/theme/theme.json", QCoreApplication::applicationDirPath() + "/theme.json");
    Theme::setInstance(&theme);

    QQmlApplicationEngine engine;
    const qint64 tEngine = MpvItem::processAgeMsNow();
    engine.setInitialProperties({
        {"file", cli.positionalArguments().value(0)},
        {"benchSeconds", cli.value(bench).toInt()},
        {"glass", !cli.isSet(noGlass)},
        {"mix", cli.isSet(mix)},
        {"logFile", cli.value(logFile)},
        {"mixToggle", cli.isSet(mixToggle)},
        {"shotAt", cli.value(shotAt).toDouble()},
        {"shotOut", cli.value(shotOut)},
        {"startup", QVariantMap{{"main_ms", tMain}, {"app_ms", tApp}, {"engine_ms", tEngine}}},
        {"instance", QVariant::fromValue<QObject *>(instance.get())},
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("EchoPlay", "Main");
    return app.exec();
}
