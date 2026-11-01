#include "AppState.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>

AppState::AppState(const QString &path, QObject *parent) : QObject(parent), m_path(path)
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(500);
    connect(&m_saveTimer, &QTimer::timeout, this, &AppState::save);
    load();
}

AppState::~AppState()
{
    if (m_saveTimer.isActive()) save();
}

void AppState::setLanguage(const QString &code)
{
    if (code == m_language) return;
    m_language = code;
    emit languageChanged();
    changed();
}

void AppState::setVolume(double volume)
{
    if (volume == m_volume) return;
    m_volume = volume;
    emit volumeChanged();
    changed();
}

void AppState::setQuality(const QString &quality)
{
    if (quality == m_quality) return;
    m_quality = quality;
    changed();
}

void AppState::setMixer(const MixerPrefs &prefs)
{
    if (prefs == m_mixer) return;
    m_mixer = prefs;
    changed();
}

void AppState::setEndMode(const QString &mode)
{
    if (mode == m_endMode) return;
    m_endMode = mode;
    changed();
}

void AppState::setSpeedFactor(double factor)
{
    if (factor == m_speedFactor) return;
    m_speedFactor = factor;
    changed();
}

void AppState::setSpeedStep(double step)
{
    if (step == m_speedStep) return;
    m_speedStep = step;
    changed();
}

void AppState::setScreenshotFolder(const QString &folder)
{
    if (folder == m_screenshotFolder) return;
    m_screenshotFolder = folder;
    changed();
}

void AppState::setScreenshotCustomPath(const QString &path)
{
    if (path == m_screenshotCustomPath) return;
    m_screenshotCustomPath = path;
    changed();
}

void AppState::setSeekStep(int seconds)
{
    if (seconds == m_seekStep) return;
    m_seekStep = seconds;
    changed();
}

void AppState::setVolumeMax(int percent)
{
    if (percent == m_volumeMax) return;
    m_volumeMax = percent;
    changed();
}

void AppState::setLoudnessBoost(bool on)
{
    if (on == m_loudnessBoost) return;
    m_loudnessBoost = on;
    changed();
}

void AppState::setLoudnessStrength(const QString &strength)
{
    if (strength == m_loudnessStrength) return;
    m_loudnessStrength = strength;
    changed();
}

void AppState::setAudioDevice(const QString &device)
{
    if (device == m_audioDevice) return;
    m_audioDevice = device;
    changed();
}

void AppState::setTracksOnOpen(const QString &tracks)
{
    if (tracks == m_tracksOnOpen) return;
    m_tracksOnOpen = tracks;
    changed();
}

void AppState::setResumeMode(const QString &mode)
{
    if (mode == m_resumeMode) return;
    m_resumeMode = mode;
    changed();
}

void AppState::setFirstRunHintDone(bool done)
{
    if (done == m_firstRunHintDone) return;
    m_firstRunHintDone = done;
    changed();
}

void AppState::setShortcuts(const QVariantMap &shortcuts)
{
    if (shortcuts == m_shortcuts) return;
    m_shortcuts = shortcuts;
    changed();
}

void AppState::setThemeMode(const QString &mode)
{
    if (mode == m_themeMode) return;
    m_themeMode = mode;
    changed();
}

void AppState::setAccentColor(const QString &color)
{
    if (color == m_accentColor) return;
    m_accentColor = color;
    changed();
}

void AppState::setBarStyle(const QString &style)
{
    if (style == m_barStyle) return;
    m_barStyle = style;
    changed();
}

void AppState::setFontFamily(const QString &family)
{
    if (family == m_fontFamily) return;
    m_fontFamily = family;
    changed();
}

void AppState::setGlassStrength(int strength)
{
    if (strength == m_glassStrength) return;
    m_glassStrength = strength;
    changed();
}

void AppState::setUpdateLastChecked(qint64 epochSeconds)
{
    if (epochSeconds == m_updateLastChecked) return;
    m_updateLastChecked = epochSeconds;
    changed();
}

