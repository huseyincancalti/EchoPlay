#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class AppState;

// The player settings behind Settings > Audio, Playback, Screenshots and Shortcuts: v1's (what happens when a
// video ends, the speed keys, where screenshots go) and v2's own. Values are checked here and saved in
// AppState. No libmpv here: QML sets mpvOptions() before the first file and again on changed() and on every
// new file (the video's own folder is one of the screenshot places).
class PlayerSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString endMode READ endMode WRITE setEndMode NOTIFY changed)               // "next", "loop", "stop"
    Q_PROPERTY(double speedFactor READ speedFactor WRITE setSpeedFactor NOTIFY changed)    // SpeedFactorMin..Max
    Q_PROPERTY(double speedStep READ speedStep WRITE setSpeedStep NOTIFY changed)          // 0 < step <= 1
    Q_PROPERTY(QString screenshotFolder READ screenshotFolder WRITE setScreenshotFolder NOTIFY changed)
    Q_PROPERTY(QString screenshotCustomPath READ screenshotCustomPath WRITE setScreenshotCustomPath NOTIFY changed)
    Q_PROPERTY(int seekStep READ seekStep WRITE setSeekStep NOTIFY changed)                // 3, 5, 10, 30 seconds
    Q_PROPERTY(int volumeMax READ volumeMax WRITE setVolumeMax NOTIFY changed)             // 100..300 percent
    Q_PROPERTY(bool loudnessBoost READ loudnessBoost WRITE setLoudnessBoost NOTIFY changed)
    Q_PROPERTY(QString loudnessStrength READ loudnessStrength WRITE setLoudnessStrength NOTIFY changed) // "low", "medium", "high"
    Q_PROPERTY(QString audioDevice READ audioDevice WRITE setAudioDevice NOTIFY changed)   // mpv's audio-device
    Q_PROPERTY(QString tracksOnOpen READ tracksOnOpen WRITE setTracksOnOpen NOTIFY changed) // "last", "all"
    Q_PROPERTY(QString resumeMode READ resumeMode WRITE setResumeMode NOTIFY changed)      // "ask", "always", "never"
    Q_PROPERTY(QVariantMap picture READ picture NOTIFY changed)                            // brightness, contrast, saturation, gamma: -100..100
    Q_PROPERTY(double subtitleScale READ subtitleScale WRITE setSubtitleScale NOTIFY changed)          // 0.5..3, 1 = as authored
    Q_PROPERTY(int subtitlePosition READ subtitlePosition WRITE setSubtitlePosition NOTIFY changed)    // 0..150, 100 = bottom
    Q_PROPERTY(bool firstRunHintDone READ firstRunHintDone WRITE setFirstRunHintDone NOTIFY changed)
    Q_PROPERTY(bool clickPauses READ clickPauses WRITE setClickPauses NOTIFY changed)
    Q_PROPERTY(bool showRemaining READ showRemaining WRITE setShowRemaining NOTIFY changed)
    Q_PROPERTY(bool alwaysOnTop READ alwaysOnTop WRITE setAlwaysOnTop NOTIFY changed)
    Q_PROPERTY(QVariantMap windowPlacement READ windowPlacement WRITE setWindowPlacement NOTIFY changed) // WindowChrome's
    Q_PROPERTY(QVariantMap shortcuts READ shortcuts NOTIFY changed)                        // action -> key, only the changed ones

public:
    static constexpr double SpeedFactorMin = 1.25, SpeedFactorMax = 4; // v1's range

    explicit PlayerSettings(AppState *state, QObject *parent = nullptr);

    QString endMode() const;
    void setEndMode(const QString &mode); // anything else is ignored
    double speedFactor() const;
    void setSpeedFactor(double factor); // clamped to the range
    double speedStep() const;
    void setSpeedStep(double step); // ignored outside (0, 1]
    // "app" (EchoPlay's own folder in AppData, the default), "desktop", "videos", "pictures", "video_folder" (next to
    // the video), or "custom" (screenshotCustomPath)
    QString screenshotFolder() const;
    void setScreenshotFolder(const QString &folder);
    QString screenshotCustomPath() const;
    void setScreenshotCustomPath(const QString &path);
    // Where the shots go now, as Windows writes it; for "next to the video" that is the default, as no video is named.
    Q_INVOKABLE QString screenshotPath() const;
    Q_INVOKABLE bool openScreenshotFolder() const; // in Explorer; the folder is made if it is not there yet
    int seekStep() const;
    void setSeekStep(int seconds); // anything else is ignored
    int volumeMax() const;
    void setVolumeMax(int percent); // clamped to the range
    bool loudnessBoost() const;
    void setLoudnessBoost(bool on);
    QString loudnessStrength() const;
    void setLoudnessStrength(const QString &strength); // anything else is ignored
    QString audioDevice() const;
    void setAudioDevice(const QString &device); // empty is ignored: "auto" follows Windows
    QString tracksOnOpen() const;
    void setTracksOnOpen(const QString &tracks); // anything else is ignored
    QString resumeMode() const;
    void setResumeMode(const QString &mode); // anything else is ignored
    QVariantMap picture() const; // all four, as ints
    Q_INVOKABLE void setPicture(const QString &name, int value); // an unknown name is ignored, the value is clamped
    Q_INVOKABLE void resetPicture();                             // all four back to 0
    double subtitleScale() const;
    void setSubtitleScale(double scale); // clamped to the range, kept to 2 decimals so +0.1 steps do not drift
    int subtitlePosition() const;
    void setSubtitlePosition(int position); // clamped to the range
    bool firstRunHintDone() const;
    void setFirstRunHintDone(bool done);
    bool clickPauses() const;
    void setClickPauses(bool on);
    bool showRemaining() const;
    void setShowRemaining(bool on);
    bool alwaysOnTop() const;
    void setAlwaysOnTop(bool on);
    QVariantMap windowPlacement() const;
    void setWindowPlacement(const QVariantMap &placement);
    QVariantMap shortcuts() const;
    Q_INVOKABLE void setShortcut(const QString &action, const QString &key);
    Q_INVOKABLE void resetShortcuts();
    // A pressed key as Shortcut's sequence reads it, "" for a modifier alone. A symbol's Shift is part of the symbol.
    Q_INVOKABLE static QString keyName(int key, int modifiers);

    // The mpv properties for these settings while `videoPath` plays: loop-file, keep-open, screenshot-dir,
    // volume-max, af, audio-device, brightness, contrast, saturation, gamma, sub-scale, sub-pos.
    Q_INVOKABLE QVariantMap mpvOptions(const QString &videoPath) const;
    // The fixed-speed key (v1): from normal speed to speedFactor, from any faster speed back to 1x.
    Q_INVOKABLE double toggledSpeed(double current) const { return current > 1.01 ? 1 : speedFactor(); }

    static PlayerSettings *create(QQmlEngine *, QJSEngine *)
    {
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
    static void setInstance(PlayerSettings *settings) { s_instance = settings; } // main.cpp, before QML loads

signals:
    void changed();

private:
    QString screenshotDir(const QString &videoPath) const;

    static inline PlayerSettings *s_instance = nullptr;
    AppState *m_state;
};
