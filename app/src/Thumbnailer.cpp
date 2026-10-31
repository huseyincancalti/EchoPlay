#include "Thumbnailer.h"

#include <QQuickImageProvider>
#include <mpv/client.h>

namespace {

// reply_userdata of the hidden player's async commands: the kind, plus the file generation it belongs to
// (replies about a file that was replaced since are dropped).
enum Kind : quint64 { Seek = 1, Shot = 2 };
quint64 tag(Kind kind, quint64 generation) { return generation << 2 | kind; }

class ThumbnailProvider : public QQuickImageProvider
{
public:
    explicit ThumbnailProvider(Thumbnailer *thumbnailer)
        : QQuickImageProvider(QQuickImageProvider::Image), m_thumbnailer(thumbnailer) {}

    QImage requestImage(const QString &, QSize *size, const QSize &) override
    {
        const QImage image = m_thumbnailer->image();
        if (size) *size = image.size();
        return image;
    }

private:
    Thumbnailer *m_thumbnailer;
};

} // namespace

Thumbnailer::Thumbnailer(QObject *parent) : QObject(parent)
{
    m_settle.setSingleShot(true);
    m_settle.setInterval(SettleMs);
    connect(&m_settle, &QTimer::timeout, this, [this] {
        if (!m_busy && m_target == m_served && !m_servedExact) seek(true);
    });
}

Thumbnailer::~Thumbnailer()
{
    if (!m_mpv) return;
    mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
    mpv_terminate_destroy(m_mpv);
}

