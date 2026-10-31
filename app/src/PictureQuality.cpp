#include "PictureQuality.h"

#include <QStringList>
#include <algorithm>
#include "AppState.h"

namespace {

const QStringList Tiers{"ultra", "normal", "low"}; // best first

// v1's thresholds, in ticks of pollMs (2 s).
constexpr double DropThreshold = 6;  // frames dropped in one tick that count as a bad tick
constexpr int BadStreakNeeded = 3;   // ~6 s of drops before stepping down
constexpr int GoodStreakNeeded = 5;  // ~10 s clean before stepping up
constexpr int UltraStreakNeeded = 15; // ~30 s clean before trying the expensive Ultra tier

} // namespace

PictureQuality::PictureQuality(AppState *state, QObject *parent)
    : QObject(parent), m_state(state), m_pref(state->quality())
{
    m_active = m_pref == "auto" ? "normal" : m_pref;
}

QVariantMap PictureQuality::options(const QString &tier)
{
    // Normal is the tuned v1 baseline; the last four keys are mpv's defaults, set explicitly so that leaving
    // another tier restores them.
    QVariantMap o{{"scale", "lanczos"}, {"cscale", "lanczos"}, {"dscale", "mitchell"}, {"correct-downscaling", "yes"},
                  {"linear-downscaling", "yes"}, {"sigmoid-upscaling", "yes"}, {"dither", "fruit"}};
    if (tier == "ultra") {
        o["scale"] = o["cscale"] = "ewa_lanczossharp";
    } else if (tier == "low") { // for weak GPUs: the cheapest scalers, no extra passes
        o["scale"] = o["cscale"] = o["dscale"] = "bilinear";
        o["correct-downscaling"] = o["sigmoid-upscaling"] = "no";
        o["dither"] = "no";
    }
    return o;
}

void PictureQuality::setPref(const QString &pref)
{
    if (pref == m_pref || (pref != "auto" && !Tiers.contains(pref))) return;
    m_pref = pref;
    m_state->setQuality(pref);
    m_badStreak = m_goodStreak = 0;
    if (pref != "auto") setActive(pref);
    emit changed();
}

void PictureQuality::fileStarted()
{
    m_lastCount = -1;
    m_badStreak = m_goodStreak = 0;
    m_blocked.clear();
}

void PictureQuality::tick(double frameDropCount)
{
    if (m_pref != "auto") return;
    if (m_lastCount < 0) {
        m_lastCount = frameDropCount;
        return;
    }
    // mpv resets the counter on a new file and on every audio-chain rebuild (a mixer switch).
    const double dropped = std::max(0.0, frameDropCount - m_lastCount);
    m_lastCount = frameDropCount;
    const int i = Tiers.indexOf(m_active);
    if (dropped >= DropThreshold) {
        m_goodStreak = 0;
        if (++m_badStreak >= BadStreakNeeded && i + 1 < Tiers.size()) {
            m_badStreak = 0;
            m_blocked.insert(m_active); // too heavy here: Auto does not flap back into it on this file
            setActive(Tiers[i + 1]);
        }
        return;
    }
    m_badStreak = 0;
    const QString up = i > 0 ? Tiers[i - 1] : QString();
    if (up.isEmpty() || m_blocked.contains(up)) return;
    if (++m_goodStreak >= (up == "ultra" ? UltraStreakNeeded : GoodStreakNeeded)) {
        m_goodStreak = 0;
        setActive(up);
    }
}

void PictureQuality::setActive(const QString &tier)
{
    if (tier == m_active) return;
    m_active = tier;
    emit applyRequested(options(tier));
    emit changed();
}
