#pragma once

#include <QQmlEngine>
#include <QObject>
#include <QTimer>
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
    QString screenshotFolder() const { return m_screenshotFolder; } // "desktop", "videos", "pictures", "video_folder", "custom"
    void setScreenshotFolder(const QString &folder);
    QString screenshotCustomPath() const { return m_screenshotCustomPath; }
    void setScreenshotCustomPath(const QString &path);
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
    // The daily update check: when it last ran, and the newest version already shown to the user (shown once).
    qint64 updateLastChecked() const { return m_updateLastChecked; } // seconds since the epoch; 0 = never
    void setUpdateLastChecked(qint64 epochSeconds);
    QString updateSeenVersion() const { return m_updateSeenVersion; }
    void setUpdateSeenVersion(const QString &version);

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
    QString m_screenshotFolder = QStringLiteral("desktop");
    QString m_screenshotCustomPath;
    QString m_themeMode = QStringLiteral("dark");
    QString m_accentColor;
    QString m_barStyle;
    QString m_fontFamily;
    int m_glassStrength = 1;
    qint64 m_updateLastChecked = 0;
    QString m_updateSeenVersion;
};
