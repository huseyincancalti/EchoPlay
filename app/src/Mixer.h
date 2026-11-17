#pragma once

#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QVariantList>

// What the mixer remembers across files and restarts. Gains and mono are keyed by track id, so
// "track 2 = mic at 150 %" carries over to the next clip from the same recorder.
struct MixerPrefs
{
    QString defaultOn = QStringLiteral("1"); // "all", "none" or track ids "1,3"
    QMap<int, double> gains;                 // only gains other than 1.0
    QSet<int> mono;

    bool operator==(const MixerPrefs &o) const
    {
        return defaultOn == o.defaultOn && gains == o.gains && mono == o.mono;
    }
};

// The live track mixer, ported from v1 (mpv/scripts/echoplay-audio/main.lua: apply_audio, track_chain,
// apply_audio_defaults). No libmpv here: the caller feeds mpv's track-list and applies output() to mpv,
// lavfi-complex first, then aid (v1's order; switching this way never re-initializes the video).
class Mixer
{
public:
    struct Track
    {
        int id = 0;
        QString name; // from the file; empty when it has none (the UI shows "Track N" then)
        QString codec;
        QString lang; // the language's own name (I18n::languageName); empty when the file has none
    };
    struct Output
    {
        QString lavfiComplex; // empty = no filter graph
        QString aid;          // "no" or a track id
    };

    static constexpr double GainMax = 4.0;

    // mpv's track-list property; keeps the audio tracks. False when there are none yet: mpv can report
    // a track-list without the audio tracks for the first moment after file-loaded (seen live in v1).
    bool setTracks(const QVariantList &trackList);
    // Once per file, after setTracks() found tracks: enables what the prefs say, at least one track
    // unless the prefs say "none".
    void applyPrefs(const MixerPrefs &prefs);
    // The current choice in the shape applyPrefs() takes, to save after a user change.
    MixerPrefs prefs() const;

    const QList<Track> &tracks() const { return m_tracks; }
    QList<int> enabledTracks() const; // in track order
    bool isEnabled(int id) const { return m_enabled.contains(id); }
    double gain(int id) const { return m_gains.value(id, 1.0); }
    bool isMono(int id) const { return m_mono.contains(id); }

    void setEnabled(int id, bool on);
    void setAllEnabled(bool on);
    void setGain(int id, double gain); // clamped to 0..GainMax, rounded to whole percent; enables the track
    void setMono(int id, bool mono);   // enables the track

    Output output() const;

private:
    QString chain(int id, const QString &out) const;

    QList<Track> m_tracks;
    QSet<int> m_enabled; // may hold ids this file does not have (prefs "1,3" on a 2-track clip), like v1
    QMap<int, double> m_gains;
    QSet<int> m_mono;
};
