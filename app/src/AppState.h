#pragma once

#include <QQmlEngine>
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include "Mixer.h"

// The user's choices that persist across files and restarts, in one JSON file with a schema version.
// Every setting follows load -> apply at start -> save on change (CLAUDE.md). A new setting: a member,
// its accessors, one line each in load() and save(), a test. When a saved field changes meaning, bump
// Schema and convert the old shape in load(), the only place that knows older formats.
class AppState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)

public:
    static constexpr int Schema = 1;
    static constexpr double VolumeMax = 300; // mpv's volume-max, as in v1
    static constexpr int PictureMax = 100;   // the picture adjustments run -PictureMax..PictureMax
    static constexpr double SubtitleScaleMin = 0.5, SubtitleScaleMax = 3.0; // mpv's sub-scale, as shown
    static constexpr int SubtitlePositionMax = 150; // mpv's sub-pos: 0 top, 100 bottom, up to 150 below the picture

    explicit AppState(const QString &path, QObject *parent = nullptr); // loads the file
    ~AppState() override;                                               // writes a pending save

    QString language() const { return m_language; } // empty = follow the system language
    void setLanguage(const QString &code);
    double volume() const { return m_volume; } // mpv's volume, 0..VolumeMax
    void setVolume(double volume);
    QString quality() const { return m_quality; } // picture quality: "auto", "ultra", "normal", "low"
    void setQuality(const QString &quality);
    MixerPrefs mixer() const { return m_mixer; }
    void setMixer(const MixerPrefs &prefs);
    // Playback and screenshot settings (v1's): their values are checked by PlayerSettings, which owns them.
    QString endMode() const { return m_endMode; } // "next", "loop", "stop"
    void setEndMode(const QString &mode);
    double speedFactor() const { return m_speedFactor; } // the speed the fixed-speed key toggles to
    void setSpeedFactor(double factor);
    double speedStep() const { return m_speedStep; } // the speed-down/up keys' step
    void setSpeedStep(double step);
    QString directory() const; // the folder of the state file: EchoPlay's own folder in the user's AppData
    QString screenshotFolder() const { return m_screenshotFolder; } // "app", "desktop", "videos", "pictures", "video_folder", "custom"
    void setScreenshotFolder(const QString &folder);
    QString screenshotCustomPath() const { return m_screenshotCustomPath; }
    void setScreenshotCustomPath(const QString &path);
    int seekStep() const { return m_seekStep; } // seconds the left and right arrows jump
    void setSeekStep(int seconds);
    int volumeMax() const { return m_volumeMax; } // the highest level the slider and keys reach
    void setVolumeMax(int percent);
    bool loudnessBoost() const { return m_loudnessBoost; }
    void setLoudnessBoost(bool on);
    QString loudnessStrength() const { return m_loudnessStrength; } // "low", "medium", "high"
    void setLoudnessStrength(const QString &strength);
    QString audioDevice() const { return m_audioDevice; } // mpv's audio-device; "auto" follows Windows
    void setAudioDevice(const QString &device);
    QString tracksOnOpen() const { return m_tracksOnOpen; } // "last" (as the last clip), "all"
    void setTracksOnOpen(const QString &tracks);
    QVariantMap picture() const { return m_picture; } // brightness, contrast, saturation, gamma: all four, as mpv names them
    void setPicture(const QString &name, int value);  // an unknown name is ignored; PlayerSettings checks the range
    double subtitleScale() const { return m_subtitleScale; } // SubtitleScaleMin..Max, 1 = the subtitle's own size
    void setSubtitleScale(double scale);
    int subtitlePosition() const { return m_subtitlePosition; } // 0..SubtitlePositionMax, 100 = mpv's default
    void setSubtitlePosition(int position);
    QString resumeMode() const { return m_resumeMode; } // "ask", "always", "never"
    void setResumeMode(const QString &mode);
    bool firstRunHintDone() const { return m_firstRunHintDone; }
    void setFirstRunHintDone(bool done);
    bool clickPauses() const { return m_clickPauses; } // a click on the video pauses and plays
    void setClickPauses(bool on);
    bool showRemaining() const { return m_showRemaining; } // the bar's time: what is left instead of what has played
    void setShowRemaining(bool on);
    bool alwaysOnTop() const { return m_alwaysOnTop; }
    void setAlwaysOnTop(bool on);
    QVariantMap windowPlacement() const { return m_windowPlacement; } // WindowChrome's; empty until the first close
    void setWindowPlacement(const QVariantMap &placement);
    QVariantMap shortcuts() const { return m_shortcuts; } // action -> key sequence, only what the user changed
    void setShortcuts(const QVariantMap &shortcuts);
    // Settings > Appearance: their values are checked by Theme, which owns them. Empty = the theme file's token.
    QString themeMode() const { return m_themeMode; } // "dark", "light", "system"
    void setThemeMode(const QString &mode);
    QString accentColor() const { return m_accentColor; }
    void setAccentColor(const QString &color);
    QString barStyle() const { return m_barStyle; } // "island", "full"
    void setBarStyle(const QString &style);
    QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString &family);
    int glassStrength() const { return m_glassStrength; } // 0..2
    void setGlassStrength(int strength);
    // The update check: when it last ran.
    qint64 updateLastChecked() const { return m_updateLastChecked; } // seconds since the epoch; 0 = never
    void setUpdateLastChecked(qint64 epochSeconds);
    // The notes of the newest release found, for the card once the app runs that version (UpdateCheck).
    QString notesVersion() const { return m_notesVersion; }
    QString notesText() const { return m_notesText; }
    void setReleaseNotes(const QString &version, const QString &text);

    // Writes now. A change schedules this a moment later instead: a dragged slider changes a value
    // many times per second, and one write is enough.
    void save();

    static AppState *create(QQmlEngine *, QJSEngine *)
    {
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
    static void setInstance(AppState *state) { s_instance = state; } // main.cpp, before QML loads

signals:
    void languageChanged();
    void volumeChanged();

private:
    void load();
    void changed();

    static inline AppState *s_instance = nullptr;
    QString m_path;
    QTimer m_saveTimer;
    QString m_language;
    double m_volume = 100;
    QString m_quality = QStringLiteral("auto");
    MixerPrefs m_mixer;
    QString m_endMode = QStringLiteral("stop");
    double m_speedFactor = 2.0;
    double m_speedStep = 0.1;
    QString m_screenshotFolder = QStringLiteral("app");
    QString m_screenshotCustomPath;
    int m_seekStep = 10;
    int m_volumeMax = int(VolumeMax);
    bool m_loudnessBoost = true;
    QString m_loudnessStrength = QStringLiteral("medium");
    QString m_audioDevice = QStringLiteral("auto");
    QString m_tracksOnOpen = QStringLiteral("last");
    QVariantMap m_picture{{"brightness", 0}, {"contrast", 0}, {"saturation", 0}, {"gamma", 0}};
    double m_subtitleScale = 1.0;
    int m_subtitlePosition = 100;
    QString m_resumeMode = QStringLiteral("ask");
    bool m_firstRunHintDone = false;
    bool m_clickPauses = true;
    bool m_showRemaining = false;
    bool m_alwaysOnTop = false;
    QVariantMap m_windowPlacement;
    QVariantMap m_shortcuts;
    QString m_themeMode = QStringLiteral("dark");
    QString m_accentColor;
    QString m_barStyle;
    QString m_fontFamily;
    int m_glassStrength = 1;
    qint64 m_updateLastChecked = 0;
    QString m_notesVersion;
    QString m_notesText;
};
