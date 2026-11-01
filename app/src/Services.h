#pragma once

#include <QString>
#include "AppState.h"
#include "I18n.h"
#include "MixerController.h"
#include "PictureQuality.h"
#include "PlayerSettings.h"
#include "ResumeStore.h"
#include "SilenceCheck.h"
#include "Theme.h"
#include "Thumbnailer.h"
#include "UpdateCheck.h"
#include "Updater.h"

// Everything QML reaches as a singleton, created and wired in one place, so the app (main.cpp) and the
// end-to-end test (tests/tst_e2e.cpp) start the same player. Create it before the QML engine and destroy it
// after: the destructors write pending state.
struct Services
{
    // configDir holds state.json and resume.json; appDir holds lang/ and theme.json (next to the exe).
    Services(const QString &configDir, const QString &appDir, const QString &version);

    AppState state;
    ResumeStore resume;
    I18n i18n;
    Theme theme;
    MixerController mixer;
    PictureQuality quality;
    PlayerSettings player;
    UpdateCheck updates;
    Updater updater;
    Thumbnailer thumbnails;
    SilenceCheck silence;
};
