#pragma once

#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

struct mpv_handle;

// Which audio tracks of the playing file are silent, for the mixer's "Microphone is silent in this clip"
// (ShadowPlay writes a mic track even when the mic was muted or the wrong device). A hidden libmpv player
// (audio only, no sound device, as fast as it decodes: a 30 s clip took ~0.6 s per track) reads one track at a
// time through astats: an audible track is left as soon as its peak passes Threshold, a silent one is read
// to the end. Starts StartDelayMs after the file is set, so it never competes with the first frame.
class SilenceCheck : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString file READ file WRITE setFile NOTIFY fileChanged)
    Q_PROPERTY(QVariantList silent READ silent NOTIFY changed) // track ids (mpv's aid), found so far
    Q_PROPERTY(bool done READ done NOTIFY changed)              // every track of the file was checked

public:
    static constexpr double Threshold = -70; // dBFS peak: inaudible even at the mixer's 400 % gain
    static constexpr int StartDelayMs = 2000;
    static constexpr int PollMs = 200;

    // No default for parent: QML would construct a default-constructible singleton itself, not call create().
    explicit SilenceCheck(QObject *parent);
    ~SilenceCheck() override;

    QString file() const { return m_file; }
    void setFile(const QString &file);
    QVariantList silent() const;
    bool done() const { return m_done; }

    static SilenceCheck *create(QQmlEngine *, QJSEngine *)
    {
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
    static void setInstance(SilenceCheck *check) { s_instance = check; } // before QML loads

signals:
    void fileChanged();
    void changed();

private slots:
    void drainEvents();

private:
    bool ensurePlayer();
    void start();
    void load(int track);
    void finishTrack(bool silent);

    static inline SilenceCheck *s_instance = nullptr;
    mpv_handle *m_mpv = nullptr;
    QString m_file;
    quint64 m_generation = 1; // +1 per file: replies about an earlier file are dropped
    bool m_starting = false;   // loadfile sent, its START_FILE not seen yet: older events are the previous file's
    QList<int> m_tracks;       // audio track ids, read when the file is first loaded
    int m_current = -1;        // index into m_tracks of the track being read; -1 before the track list is known
    bool m_atEnd = false;      // the current track was read to its end
    QList<int> m_silent;
    bool m_done = false;
    QTimer m_startTimer;
    QTimer m_poll;
};
