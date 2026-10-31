#include "MixerController.h"

#include <QVariantMap>
#include "AppState.h"

MixerController::MixerController(AppState *state, QObject *parent) : QObject(parent), m_state(state)
{
}

QVariantList MixerController::tracks() const
{
    QVariantList out;
    if (!m_applied) return out;
    for (const Mixer::Track &t : m_mixer.tracks()) {
        out.append(QVariantMap{{"id", t.id}, {"name", t.name}, {"codec", t.codec}, {"enabled", m_mixer.isEnabled(t.id)},
                               {"gain", m_mixer.gain(t.id)}, {"mono", m_mixer.isMono(t.id)}});
    }
    return out;
}

void MixerController::fileStarted()
{
    m_applied = false;
    m_last = {};
    emit outputChanged(QString(), QString());
    emit tracksChanged();
}

void MixerController::setTrackList(const QVariantList &trackList)
{
    if (m_applied || !m_mixer.setTracks(trackList)) return;
    m_mixer.applyPrefs(m_state->mixer());
    m_applied = true;
    emitOutput();
    emit tracksChanged();
}

void MixerController::toggle(int id)
{
    if (!m_applied) return;
    m_mixer.setEnabled(id, !m_mixer.isEnabled(id));
    userChanged();
}

void MixerController::setAllEnabled(bool on)
{
    if (!m_applied) return;
    m_mixer.setAllEnabled(on);
    userChanged();
}

void MixerController::setGain(int id, double gain)
{
    if (!m_applied) return;
    m_mixer.setGain(id, gain);
    userChanged();
}

void MixerController::setMono(int id, bool mono)
{
    if (!m_applied) return;
    m_mixer.setMono(id, mono);
    userChanged();
}

void MixerController::userChanged()
{
    m_state->setMixer(m_mixer.prefs());
    emitOutput();
    emit tracksChanged();
}

// Only when the result differs: every lavfi-complex write rebuilds mpv's audio chain (a short gap).
void MixerController::emitOutput()
{
    const Mixer::Output out = m_mixer.output();
    if (out.lavfiComplex == m_last.lavfiComplex && out.aid == m_last.aid) return;
    m_last = out;
    emit outputChanged(out.lavfiComplex, out.aid);
}
