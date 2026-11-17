#include "Services.h"

#include <QGuiApplication>
#include <QPalette>
#include <QStandardPaths>
#include "SetupLock.h"

Services::Services(const QString &configDir, const QString &appDir, const QString &version)
    : state(configDir + "/state.json"),
      resume(configDir + "/resume.json"), // its own file: it grows with every file watched
      i18n(appDir + "/lang"),
      theme(&state, ":/theme/theme.json", appDir + "/theme.json", configDir + "/fonts"),
      mixer(&state),
      quality(&state),
      player(&state),
      updates(&state, version),
      // The setup is a per-user installer: silent, no restart prompt. Install now reopens EchoPlay when it is done;
      // install on close does not (main.cpp).
      updater(QStandardPaths::writableLocation(QStandardPaths::TempLocation),
              [](const QString &setupPath) { return SetupLock::startSetup(setupPath, true); }),
      thumbnails(nullptr),
      clipInfo(nullptr),
      silence(nullptr),
      cuts(nullptr)
{
    i18n.setLanguage(state.language()); // empty: the system language, and nothing is saved until the user picks one
    QObject::connect(&i18n, &I18n::languageChanged, &state, [this] { state.setLanguage(i18n.language()); });
    theme.startGui();
    // Links in Markdown take the application palette's color, whatever Text.linkColor says (WhatsNewCard).
    const auto linksFollowAccent = [this] {
        QPalette palette = QGuiApplication::palette();
        palette.setColor(QPalette::Link, theme.accent());
        QGuiApplication::setPalette(palette);
    };
    linksFollowAccent();
    QObject::connect(&theme, &Theme::accentChanged, &theme, linksFollowAccent);

    AppState::setInstance(&state);
    ResumeStore::setInstance(&resume);
    I18n::setInstance(&i18n);
    Theme::setInstance(&theme);
    MixerController::setInstance(&mixer);
    PictureQuality::setInstance(&quality);
    PlayerSettings::setInstance(&player);
    UpdateCheck::setInstance(&updates);
    Updater::setInstance(&updater);
    Thumbnailer::setInstance(&thumbnails);
    ClipInfo::setInstance(&clipInfo);
    SilenceCheck::setInstance(&silence);
    CutSaver::setInstance(&cuts);
}
