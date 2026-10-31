#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <clocale>
#include <cstdio>
#include <mpv/client.h>
#include "MpvItem.h"

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
    // The libmpv render API draws with OpenGL; Qt on Windows would pick Direct3D 11 otherwise.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QGuiApplication app(argc, argv);
    const qint64 tApp = MpvItem::processAgeMsNow();
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

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {"file", cli.positionalArguments().value(0)},
        {"benchSeconds", cli.value(bench).toInt()},
        {"glass", !cli.isSet(noGlass)},
        {"mix", cli.isSet(mix)},
        {"logFile", cli.value(logFile)},
        {"mixToggle", cli.isSet(mixToggle)},
        {"shotAt", cli.value(shotAt).toDouble()},
        {"shotOut", cli.value(shotOut)},
        {"startup", QVariantMap{{"main_ms", tMain}, {"app_ms", tApp}}},
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("EchoPlay", "Main");
    return app.exec();
}
