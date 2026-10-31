#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class AppState;

// v1's playback and screenshot settings (main.lua): what happens when a video ends, the speed the fixed-speed
// key toggles to, the speed keys' step, and where screenshots go. Values are checked here and saved in
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

public:
    static constexpr double SpeedFactorMin = 1.25, SpeedFactorMax = 4; // v1's range

    explicit PlayerSettings(AppState *state, QObject *parent = nullptr);

    QString endMode() const;
    void setEndMode(const QString &mode); // anything else is ignored
    double speedFactor() const;
    void setSpeedFactor(double factor); // clamped to the range
    double speedStep() const;
    void setSpeedStep(double step); // ignored outside (0, 1]
    // "desktop", "videos", "pictures", "video_folder" (next to the video), or "custom" (screenshotCustomPath)
    QString screenshotFolder() const;
    void setScreenshotFolder(const QString &folder);
    QString screenshotCustomPath() const;
    void setScreenshotCustomPath(const QString &path);

    // The mpv properties for these settings while `videoPath` plays: loop-file, keep-open, screenshot-dir.
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
    static inline PlayerSettings *s_instance = nullptr;
    AppState *m_state;
};
