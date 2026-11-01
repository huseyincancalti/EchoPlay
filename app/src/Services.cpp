#include "Services.h"

#include <QProcess>
#include <QStandardPaths>

Services::Services(const QString &configDir, const QString &appDir, const QString &version)
    : state(configDir + "/state.json"),
      resume(configDir + "/resume.json"), // its own file: it grows with every file watched
      i18n(appDir + "/lang"),
      theme(&state, ":/theme/theme.json", appDir + "/theme.json", configDir + "/fonts"),
      mixer(&state),
      quality(&state),
      player(&state),
      updates(&state, version),
      // The setup is a per-user installer: silent, no restart prompt. ECHOPLAYUPDATE makes it start the new
      // EchoPlay when it is done (installer/windows/v2/EchoPlay.iss).
      updater(QStandardPaths::writableLocation(QStandardPaths::TempLocation),
              [](const QString &setupPath) {
                  return QProcess::startDetached(setupPath, {"/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART",
                                                             "/ECHOPLAYUPDATE=1"});
              }),
      thumbnails(nullptr),
      silence(nullptr)
{
    i18n.setLanguage(state.language()); // empty: the system language, and nothing is saved until the user picks one
    QObject::connect(&i18n, &I18n::languageChanged, &state, [this] { state.setLanguage(i18n.language()); });
    theme.startGui();

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
    SilenceCheck::setInstance(&silence);
}
