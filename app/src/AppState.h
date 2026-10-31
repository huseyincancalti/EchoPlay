#pragma once

#include <QQmlEngine>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
#include "Mixer.h"

// The user's choices that persist across files and restarts, in one JSON file with a schema version.
// Every setting follows load -> apply at start -> save on change (CLAUDE.md). A new setting: a member,
// its accessors, one line each in load() and save(), a test. When a saved field changes meaning, bump
// Schema and convert the old shape in load(), the only place that knows older formats.
class AppState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

public:
    static constexpr int Schema = 1;

    explicit AppState(const QString &path, QObject *parent = nullptr); // loads the file
    ~AppState() override;                                               // writes a pending save

    QString language() const { return m_language; } // empty = follow the system language
    void setLanguage(const QString &code);
    MixerPrefs mixer() const { return m_mixer; }
    void setMixer(const MixerPrefs &prefs);

    // Writes now. A change schedules this a moment later instead: a dragged slider changes a value
    // many times per second, and one write is enough.
    void save();

    static AppState *create(QQmlEngine *, QJSEngine *)
    {
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
    static void setInstance(AppState *state) { s_instance = state; } // main.cpp, before QML loads

signals:
    void languageChanged();

private:
    void load();
    void changed();

    static inline AppState *s_instance = nullptr;
    QString m_path;
    QTimer m_saveTimer;
    QString m_language;
    MixerPrefs m_mixer;
};
