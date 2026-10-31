#include "Mixer.h"

#include <QRegularExpression>
#include <QStringList>
#include <QVariantMap>
#include <algorithm>
#include <cmath>

namespace {

// Same order as v1's label(): a title from the recorder (ShadowPlay writes "System sounds",
// "Microphone"), then the language, else nothing.
QString trackName(const QVariantMap &track)
{
    const QVariantMap md = track.value("metadata").toMap();
    for (const QString &name : {track.value("title").toString(), md.value("title").toString(),
                                md.value("name").toString(), md.value("NAME").toString()}) {
        if (!name.isEmpty()) return name;
    }
    const QString lang = track.value("lang").toString();
    return lang.isEmpty() || lang == "und" ? QString() : lang.toUpper();
}

} // namespace

bool Mixer::setTracks(const QVariantList &trackList)
{
    m_tracks.clear();
    for (const QVariant &v : trackList) {
        const QVariantMap t = v.toMap();
        if (t.value("type").toString() != "audio") continue;
        m_tracks.append({t.value("id").toInt(), trackName(t), t.value("codec").toString()});
    }
    return !m_tracks.isEmpty();
}

void Mixer::applyPrefs(const MixerPrefs &prefs)
{
    m_enabled.clear();
    if (prefs.defaultOn == "all") {
        for (const Track &t : m_tracks) m_enabled.insert(t.id);
    } else if (prefs.defaultOn != "none") {
        static const QRegularExpression digits("\\d+");
        for (const auto &m : digits.globalMatch(prefs.defaultOn)) m_enabled.insert(m.captured().toInt());
    }
    m_gains = prefs.gains;
    m_mono = prefs.mono;
    if (prefs.defaultOn != "none" && enabledTracks().isEmpty() && !m_tracks.isEmpty())
        m_enabled.insert(m_tracks.first().id);
}

MixerPrefs Mixer::prefs() const
{
    MixerPrefs p;
    const QList<int> on = enabledTracks();
    if (!m_tracks.isEmpty() && on.size() == m_tracks.size()) {
        p.defaultOn = "all";
    } else if (on.isEmpty()) {
        p.defaultOn = "none";
    } else {
        QStringList ids;
        for (int id : on) ids.append(QString::number(id));
        p.defaultOn = ids.join(',');
    }
    for (auto it = m_gains.cbegin(); it != m_gains.cend(); ++it)
        if (it.value() != 1.0) p.gains.insert(it.key(), it.value());
    p.mono = m_mono;
    return p;
}

QList<int> Mixer::enabledTracks() const
{
    QList<int> on;
    for (const Track &t : m_tracks)
        if (m_enabled.contains(t.id)) on.append(t.id);
    return on;
}

void Mixer::setEnabled(int id, bool on)
{
    if (on) m_enabled.insert(id);
    else m_enabled.remove(id);
}

void Mixer::setAllEnabled(bool on)
{
    for (const Track &t : m_tracks) setEnabled(t.id, on);
}

void Mixer::setGain(int id, double gain)
{
    // Whole percent: repeated +/-10 % steps would otherwise drift (1.0 + 0.1 - 0.1 != 1.0) and save noise.
    m_gains.insert(id, std::round(std::clamp(gain, 0.0, GainMax) * 100) / 100);
    m_enabled.insert(id);
}

void Mixer::setMono(int id, bool mono)
{
    if (mono) m_mono.insert(id);
    else m_mono.remove(id);
    m_enabled.insert(id);
}

QString Mixer::chain(int id, const QString &out) const
{
    QString f = QString("[aid%1]volume=%2").arg(id).arg(gain(id), 0, 'f', 2);
    if (isMono(id)) f += ",pan=stereo|c0=0.5*c0+0.5*c1|c1=0.5*c0+0.5*c1"; // a mic recorded on one ear
    return f + out;
}

Mixer::Output Mixer::output() const
{
    const QList<int> on = enabledTracks();
    if (on.isEmpty()) return {QString(), "no"};
    // One plain track needs no filter graph: mpv plays it directly.
    if (on.size() == 1 && gain(on.first()) == 1.0 && !isMono(on.first())) return {QString(), QString::number(on.first())};
    if (on.size() == 1) return {chain(on.first(), "[ao]"), "no"};
    QStringList chains;
    QString labels;
    for (int id : on) {
        const QString label = QString("[ev%1]").arg(id);
        chains.append(chain(id, label));
        labels += label;
    }
    // normalize=0: amix would otherwise halve each input, making the mix quieter than one track alone.
    return {chains.join(';') + ';' + labels + QString("amix=inputs=%1:normalize=0[ao]").arg(on.size()), "no"};
}
