#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class AppState;

// v1's picture-quality tiers (mpv/scripts/echoplay-audio/quality.lua): Ultra, Normal, Low, or Auto. Auto
// starts at Normal, steps down after ~6 s of sustained frame drops, steps back up after a clean stretch
// (Ultra only after ~30 s), and never re-enters a tier that already dropped frames on the current file.
// No libmpv here: QML feeds frame-drop-count every PollMs and applies applyRequested's mpv properties.
class PictureQuality : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString pref READ pref WRITE setPref NOTIFY changed)  // "auto", "ultra", "normal", "low"
    Q_PROPERTY(QString active READ active NOTIFY changed)            // the tier in effect
    Q_PROPERTY(int pollMs READ pollMs CONSTANT)

public:
    explicit PictureQuality(AppState *state, QObject *parent = nullptr);

    QString pref() const { return m_pref; }
    void setPref(const QString &pref); // saved; a tier name applies that tier at once
    QString active() const { return m_active; }
    int pollMs() const { return 2000; } // the streak lengths below count ticks of this interval

    // The mpv properties of a tier. Every tier sets the same keys, so switching never needs a restore step.
    static QVariantMap options(const QString &tier);
    Q_INVOKABLE QVariantMap activeOptions() const { return options(m_active); } // set before the first file

    Q_INVOKABLE void fileStarted(); // a new file costs differently: Auto may try every tier again
    Q_INVOKABLE void tick(double frameDropCount);

    static PictureQuality *create(QQmlEngine *, QJSEngine *)
    {
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
    static void setInstance(PictureQuality *quality) { s_instance = quality; } // main.cpp, before QML loads

signals:
    void changed();
    void applyRequested(const QVariantMap &options);

private:
    void setActive(const QString &tier);

    static inline PictureQuality *s_instance = nullptr;
    AppState *m_state;
    QString m_pref;
    QString m_active;
    double m_lastCount = -1; // -1: the next tick only sets the baseline
    int m_badStreak = 0;
    int m_goodStreak = 0;
    QSet<QString> m_blocked; // tiers that dropped frames on this file
};
