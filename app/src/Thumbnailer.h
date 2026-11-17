#pragma once

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

struct mpv_handle;
struct mpv_node;

// Seek-bar preview pictures (v1 used thumbfast): a second, hidden libmpv player (no window, no audio, no
// GPU decoder: the one playing the video keeps it) opens the same file on the first request, seeks to the
// time under the pointer and grabs that frame, scaled down; IdleMs after the last request it closes the
// file again. As thumbfast does, a request is first served by a keyframe seek and, once the pointer rests,
// by an exact seek. Measured 2026-10-31 (tools/thumbs-bench.sh, medians): 1080p H.264 open 66 ms, keyframe
// 48 ms, exact 0.2 s, +48 MB while open; 4K HEVC with 4 s between keyframes open 121 ms, keyframe 167 ms,
// exact 2 s (max 4.1 s), +246 MB while open. Requests that come in while a keyframe seek runs collapse into
// the newest; a new request cuts a running exact seek short.
// QML shows `source` (image://thumbnail/<n>); the picture is at most MaxSize px on its longer side.
class Thumbnailer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString file READ file WRITE setFile NOTIFY fileChanged)
    Q_PROPERTY(QUrl source READ source NOTIFY imageChanged) // empty until the file's first picture

public:
    static constexpr int MaxSize = 360;   // physical px: ~288 px wide at 125% display scaling
    static constexpr int SettleMs = 150; // the pointer rested this long on a time: exact seek
    static constexpr int IdleMs = 5000;  // no request this long: the hidden player closes the file (memory)

    // No default for parent: QML would construct a default-constructible singleton itself, not call create().
    explicit Thumbnailer(QObject *parent);
    ~Thumbnailer() override;

    QString file() const { return m_file; }
    void setFile(const QString &file); // the playing file; the hidden player opens it on the first request
    QUrl source() const;
    QImage image() const; // the newest picture; any thread (the image provider)

    Q_INVOKABLE void request(double seconds);

    // Also registers the image provider behind `source` with the engine that asks for the singleton.
    static Thumbnailer *create(QQmlEngine *engine, QJSEngine *);
    static void setInstance(Thumbnailer *thumbnailer) { s_instance = thumbnailer; } // before QML loads

signals:
    void fileChanged();
    void imageChanged();

private slots:
    void drainEvents();
    void unload();

private:
    static QByteArray scaleFilter();
    bool ensurePlayer();
    void seek(bool exact);
    void grab();
    void done();
    void store(const mpv_node &result);

    static inline Thumbnailer *s_instance = nullptr;
    mpv_handle *m_mpv = nullptr;
    QString m_file;
    quint64 m_generation = 1; // +1 per file: replies about an earlier file are dropped
    bool m_loaded = false;   // m_file is open (or opening) in the hidden player
    bool m_starting = false; // loadfile sent, its START_FILE not seen yet: older events are the previous file's
    bool m_busy = false;     // a seek (or the load) runs; its picture has not arrived yet
    bool m_busyExact = false;
    double m_target = -1;    // the newest request
    double m_served = -1;    // the time of the last seek started
    bool m_servedExact = false;
    bool m_toneChecked = false; // this file's first frame was looked at for HDR (HdrTone.h)
    bool m_toned = false;       // the tone-mapping filter is on
    QTimer m_settle;
    QTimer m_idle;
    mutable QMutex m_imageLock;
    QImage m_image;
    int m_serial = 0;
};
