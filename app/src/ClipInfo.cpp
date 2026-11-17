#include "ClipInfo.h"

#include <QQuickImageProvider>
#include <cstring>
#include <mpv/client.h>
#include "HdrTone.h"

namespace {

// reply_userdata of the hidden player's async requests: the kind, plus the clip it belongs to (replies about
// an earlier clip are dropped). The kinds are bits: ClipInfo::m_got collects the ones that arrived.
enum Kind : quint64 { Duration = 1, Tracks = 2, Shot = 4, AllKinds = 7 };
quint64 tag(Kind kind, quint64 generation) { return generation << 3 | kind; }

class ClipInfoProvider : public QQuickImageProvider
{
public:
    explicit ClipInfoProvider(ClipInfo *clipInfo)
        : QQuickImageProvider(QQuickImageProvider::Image), m_clipInfo(clipInfo) {}

    QImage requestImage(const QString &id, QSize *size, const QSize &) override
    {
        const QImage image = m_clipInfo->image(id.toInt());
        if (size) *size = image.size();
        return image;
    }

private:
    ClipInfo *m_clipInfo;
};

const mpv_node *find(const mpv_node &map, const char *key)
{
    if (map.format != MPV_FORMAT_NODE_MAP) return nullptr;
    for (int i = 0; i < map.u.list->num; ++i)
        if (std::strcmp(map.u.list->keys[i], key) == 0) return &map.u.list->values[i];
    return nullptr;
}

int audioTracksIn(const mpv_node &trackList)
{
    int count = 0;
    for (int i = 0; trackList.format == MPV_FORMAT_NODE_ARRAY && i < trackList.u.list->num; ++i) {
        const mpv_node *type = find(trackList.u.list->values[i], "type");
        if (type && type->format == MPV_FORMAT_STRING && std::strcmp(type->u.string, "audio") == 0) ++count;
    }
    return count;
}

// The screenshot-raw result as a picture; null when it is not a usable one.
QImage pictureIn(const mpv_node &result)
{
    if (result.format != MPV_FORMAT_NODE_MAP) return {};
    qint64 w = 0, h = 0, stride = 0;
    const mpv_byte_array *data = nullptr;
    for (int i = 0; i < result.u.list->num; ++i) {
        const QByteArray key = result.u.list->keys[i];
        const mpv_node &v = result.u.list->values[i];
        if (key == "w") w = v.u.int64;
        else if (key == "h") h = v.u.int64;
        else if (key == "stride") stride = v.u.int64;
        else if (key == "data" && v.format == MPV_FORMAT_BYTE_ARRAY) data = v.u.ba;
    }
    if (!data || w <= 0 || h <= 0 || stride < w * 4 || qint64(data->size) < stride * h) return {};
    // bgra bytes = ARGB32 on little-endian; video is opaque, so premultiplied is the same. copy(): the
    // bytes belong to mpv and are freed after this event.
    return QImage(static_cast<const uchar *>(data->data), int(w), int(h), int(stride), QImage::Format_ARGB32_Premultiplied)
        .copy();
}

} // namespace

ClipInfo::ClipInfo(QObject *parent) : QObject(parent)
{
    m_idle.setSingleShot(true);
    m_idle.setInterval(IdleMs);
    connect(&m_idle, &QTimer::timeout, this, &ClipInfo::unload);
}

ClipInfo::~ClipInfo()
{
    if (!m_mpv) return;
    mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
    mpv_terminate_destroy(m_mpv);
}

ClipInfo *ClipInfo::create(QQmlEngine *engine, QJSEngine *)
{
    if (!engine->imageProvider("clipinfo")) engine->addImageProvider("clipinfo", new ClipInfoProvider(s_instance));
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void ClipInfo::request(const QStringList &paths)
{
    m_queue.clear();
    for (const QString &path : paths)
        if (!path.isEmpty() && path != m_reading && !m_clips.contains(path) && !m_queue.contains(path)) m_queue.append(path);
    if (m_reading.isEmpty()) startNext();
}

QVariantMap ClipInfo::info(const QString &path) const
{
    const auto it = m_clips.constFind(path);
    if (it == m_clips.cend()) return {};
    QVariantMap map{{"duration", it->duration}, {"audioTracks", it->audioTracks}};
    if (it->image) map["thumbnail"] = "image://clipinfo/" + QString::number(it->image);
    return map;
}

QImage ClipInfo::image(int id) const
{
    QMutexLocker lock(&m_imageLock);
    return m_images.value(id);
}

QByteArray ClipInfo::scaleFilter()
{
    return "scale=w=" + QByteArray::number(ThumbWidth) + ":h=-2"; // -2: even height, same aspect
}

bool ClipInfo::ensurePlayer()
{
    if (m_mpv) return true;
    m_mpv = mpv_create();
    if (!m_mpv) return false;
    // Thumbnailer's settings: no window, no sound device (ao=null, paused: nothing plays. Not aid=no, with which a
    // file without video does not open at all, and an opened audio file makes a playlist of audio files), a software
    // decode, scaled in the filter chain so grabbing the frame copies only ThumbWidth pixels. hr-seek=no: the start
    // position is a keyframe seek. Unlike a seek-bar preview this picture stays on screen, so the scaler is
    // mpv's default, not the fastest. No keep-open: a clip that ends before its picture is grabbed sends
    // END_FILE and the queue goes on.
    const QByteArray vf = scaleFilter();
    const std::pair<const char *, const char *> options[] = {
        {"vo", "null"}, {"ao", "null"}, {"sid", "no"}, {"sub-auto", "no"}, {"audio-file-auto", "no"},
        {"idle", "yes"}, {"pause", "yes"}, {"hr-seek", "no"}, {"hwdec", "no"}, {"vd-lavc-threads", "4"},
        {"demuxer-readahead-secs", "0"}, {"demuxer-max-bytes", "128KiB"},
        {"vf", vf.constData()}, {"ytdl", "no"}, {"load-scripts", "no"}, {"input-default-bindings", "no"},
        {"load-stats-overlay", "no"}, {"load-console", "no"}, {"load-osd-console", "no"},
        {"load-auto-profiles", "no"}, {"load-select", "no"}, {"load-positioning", "no"}, {"load-commands", "no"}};
    for (const auto &[name, value] : options) mpv_set_option_string(m_mpv, name, value);
    if (mpv_initialize(m_mpv) < 0) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return false;
    }
    mpv_set_wakeup_callback(
        m_mpv, [](void *ctx) { QMetaObject::invokeMethod(static_cast<ClipInfo *>(ctx), "drainEvents", Qt::QueuedConnection); },
        this);
    return true;
}

