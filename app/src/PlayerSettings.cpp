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

int PlayerSettings::seekStep() const { return m_state->seekStep(); }

void PlayerSettings::setSeekStep(int seconds)
{
    if (seconds == seekStep() || !QList<int>{3, 5, 10, 30}.contains(seconds)) return;
    m_state->setSeekStep(seconds);
    emit changed();
}

int PlayerSettings::volumeMax() const { return m_state->volumeMax(); }

void PlayerSettings::setVolumeMax(int percent)
{
    percent = qBound(100, percent, int(AppState::VolumeMax));
    if (percent == volumeMax()) return;
    m_state->setVolumeMax(percent);
    emit changed();
}

bool PlayerSettings::loudnessBoost() const { return m_state->loudnessBoost(); }

void PlayerSettings::setLoudnessBoost(bool on)
{
    if (on == loudnessBoost()) return;
    m_state->setLoudnessBoost(on);
    emit changed();
}

QString PlayerSettings::loudnessStrength() const { return m_state->loudnessStrength(); }

void PlayerSettings::setLoudnessStrength(const QString &strength)
{
    if (strength == loudnessStrength() || !QStringList{"low", "medium", "high"}.contains(strength)) return;
    m_state->setLoudnessStrength(strength);
    emit changed();
}

QString PlayerSettings::audioDevice() const { return m_state->audioDevice(); }

void PlayerSettings::setAudioDevice(const QString &device)
{
    if (device == audioDevice() || device.isEmpty()) return;
    m_state->setAudioDevice(device);
    emit changed();
}

QString PlayerSettings::tracksOnOpen() const { return m_state->tracksOnOpen(); }

void PlayerSettings::setTracksOnOpen(const QString &tracks)
{
    if (tracks == tracksOnOpen() || !QStringList{"last", "all"}.contains(tracks)) return;
    m_state->setTracksOnOpen(tracks);
    emit changed();
}

QString PlayerSettings::resumeMode() const { return m_state->resumeMode(); }

void PlayerSettings::setResumeMode(const QString &mode)
{
    if (mode == resumeMode() || !QStringList{"ask", "always", "never"}.contains(mode)) return;
    m_state->setResumeMode(mode);
    emit changed();
}

bool PlayerSettings::firstRunHintDone() const { return m_state->firstRunHintDone(); }

void PlayerSettings::setFirstRunHintDone(bool done)
{
    if (done == firstRunHintDone()) return;
    m_state->setFirstRunHintDone(done);
    emit changed();
}

QVariantMap PlayerSettings::shortcuts() const { return m_state->shortcuts(); }

void PlayerSettings::setShortcut(const QString &action, const QString &key)
{
    QVariantMap map = shortcuts();
    if (map.value(action).toString() == key) return;
    map.insert(action, key);
    m_state->setShortcuts(map);
    emit changed();
}

void PlayerSettings::resetShortcuts()
{
    if (shortcuts().isEmpty()) return;
    m_state->setShortcuts({});
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

    out.insert("volume-max", QString::number(volumeMax()));
    out.insert("audio-device", audioDevice());
    // Medium is v1's levelling. A stronger one lifts quiet parts more (m) and reacts faster (a small g), so it can pump.
    const QString strength = loudnessStrength();
    out.insert("af", !loudnessBoost()      ? QString()
                     : strength == "low"   ? QStringLiteral("dynaudnorm=p=0.9:m=10:g=25")
                     : strength == "high"  ? QStringLiteral("dynaudnorm=p=0.95:m=50:g=9")
                                           : QStringLiteral("dynaudnorm=p=0.9:m=25:g=15"));
    return out;
}
