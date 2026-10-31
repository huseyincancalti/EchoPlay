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

void AppState::setMixer(const MixerPrefs &prefs)
{
    if (prefs == m_mixer) return;
    m_mixer = prefs;
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
        {"mixer", QJsonObject{{"default_on", m_mixer.defaultOn}, {"gains", gains}, {"mono", monoIds}}},
    };

    QDir().mkpath(QFileInfo(m_path).absolutePath());
    // QSaveFile writes a temp file and renames it: a crash or power loss mid-write keeps the old state.
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(s).toJson()) < 0 || !f.commit())
        qWarning("AppState: could not save %s: %s", qPrintable(m_path), qPrintable(f.errorString()));
}