// Opens the first queued clip, or, with the queue empty, starts the countdown to closing the file.
void ClipInfo::startNext()
{
    if (m_queue.isEmpty()) {
        m_idle.start();
        return;
    }
    if (!ensurePlayer()) {
        m_queue.clear(); // no player: nothing can be read; the next request tries to create it again
        return;
    }
    m_idle.stop();
    m_reading = m_queue.takeFirst();
    m_clip = {};
    m_picture = QImage();
    m_got = 0;
    m_grabbed = false;
    if (m_toned) mpv_set_property_string(m_mpv, "vf", scaleFilter().constData()); // the last clip was HDR
    m_toned = m_toneChecked = false;
    m_starting = true;
    ++m_generation;
    const QByteArray file = m_reading.toUtf8();
    const char *load[] = {"loadfile", file.constData(), "replace", "-1", "start=10%", nullptr};
    mpv_command_async(m_mpv, 0, load);
}

// The clip has all its answers, or cannot give more (it failed to open, or ended): keep what is known.
void ClipInfo::finish()
{
    if (!m_picture.isNull()) {
        QMutexLocker lock(&m_imageLock);
        m_clip.image = ++m_lastImage;
        m_images.insert(m_clip.image, m_picture);
    }
    m_clips.insert(m_reading, m_clip);
    m_reading.clear();
    m_picture = QImage();
    ++m_revision;
    emit changed();
    startNext();
}

// Closes the file in the hidden player, which frees its demuxer and decoder; the next clip opens a new one.
void ClipInfo::unload()
{
    if (!m_mpv) return;
    const char *stop[] = {"stop", nullptr};
    mpv_command_async(m_mpv, 0, stop);
}

void ClipInfo::drainEvents()
{
    while (m_mpv) {
        mpv_event *e = mpv_wait_event(m_mpv, 0);
        if (e->event_id == MPV_EVENT_NONE) break;
        if (e->event_id == MPV_EVENT_START_FILE) {
            m_starting = false; // events before this one were about the previous clip
        } else if (m_starting || m_reading.isEmpty()) {
            continue;
        } else if (e->event_id == MPV_EVENT_END_FILE) {
            // Our own replace or stop is not an end: only a clip that failed to open, or ran out first, is.
            const auto *end = static_cast<mpv_event_end_file *>(e->data);
            if (end->reason == MPV_END_FILE_REASON_ERROR || end->reason == MPV_END_FILE_REASON_EOF) finish();
        } else if (e->event_id == MPV_EVENT_FILE_LOADED) {
            mpv_get_property_async(m_mpv, tag(Duration, m_generation), "duration", MPV_FORMAT_DOUBLE);
            mpv_get_property_async(m_mpv, tag(Tracks, m_generation), "track-list", MPV_FORMAT_NODE);
        } else if (e->event_id == MPV_EVENT_PLAYBACK_RESTART) {
            // The seek to 10 % is done (this is the clip's only restart: it starts there). A file without video
            // answers with an error, and has no picture.
            if (m_grabbed) continue;
            if (!m_toneChecked) { // HDR: the tone-mapping filter goes on, and the picture comes from the seek it needs
                m_toneChecked = true;
                if (HdrTone::isHdr(m_mpv)) {
                    m_toned = true;
                    mpv_set_property_string(m_mpv, "vf", HdrTone::filter(scaleFilter()).constData());
                    const char *seek[] = {"seek", "10", "absolute-percent", nullptr};
                    mpv_command_async(m_mpv, 0, seek);
                    continue;
                }
            }
            m_grabbed = true;
            const char *shot[] = {"screenshot-raw", "video", "bgra", nullptr};
            mpv_command_async(m_mpv, tag(Shot, m_generation), shot);
        } else if (e->event_id == MPV_EVENT_GET_PROPERTY_REPLY || e->event_id == MPV_EVENT_COMMAND_REPLY) {
            if (e->reply_userdata >> 3 != m_generation) continue;
            const Kind kind = Kind(e->reply_userdata & AllKinds);
            if (e->event_id == MPV_EVENT_COMMAND_REPLY) { // loadfile and stop carry tag 0: never a clip's generation
                if (e->error >= 0) m_picture = pictureIn(static_cast<mpv_event_command *>(e->data)->result);
            } else {
                const auto *p = static_cast<mpv_event_property *>(e->data);
                if (e->error >= 0 && kind == Duration && p->format == MPV_FORMAT_DOUBLE) {
                    const double seconds = *static_cast<double *>(p->data);
                    if (seconds > 0) m_clip.duration = seconds; // also rules out NaN
                } else if (e->error >= 0 && kind == Tracks && p->format == MPV_FORMAT_NODE) {
                    m_clip.audioTracks = audioTracksIn(*static_cast<mpv_node *>(p->data));
                }
            }
            m_got |= kind;
            if (m_got == AllKinds) finish();
        }
    }
}
