#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
#include "Mixer.h"

class AppState;

// The mixer as QML sees it: feeds mpv's track-list into Mixer once per file (with the saved prefs), saves
// every user change to AppState, and asks QML to apply the result to mpv (outputChanged). No libmpv here,
// so the whole flow is unit-tested; QML only forwards mpv's events and properties.
class MixerController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY tracksChanged) // [{id, name, codec, enabled, gain, mono}]

public:
    explicit MixerController(AppState *state, QObject *parent = nullptr);

    QVariantList tracks() const;

    // mpv's start-file: forget the previous file's tracks and drop its filter graph, which names track ids
    // the next file may not have.
    Q_INVOKABLE void fileStarted();
    // mpv's track-list. The first list with audio tracks applies the saved prefs; later ones are ignored, so a
    // track-list update mid-file never undoes the user's choice. Early lists without audio (seen in v1 right
    // after file-loaded) are skipped until the audio tracks show up.
    Q_INVOKABLE void setTrackList(const QVariantList &trackList);

    Q_INVOKABLE void toggle(int id);
    Q_INVOKABLE void setAllEnabled(bool on);
    Q_INVOKABLE void setGain(int id, double gain);
    Q_INVOKABLE void setMono(int id, bool mono);

    static MixerController *create(QQmlEngine *, QJSEngine *)
    {
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
    static void setInstance(MixerController *mixer) { s_instance = mixer; } // main.cpp, before QML loads

signals:
    // Set lavfi-complex first, then aid (v1's order: the video is never re-initialized). An empty aid
    // means "leave aid alone".
    void outputChanged(const QString &lavfiComplex, const QString &aid);
    void tracksChanged();

private:
    void userChanged();
    void emitOutput();

    static inline MixerController *s_instance = nullptr;
    AppState *m_state;
    Mixer m_mixer;
    bool m_applied = false; // prefs applied for the current file
    Mixer::Output m_last;
};
