#include "Services.h"

Services::Services(const QString &configDir, const QString &appDir, const QString &version)
    : state(configDir + "/state.json"),
      resume(configDir + "/resume.json"), // its own file: it grows with every file watched
      i18n(appDir + "/lang"),
      theme(":/theme/theme.json", appDir + "/theme.json"),
      mixer(&state),
      quality(&state),
      updates(&state, version),
      thumbnails(nullptr),
      silence(nullptr)
{
    i18n.setLanguage(state.language()); // empty: the system language, and nothing is saved until the user picks one
    QObject::connect(&i18n, &I18n::languageChanged, &state, [this] { state.setLanguage(i18n.language()); });

    AppState::setInstance(&state);
    ResumeStore::setInstance(&resume);
    I18n::setInstance(&i18n);
    Theme::setInstance(&theme);
    MixerController::setInstance(&mixer);
    PictureQuality::setInstance(&quality);
    UpdateCheck::setInstance(&updates);
    Thumbnailer::setInstance(&thumbnails);
    SilenceCheck::setInstance(&silence);
}