Thumbnailer *Thumbnailer::create(QQmlEngine *engine, QJSEngine *)
{
    if (!engine->imageProvider("thumbnail")) engine->addImageProvider("thumbnail", new ThumbnailProvider(s_instance));
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void Thumbnailer::setFile(const QString &file)
{
    if (file == m_file) return;
    m_file = file;
    ++m_generation;
    m_loaded = m_starting = m_busy = false;
    m_target = m_served = -1;
    m_settle.stop();
    if (m_mpv) {
        const char *stop[] = {"stop", nullptr}; // frees the old file's decoder until the next request
        mpv_command_async(m_mpv, 0, stop);
    }
    {
        QMutexLocker lock(&m_imageLock);
        m_image = QImage();
    }
    emit fileChanged();
    emit imageChanged();
}

QUrl Thumbnailer::source() const
{
    QMutexLocker lock(&m_imageLock);
    return m_image.isNull() ? QUrl() : QUrl("image://thumbnail/" + QString::number(m_serial));
}

QImage Thumbnailer::image() const
{
    QMutexLocker lock(&m_imageLock);
    return m_image;
}

void Thumbnailer::request(double seconds)
{
    if (m_file.isEmpty() || !(seconds >= 0)) return;
    m_target = seconds;
    m_settle.stop();
    if (!ensurePlayer()) return;
    if (!m_loaded) {
        // The start position is the first keyframe seek (hr-seek=no).
        const QByteArray file = m_file.toUtf8(), start = "start=" + QByteArray::number(seconds, 'f', 3);
        const char *load[] = {"loadfile", file.constData(), "replace", "-1", start.constData(), nullptr};
        mpv_command_async(m_mpv, 0, load);
        m_loaded = m_starting = m_busy = true;
        m_busyExact = m_servedExact = false;
        m_served = seconds;
        return;
    }
    if (!m_busy && seconds == m_served) {
        if (!m_servedExact) m_settle.start();
        return;
    }
    if (m_busy && !m_busyExact) return; // its picture arrives soon; then the newest request is served
    seek(false);                        // also cuts a running exact seek short
}

bool Thumbnailer::ensurePlayer()
{
    if (m_mpv) return true;
    m_mpv = mpv_create();
    if (!m_mpv) return false;
    // thumbfast's settings: no window, no audio or subtitles, a small software decode (fast, low quality is
    // fine at this size), scaled in the filter chain so grabbing the frame copies only MaxSize pixels.
    // 4 decoder threads instead of thumbfast's 2: exact seeks ~1.6x faster (measured 2026-10-31).
    const QByteArray vf = QByteArray("scale=w=%1:h=%1:force_original_aspect_ratio=decrease").replace("%1", QByteArray::number(MaxSize));
    const std::pair<const char *, const char *> options[] = {
        {"vo", "null"}, {"aid", "no"}, {"sid", "no"}, {"sub-auto", "no"}, {"audio-file-auto", "no"},
        {"idle", "yes"}, {"pause", "yes"}, {"keep-open", "always"}, {"hr-seek", "no"}, {"hwdec", "no"},
        {"vd-lavc-threads", "4"}, {"vd-lavc-skiploopfilter", "all"}, {"vd-lavc-fast", "yes"},
        {"demuxer-readahead-secs", "0"}, {"demuxer-max-bytes", "128KiB"}, {"sws-scaler", "fast-bilinear"},
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
        m_mpv, [](void *ctx) { QMetaObject::invokeMethod(static_cast<Thumbnailer *>(ctx), "drainEvents", Qt::QueuedConnection); },
        this);
    return true;
}

void Thumbnailer::seek(bool exact)
{
    m_busy = true;
    m_busyExact = m_servedExact = exact;
    m_served = m_target;
    const QByteArray time = QByteArray::number(m_target, 'f', 3);
    const char *seek[] = {"seek", time.constData(), exact ? "absolute+exact" : "absolute+keyframes", nullptr};
    mpv_command_async(m_mpv, tag(Seek, m_generation), seek);
}

void Thumbnailer::grab()
{
    const char *shot[] = {"screenshot-raw", "video", "bgra", nullptr};
    mpv_command_async(m_mpv, tag(Shot, m_generation), shot);
}

// A picture arrived (or the seek failed): serve the newest request, or refine a resting one.
void Thumbnailer::done()
{
    m_busy = false;
    if (m_target != m_served) seek(false);
    else if (!m_servedExact) m_settle.start();
}

void Thumbnailer::drainEvents()
{
    while (m_mpv) {
        mpv_event *e = mpv_wait_event(m_mpv, 0);
        if (e->event_id == MPV_EVENT_NONE) break;
        if (e->event_id == MPV_EVENT_START_FILE) {
            m_starting = false; // events before this one were about the previous file
        } else if (e->event_id == MPV_EVENT_END_FILE) {
            // The file could not be opened (or was stopped): the next request opens it again.
            if (!m_starting && m_loaded) m_loaded = m_busy = false;
        } else if (e->event_id == MPV_EVENT_PLAYBACK_RESTART) {
            // Every restart is grabbed, also one a newer seek overtook: the picture is still recent.
            if (!m_starting && m_loaded) grab();
        } else if (e->event_id == MPV_EVENT_COMMAND_REPLY) {
            if (e->reply_userdata >> 2 != m_generation) continue;
            const Kind kind = Kind(e->reply_userdata & 3);
            if (kind == Seek && e->error < 0) {
                done();
            } else if (kind == Shot) {
                if (e->error >= 0) store(static_cast<mpv_event_command *>(e->data)->result);
                done();
            }
        }
    }
}

void Thumbnailer::store(const mpv_node &result)
{
    if (result.format != MPV_FORMAT_NODE_MAP) return;
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
    if (!data || w <= 0 || h <= 0 || stride < w * 4 || qint64(data->size) < stride * h) return;
    // bgra bytes = ARGB32 on little-endian; video is opaque, so premultiplied is the same. copy(): the
    // bytes belong to mpv and are freed after this event.
    const QImage image = QImage(static_cast<const uchar *>(data->data), int(w), int(h), int(stride),
                                QImage::Format_ARGB32_Premultiplied).copy();
    {
        QMutexLocker lock(&m_imageLock);
        m_image = image;
        ++m_serial;
    }
    emit imageChanged();
}
