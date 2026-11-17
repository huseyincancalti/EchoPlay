#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

struct mpv_handle;

// What the playlist panel shows per clip: a picture, the duration and the number of audio tracks. All of it
// needs the file opened, so a hidden libmpv player (as Thumbnailer's: no window, no audio, software decode)
// reads the requested clips one at a time, in the background, and never touches the player that plays the
// video. A clip is opened paused straight at 10 % of its duration (by keyframe; a fade-in from black would
// make every picture at 0 % black), its picture grabbed, scaled to ThumbWidth. Results stay in memory for the
// life of the process, keyed by the path as given. A clip that cannot be read keeps what is known (duration 0,
// no picture) and is not tried again. IdleMs after the queue runs empty the hidden player closes the file.
// QML calls info(path) again whenever `revision` changes; the picture is image://clipinfo/<n>.
class ClipInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int revision READ revision NOTIFY changed) // goes up with every clip read: QML re-reads info()

public:
    static constexpr int ThumbWidth = 192; // physical px: the row shows 96 px, up to 2x display scaling
    static constexpr int IdleMs = 5000;    // queue empty this long: the hidden player closes the file (memory)

    // No default for parent: QML would construct a default-constructible singleton itself, not call create().
    explicit ClipInfo(QObject *parent);
    ~ClipInfo() override;

    // Replaces the queue with these paths, in this order, skipping the ones already read (or being read now).
    // Called with the rows on screen; a clip that is being read when this arrives is finished first.
    Q_INVOKABLE void request(const QStringList &paths);
    // {duration: seconds (double), audioTracks: int, thumbnail: "image://clipinfo/<n>"}; an empty map until the
    // clip is read. No thumbnail key when there is no picture.
    Q_INVOKABLE QVariantMap info(const QString &path) const;
    int revision() const { return m_revision; }
    QImage image(int id) const; // for the image provider; any thread

    // Also registers the image provider behind `thumbnail` with the engine that asks for the singleton.
    static ClipInfo *create(QQmlEngine *engine, QJSEngine *);
    static void setInstance(ClipInfo *clipInfo) { s_instance = clipInfo; } // before QML loads

signals:
    void changed();

private slots:
    void drainEvents();
    void unload();

private:
    struct Clip
    {
        double duration = 0;
        int audioTracks = 0;
        int image = 0; // id for image(); 0: no picture
    };

    static QByteArray scaleFilter();
    bool ensurePlayer();
    void startNext();
    void finish();

    static inline ClipInfo *s_instance = nullptr;
    mpv_handle *m_mpv = nullptr;
    QStringList m_queue;
    QString m_reading;        // the clip being read; empty: none
    quint64 m_generation = 1; // +1 per clip: replies about an earlier one are dropped
    bool m_starting = false;  // loadfile sent, its START_FILE not seen yet: older events are the previous clip's
    bool m_grabbed = false;   // the picture was asked for
    bool m_toneChecked = false; // this clip's first frame was looked at for HDR (HdrTone.h)
    bool m_toned = false;       // the tone-mapping filter is on
    unsigned m_got = 0;       // which of the clip's three answers (duration, tracks, picture) have arrived
    Clip m_clip;              // what is known of the clip being read
    QImage m_picture;
    QHash<QString, Clip> m_clips;
    int m_revision = 0;
    QTimer m_idle;
    mutable QMutex m_imageLock;
    QHash<int, QImage> m_images; // by Clip::image
    int m_lastImage = 0;
};
