#include "AppState.h"

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

void AppState::setUpdateLastChecked(qint64 epochSeconds)
{
    if (epochSeconds == m_updateLastChecked) return;
    m_updateLastChecked = epochSeconds;
    changed();
}

void AppState::setUpdateSeenVersion(const QString &version)
{
    if (version == m_updateSeenVersion) return;
    m_updateSeenVersion = version;
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
    const QJsonObject screenshots = s.value("screenshots").toObject();
    static const QRegularExpression folder("^(desktop|videos|pictures|video_folder|custom)$");
    if (folder.match(screenshots.value("folder").toString()).hasMatch())
        m_screenshotFolder = screenshots.value("folder").toString();
    if (screenshots.value("custom").isString()) m_screenshotCustomPath = screenshots.value("custom").toString();

    const QJsonObject update = s.value("update").toObject();
    if (update.value("last_checked").isDouble()) m_updateLastChecked = update.value("last_checked").toInteger();
    if (update.value("seen_version").isString()) m_updateSeenVersion = update.value("seen_version").toString();
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
        {"playback", QJsonObject{{"end", m_endMode}, {"speed_factor", m_speedFactor}, {"speed_step", m_speedStep}}},
        {"screenshots", QJsonObject{{"folder", m_screenshotFolder}, {"custom", m_screenshotCustomPath}}},
        {"update", QJsonObject{{"last_checked", m_updateLastChecked}, {"seen_version", m_updateSeenVersion}}},
    };

    QDir().mkpath(QFileInfo(m_path).absolutePath());
    // QSaveFile writes a temp file and renames it: a crash or power loss mid-write keeps the old state.
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(s).toJson()) < 0 || !f.commit())
        qWarning("AppState: could not save %s: %s", qPrintable(m_path), qPrintable(f.errorString()));
}