void AppState::setReleaseNotes(const QString &version, const QString &text)
{
    if (version == m_notesVersion && text == m_notesText) return;
    m_notesVersion = version;
    m_notesText = text;
    changed();
}

void AppState::changed()
{
    m_saveTimer.start();
}

// Each field is read only when it has the expected shape; anything else keeps its default, so a
// hand-edited or damaged file never breaks start-up. A file from a newer EchoPlay (higher schema) is
// read the same way: the fields this version knows are still worth keeping.
void AppState::load()
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly)) return; // first run
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (!doc.isObject()) {
        qWarning("AppState: %s is not a JSON object (%s), using defaults", qPrintable(m_path),
                 qPrintable(err.errorString()));
        return;
    }
    const QJsonObject s = doc.object();

    if (s.value("language").isString()) m_language = s.value("language").toString();
    const double volume = s.value("volume").toDouble(-1);
    if (volume >= 0 && volume <= VolumeMax) m_volume = volume;
    static const QRegularExpression quality("^(auto|ultra|normal|low)$");
    if (quality.match(s.value("quality").toString()).hasMatch()) m_quality = s.value("quality").toString();

    const QJsonObject mixer = s.value("mixer").toObject();
    static const QRegularExpression defaultOn("^(all|none|\\d+(,\\d+)*)$");
    if (defaultOn.match(mixer.value("default_on").toString()).hasMatch())
        m_mixer.defaultOn = mixer.value("default_on").toString();
    const QJsonObject gains = mixer.value("gains").toObject();
    for (auto it = gains.begin(); it != gains.end(); ++it) {
        bool isId = false;
        const int id = it.key().toInt(&isId);
        const double gain = it.value().toDouble(-1);
        if (isId && gain >= 0 && gain <= Mixer::GainMax) m_mixer.gains.insert(id, gain);
    }
    for (const QJsonValue &id : mixer.value("mono").toArray())
        if (id.isDouble()) m_mixer.mono.insert(id.toInt());

    const QJsonObject playback = s.value("playback").toObject();
    static const QRegularExpression endMode("^(next|loop|stop)$");
    if (endMode.match(playback.value("end").toString()).hasMatch()) m_endMode = playback.value("end").toString();
    const double factor = playback.value("speed_factor").toDouble(-1);
    if (factor >= 1.25 && factor <= 4) m_speedFactor = factor;
    const double step = playback.value("speed_step").toDouble(-1);
    if (step > 0 && step <= 1) m_speedStep = step;
    const int seekStep = playback.value("seek_step").toInt();
    if (seekStep == 3 || seekStep == 5 || seekStep == 10 || seekStep == 30) m_seekStep = seekStep;
    static const QRegularExpression resumeMode("^(ask|always|never)$");
    if (resumeMode.match(playback.value("resume").toString()).hasMatch()) m_resumeMode = playback.value("resume").toString();

    const QJsonObject audio = s.value("audio").toObject();
    const int volumeMax = audio.value("volume_max").toInt();
    if (volumeMax >= 100 && volumeMax <= VolumeMax) m_volumeMax = volumeMax;
    if (audio.value("loudness_boost").isBool()) m_loudnessBoost = audio.value("loudness_boost").toBool();
    static const QRegularExpression strength("^(low|medium|high)$");
    if (strength.match(audio.value("loudness_strength").toString()).hasMatch())
        m_loudnessStrength = audio.value("loudness_strength").toString();
    if (!audio.value("device").toString().isEmpty()) m_audioDevice = audio.value("device").toString();
    static const QRegularExpression tracksOnOpen("^(last|all)$");
    if (tracksOnOpen.match(audio.value("tracks_on_open").toString()).hasMatch())
        m_tracksOnOpen = audio.value("tracks_on_open").toString();

    m_firstRunHintDone = s.value("first_run_hint_done").toBool();
    const QJsonObject shortcuts = s.value("shortcuts").toObject();
    for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it)
        if (it.value().isString()) m_shortcuts.insert(it.key(), it.value().toString());

    const QJsonObject screenshots = s.value("screenshots").toObject();
    static const QRegularExpression folder("^(desktop|videos|pictures|video_folder|custom)$");
    if (folder.match(screenshots.value("folder").toString()).hasMatch())
        m_screenshotFolder = screenshots.value("folder").toString();
    if (screenshots.value("custom").isString()) m_screenshotCustomPath = screenshots.value("custom").toString();

    const QJsonObject look = s.value("appearance").toObject();
    static const QRegularExpression themeMode("^(dark|light|system)$");
    if (themeMode.match(look.value("mode").toString()).hasMatch()) m_themeMode = look.value("mode").toString();
    if (QColor::isValidColorName(look.value("accent").toString())) m_accentColor = look.value("accent").toString();
    static const QRegularExpression barStyle("^(island|full)$");
    if (barStyle.match(look.value("bar").toString()).hasMatch()) m_barStyle = look.value("bar").toString();
    if (look.value("font").isString()) m_fontFamily = look.value("font").toString();
    const int glass = look.value("glass").toInt(-1);
    if (glass >= 0 && glass <= 2) m_glassStrength = glass;

    const QJsonObject update = s.value("update").toObject();
    if (update.value("last_checked").isDouble()) m_updateLastChecked = update.value("last_checked").toInteger();
    m_notesVersion = update.value("notes_version").toString();
    m_notesText = update.value("notes").toString();
}

