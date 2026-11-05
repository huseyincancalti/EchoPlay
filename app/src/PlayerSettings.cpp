#include "PlayerSettings.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QKeySequence>
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
        !QStringList{"app", "desktop", "videos", "pictures", "video_folder", "custom"}.contains(folder))
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

QString PlayerSettings::screenshotDir(const QString &videoPath) const
{
    QString dir;
    const QString folder = screenshotFolder();
    if (folder == "desktop") dir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    else if (folder == "videos") dir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    else if (folder == "pictures") dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    else if (folder == "video_folder" && !videoPath.contains("://") && !videoPath.isEmpty())
        dir = QFileInfo(videoPath).absolutePath();
    else if (folder == "custom") dir = screenshotCustomPath();
    // EchoPlay's own folder also when the chosen place does not apply (a URL has no folder, no custom folder picked).
    // mpv makes it at the first screenshot.
    if (dir.isEmpty()) dir = m_state->directory() + "/Screenshots";
    return dir;
}

QString PlayerSettings::screenshotPath() const { return QDir::toNativeSeparators(screenshotDir({})); }

bool PlayerSettings::openScreenshotFolder() const
{
    const QString dir = screenshotDir({});
    return QDir().mkpath(dir) && QDesktopServices::openUrl(QUrl::fromLocalFile(dir)); // there may be no shot yet
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

QVariantMap PlayerSettings::picture() const { return m_state->picture(); }

void PlayerSettings::setPicture(const QString &name, int value)
{
    value = qBound(-AppState::PictureMax, value, AppState::PictureMax);
    const QVariantMap now = picture();
    if (!now.contains(name) || now.value(name).toInt() == value) return;
    m_state->setPicture(name, value);
    emit changed();
}

void PlayerSettings::resetPicture()
{
    const QVariantMap before = picture();
    for (auto it = before.cbegin(); it != before.cend(); ++it) m_state->setPicture(it.key(), 0);
    if (picture() != before) emit changed();
}

double PlayerSettings::subtitleScale() const { return m_state->subtitleScale(); }

void PlayerSettings::setSubtitleScale(double scale)
{
    scale = qRound(qBound(AppState::SubtitleScaleMin, scale, AppState::SubtitleScaleMax) * 100) / 100.0;
    if (scale == subtitleScale()) return;
    m_state->setSubtitleScale(scale);
    emit changed();
}

int PlayerSettings::subtitlePosition() const { return m_state->subtitlePosition(); }

void PlayerSettings::setSubtitlePosition(int position)
{
    position = qBound(0, position, AppState::SubtitlePositionMax);
    if (position == subtitlePosition()) return;
    m_state->setSubtitlePosition(position);
    emit changed();
}

bool PlayerSettings::firstRunHintDone() const { return m_state->firstRunHintDone(); }

void PlayerSettings::setFirstRunHintDone(bool done)
{
    if (done == firstRunHintDone()) return;
    m_state->setFirstRunHintDone(done);
    emit changed();
}

bool PlayerSettings::clickPauses() const { return m_state->clickPauses(); }

void PlayerSettings::setClickPauses(bool on)
{
    if (on == clickPauses()) return;
    m_state->setClickPauses(on);
    emit changed();
}

bool PlayerSettings::showRemaining() const { return m_state->showRemaining(); }

void PlayerSettings::setShowRemaining(bool on)
{
    if (on == showRemaining()) return;
    m_state->setShowRemaining(on);
    emit changed();
}

bool PlayerSettings::alwaysOnTop() const { return m_state->alwaysOnTop(); }

void PlayerSettings::setAlwaysOnTop(bool on)
{
    if (on == alwaysOnTop()) return;
    m_state->setAlwaysOnTop(on);
    emit changed();
}

QVariantMap PlayerSettings::windowPlacement() const { return m_state->windowPlacement(); }

void PlayerSettings::setWindowPlacement(const QVariantMap &placement)
{
    if (placement == windowPlacement()) return;
    m_state->setWindowPlacement(placement);
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

QString PlayerSettings::keyName(int key, int modifiers)
{
    switch (key) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Meta:
    case Qt::Key_unknown:
        return {};
    default:
        break;
    }
    Qt::KeyboardModifiers mods(modifiers & ~Qt::KeypadModifier);
    const bool letter = key >= Qt::Key_A && key <= Qt::Key_Z;
    if (key >= Qt::Key_Space && key <= Qt::Key_AsciiTilde && !letter) mods &= ~Qt::ShiftModifier;
    return QKeySequence(QKeyCombination(mods, Qt::Key(key))).toString(QKeySequence::PortableText);
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

    // screenshot-dir, not its old alias screenshot-directory: observing the alias misses changes.
    out.insert("screenshot-dir", screenshotDir(videoPath));

    out.insert("volume-max", QString::number(volumeMax()));
    out.insert("audio-device", audioDevice());
    // The picture's keys are mpv's own option names.
    const QVariantMap look = picture();
    for (auto it = look.cbegin(); it != look.cend(); ++it) out.insert(it.key(), QString::number(it.value().toInt()));
    out.insert("sub-scale", QString::number(subtitleScale()));
    out.insert("sub-pos", QString::number(subtitlePosition()));
    // Medium is v1's levelling. A stronger one lifts quiet parts more (m) and reacts faster (a small g), so it can pump.
    const QString strength = loudnessStrength();
    out.insert("af", !loudnessBoost()      ? QString()
                     : strength == "low"   ? QStringLiteral("dynaudnorm=p=0.9:m=10:g=25")
                     : strength == "high"  ? QStringLiteral("dynaudnorm=p=0.95:m=50:g=9")
                                           : QStringLiteral("dynaudnorm=p=0.9:m=25:g=15"));
    return out;
}
