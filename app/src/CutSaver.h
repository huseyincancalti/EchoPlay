#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QtQml/qqmlregistration.h>

struct mpv_handle;

// Saves a part of a clip (the A-B loop) with the mix the person hears, baked into one audio track: a second, hidden
// libmpv in encode mode (`o=`), not a bundled ffmpeg (no install growth, a frame-exact cut). mpv has no video stream
// copy, so the picture is re-encoded: libx264 crf 18 veryfast works on every PC (measured 2026-11-17: a 10 s 4K60 HEVC
// part 14 s, h264_nvenc 11 s at 2.5x the size). The file lands next to the source: "<name> 1.05-1.35.mp4".
// One save at a time; closing EchoPlay or Cancel stops it and removes the half-written file.
class CutSaver : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)       // "idle", "saving", "done", "failed"
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged) // 0..1 while saving
    Q_PROPERTY(QString output READ output NOTIFY stateChanged)     // the file being written or written
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)       // mpv's reason after "failed"

public:
    explicit CutSaver(QObject *parent); // no default: QML would construct the singleton itself
    ~CutSaver() override;

    QString state() const { return m_state; }
    double progress() const { return m_progress; }
    QString output() const { return m_output; }
    QString error() const { return m_error; }

    // lavfiComplex and aid are the playing mpv's: what the person hears. False when a save already runs.
    Q_INVOKABLE bool save(const QString &file, double start, double end, const QString &lavfiComplex, const QString &aid);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void dismiss(); // "done" or "failed" back to "idle"

    // "<dir>/<name> 1.05-1.35.mp4", " (2)" and on when that exists. Pure: unit-tested.
    static QString outputPath(const QString &file, double start, double end);

    static CutSaver *create(QQmlEngine *, QJSEngine *)
    {
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
    static void setInstance(CutSaver *saver) { s_instance = saver; }

signals:
    void stateChanged();
    void progressChanged();

private slots:
    void drainEvents();

private:
    void finish(const QString &state, const QString &error);

    static inline CutSaver *s_instance = nullptr;
    mpv_handle *m_mpv = nullptr;
    QString m_state = QStringLiteral("idle");
    double m_progress = 0;
    QString m_output, m_error;
    double m_start = 0, m_end = 0;
};