void AppState::save()
{
    m_saveTimer.stop();
    QJsonObject gains;
    for (auto it = m_mixer.gains.cbegin(); it != m_mixer.gains.cend(); ++it)
        gains.insert(QString::number(it.key()), it.value());
    QList<int> mono(m_mixer.mono.cbegin(), m_mixer.mono.cend());
    std::sort(mono.begin(), mono.end()); // stable file content for the same state
    QJsonArray monoIds;
    for (int id : mono) monoIds.append(id);

    const QJsonObject s{
        {"schema", Schema},
        {"language", m_language},
        {"volume", m_volume},
        {"quality", m_quality},
        {"mixer", QJsonObject{{"default_on", m_mixer.defaultOn}, {"gains", gains}, {"mono", monoIds}}},
        {"playback", QJsonObject{{"end", m_endMode}, {"speed_factor", m_speedFactor}, {"speed_step", m_speedStep},
                                 {"seek_step", m_seekStep}, {"resume", m_resumeMode}}},
        {"audio", QJsonObject{{"volume_max", m_volumeMax}, {"loudness_boost", m_loudnessBoost},
                              {"loudness_strength", m_loudnessStrength}, {"device", m_audioDevice},
                              {"tracks_on_open", m_tracksOnOpen}}},
        {"first_run_hint_done", m_firstRunHintDone},
        {"shortcuts", QJsonObject::fromVariantMap(m_shortcuts)},
        {"screenshots", QJsonObject{{"folder", m_screenshotFolder}, {"custom", m_screenshotCustomPath}}},
        {"appearance", QJsonObject{{"mode", m_themeMode}, {"accent", m_accentColor}, {"bar", m_barStyle},
                                   {"font", m_fontFamily}, {"glass", m_glassStrength}}},
        {"update", QJsonObject{{"last_checked", m_updateLastChecked}, {"notes_version", m_notesVersion}, {"notes", m_notesText}}},
    };

    QDir().mkpath(QFileInfo(m_path).absolutePath());
    // QSaveFile writes a temp file and renames it: a crash or power loss mid-write keeps the old state.
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(s).toJson()) < 0 || !f.commit())
        qWarning("AppState: could not save %s: %s", qPrintable(m_path), qPrintable(f.errorString()));
}
