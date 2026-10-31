#include "PlayerSettings.h"

#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>
#include "AppState.h"

PlayerSettings::PlayerSettings(AppState *state, QObject *parent) : QObject(parent), m_state(state) {}

QString PlayerSettings::endMode() const { return m_state->endMode(); }

void PlayerSettings::setEndMode(const QString &mode)
{
    if (mode == endMode() || !QStringList{"next", "loop", "stop"}.contains(mode)) return;
    m_state->setEndMode(mode);
    emit changed();
}

double PlayerSettings::speedFactor() const { return m_state->speedFactor(); }

void PlayerSettings::setSpeedFactor(double factor)
{
    factor = qBound(SpeedFactorMin, factor, SpeedFactorMax);
    if (factor == speedFactor()) return;
    m_state->setSpeedFactor(factor);
    emit changed();
}

double PlayerSettings::speedStep() const { return m_state->speedStep(); }

void PlayerSettings::setSpeedStep(double step)
{
    if (step == speedStep() || !(step > 0 && step <= 1)) return;
    m_state->setSpeedStep(step);
    emit changed();
}

QString PlayerSettings::screenshotFolder() const { return m_state->screenshotFolder(); }

void PlayerSettings::setScreenshotFolder(const QString &folder)
{
    if (folder == screenshotFolder() ||
        !QStringList{"desktop", "videos", "pictures", "video_folder", "custom"}.contains(folder))
        return;
    m_state->setScreenshotFolder(folder);
    emit changed();
}

QString PlayerSettings::screenshotCustomPath() const { return m_state->screenshotCustomPath(); }

void PlayerSettings::setScreenshotCustomPath(const QString &path)
{
    if (path == screenshotCustomPath()) return;
    m_state->setScreenshotCustomPath(path);
    emit changed();
}

QVariantMap PlayerSettings::mpvOptions(const QString &videoPath) const
{
    // keep-open=always, not yes: yes still moves on to the next file of the folder playlist (v1, checked live).
    const QString end = endMode();
    QVariantMap out{{"loop-file", end == "loop" ? "inf" : "no"}, {"keep-open", end == "stop" ? "always" : "no"}};

    QString dir;
    const QString folder = screenshotFolder();
    if (folder == "videos") dir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    else if (folder == "pictures") dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    else if (folder == "video_folder" && !videoPath.contains("://") && !videoPath.isEmpty())
        dir = QFileInfo(videoPath).absolutePath();
    else if (folder == "custom") dir = screenshotCustomPath();
    // The desktop also when the chosen place does not apply (a URL has no folder, no custom folder picked).
    if (dir.isEmpty()) dir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    // screenshot-dir, not its old alias screenshot-directory: observing the alias misses changes.
    out.insert("screenshot-dir", dir);
    return out;
}
