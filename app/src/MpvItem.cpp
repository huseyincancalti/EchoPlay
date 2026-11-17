#include "MpvItem.h"
#include "Mp4TrackNames.h"

#include <QClipboard>
#include <QDir>
#include <QGuiApplication>
#include <QPointer>
#include <QThreadPool>
#include <QQuickWindow>
#include <QStandardPaths>
#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_d3d11.h>
#include <rhi/qrhi.h>
#include <deque>
#include <stdexcept>
#include <vector>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX // a Qt header may have set it
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

constexpr quint64 KeepAwakeReply = 1; // observeProperty() uses 0
constexpr quint64 PathReply = 2;
constexpr quint64 ScreenshotReply = 2; // command() uses 0
constexpr quint64 CopyFrameReply = 3;
constexpr quint64 CaptureFrameReply = 4;
constexpr quint64 OutParamsReply = 5; // observed, like KeepAwakeReply
constexpr quint64 ShotDirReply = 6;
constexpr int RecentLogLines = 12;

// QVariant -> mpv_node (strings, numbers, bools, lists). Owns the memory the node points into;
// std::deque never moves existing elements, so pointers stay valid while it grows.
struct NodeBuilder
{
    std::deque<QByteArray> strings;
    std::deque<std::vector<mpv_node>> arrays;
    std::deque<mpv_node_list> lists;

    void build(const QVariant &v, mpv_node &n)
    {
        switch (v.typeId()) {
        case QMetaType::Bool: n.format = MPV_FORMAT_FLAG; n.u.flag = v.toBool(); break;
        case QMetaType::Int:
        case QMetaType::LongLong: n.format = MPV_FORMAT_INT64; n.u.int64 = v.toLongLong(); break;
        case QMetaType::Double: n.format = MPV_FORMAT_DOUBLE; n.u.double_ = v.toDouble(); break;
        case QMetaType::QVariantList: {
            const QVariantList src = v.toList();
            std::vector<mpv_node> &items = arrays.emplace_back(src.size());
            for (int i = 0; i < src.size(); ++i) build(src[i], items[i]);
            n.format = MPV_FORMAT_NODE_ARRAY;
            n.u.list = &lists.emplace_back(mpv_node_list{int(items.size()), items.data(), nullptr});
            break;
        }
        default:
            n.format = MPV_FORMAT_STRING;
            n.u.string = strings.emplace_back(v.toString().toUtf8()).data();
        }
    }
};

QVariant fromNode(const mpv_node &n)
{
    switch (n.format) {
    case MPV_FORMAT_STRING: return QString::fromUtf8(n.u.string);
    case MPV_FORMAT_FLAG: return bool(n.u.flag);
    case MPV_FORMAT_INT64: return qlonglong(n.u.int64);
    case MPV_FORMAT_DOUBLE: return n.u.double_;
    case MPV_FORMAT_NODE_ARRAY: {
        QVariantList out;
        for (int i = 0; i < n.u.list->num; ++i) out.append(fromNode(n.u.list->values[i]));
        return out;
    }
    case MPV_FORMAT_NODE_MAP: {
        QVariantMap out;
        for (int i = 0; i < n.u.list->num; ++i)
            out.insert(QString::fromUtf8(n.u.list->keys[i]), fromNode(n.u.list->values[i]));
        return out;
    }
    default: return {};
    }
}

// The reply to screenshot-raw: a node map {w, h, stride, format, data}. An image that owns its pixels, because the
// event's memory goes with the next mpv_wait_event. Null when the command failed or the node is not what we asked for.
QImage frameFromReply(const mpv_event &e)
{
    if (e.error < 0) return {};
    const mpv_node &result = static_cast<const mpv_event_command *>(e.data)->result;
    if (result.format != MPV_FORMAT_NODE_MAP) return {};
    qint64 w = 0, h = 0, stride = 0;
    QString format;
    const mpv_byte_array *pixels = nullptr;
    for (int i = 0; i < result.u.list->num; ++i) {
        const QByteArray key = result.u.list->keys[i];
        const mpv_node &v = result.u.list->values[i];
        if (key == "w" && v.format == MPV_FORMAT_INT64) w = v.u.int64;
        else if (key == "h" && v.format == MPV_FORMAT_INT64) h = v.u.int64;
        else if (key == "stride" && v.format == MPV_FORMAT_INT64) stride = v.u.int64;
        else if (key == "format" && v.format == MPV_FORMAT_STRING) format = QString::fromUtf8(v.u.string);
        else if (key == "data" && v.format == MPV_FORMAT_BYTE_ARRAY) pixels = v.u.ba;
    }
    // bgr0 in memory, a little-endian 0xffRRGGBB word to Qt.
    if (format != "bgr0" || !pixels || w <= 0 || h <= 0 || stride < w * 4 || qint64(pixels->size) < stride * h) return {};
    return QImage(static_cast<const uchar *>(pixels->data), int(w), int(h), qsizetype(stride), QImage::Format_RGB32).copy();
}

// Renders mpv into the item's color texture on Qt's own D3D11 device (no OpenGL driver to load,
// d3d11va decodes on the same device). Runs on Qt's render thread.
class MpvRenderer : public QQuickRhiItemRenderer
{
public:
    explicit MpvRenderer(MpvItem *item) : m_item(item) {}

    void initialize(QRhiCommandBuffer *) override
    {
        if (m_item->renderContext) return;
        if (rhi()->backend() != QRhi::D3D11) throw std::runtime_error("MpvItem needs Qt's Direct3D 11 backend");
        auto *native = static_cast<const QRhiD3D11NativeHandles *>(rhi()->nativeHandles());
        mpv_d3d11_init_params init{native->dev};
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_D3D11)},
            {MPV_RENDER_PARAM_D3D11_INIT_PARAMS, &init},
            {MPV_RENDER_PARAM_INVALID, nullptr}};
        if (mpv_render_context_create(&m_item->renderContext, m_item->handle(), params) < 0)
            throw std::runtime_error("mpv_render_context_create failed");
        mpv_render_context_set_update_callback(
            m_item->renderContext,
            [](void *ctx) { QMetaObject::invokeMethod(static_cast<MpvItem *>(ctx), "update", Qt::QueuedConnection); },
            m_item);
        QMetaObject::invokeMethod(m_item, "renderReady", Qt::QueuedConnection);
    }

    void synchronize(QQuickRhiItem *) override { m_request = std::exchange(m_item->pendingFrame, {}); }

    void render(QRhiCommandBuffer *cb) override
    {
        if (m_request.reply) renderOffScreen(cb, std::exchange(m_request, {}));
        mpv_d3d11_fbo target{reinterpret_cast<void *>(colorTexture()->nativeTexture().object)};
        mpv_render_param params[] = {{MPV_RENDER_PARAM_D3D11_FBO, &target}, {MPV_RENDER_PARAM_INVALID, nullptr}};
        // mpv draws on the immediate context and leaves its state changed; Qt flushes before and resets after.
        cb->beginExternal();
        mpv_render_context_render(m_item->renderContext, params);
        cb->endExternal();
    }

private:
    // The frame once more, into a texture of the video's size, read back as an image: the picture the screen shows,
    // tone mapping included. The screen's own frame is drawn after it, so it never shows the off-screen size.
    void renderOffScreen(QRhiCommandBuffer *cb, const MpvItem::FrameRequest &request)
    {
        QPointer<MpvItem> item(m_item);
        const quint64 reply = request.reply;
        auto fail = [item, reply] { QMetaObject::invokeMethod(item, "renderedFrame", Qt::QueuedConnection, reply, QImage()); };
        QRhiTexture *texture = rhi()->newTexture(QRhiTexture::RGBA8, request.size, 1,
                                                 QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
        if (!texture->create()) {
            delete texture;
            return fail();
        }
        mpv_d3d11_fbo target{reinterpret_cast<void *>(texture->nativeTexture().object)};
        mpv_render_param params[] = {{MPV_RENDER_PARAM_D3D11_FBO, &target}, {MPV_RENDER_PARAM_INVALID, nullptr}};
        cb->beginExternal();
        const int rendered = mpv_render_context_render(m_item->renderContext, params);
        cb->endExternal();
        if (rendered < 0) {
            texture->deleteLater();
            return fail();
        }
        auto *readback = new QRhiReadbackResult;
        readback->completed = [readback, texture, item, reply] {
            const QSize size = readback->pixelSize;
            const QImage frame = QImage(reinterpret_cast<const uchar *>(readback->data.constData()), size.width(), size.height(),
                                        QImage::Format_RGBA8888).convertToFormat(QImage::Format_RGB32); // a copy
            QMetaObject::invokeMethod(item, "renderedFrame", Qt::QueuedConnection, reply, frame);
            texture->deleteLater();
            delete readback;
        };
        QRhiResourceUpdateBatch *batch = rhi()->nextResourceUpdateBatch();
        batch->readBackTexture({texture}, readback);
        cb->resourceUpdate(batch);
    }

    MpvItem *m_item;
    MpvItem::FrameRequest m_request;
};

// mpv's own name for a screenshot (screenshot-template's default, mpv-shot%n): the first free number.
QString nextShotPath(const QString &dir)
{
    for (int n = 1;; ++n) {
        const QString path = QDir(dir).filePath(QString("mpv-shot%1.png").arg(n, 4, 10, QChar('0')));
        if (!QFileInfo::exists(path)) return path;
    }
}

} // namespace

MpvItem::MpvItem(QQuickItem *parent) : QQuickRhiItem(parent)
{
    m_marks.insert("item_ms", processAgeMsNow());
    m_mpv = mpv_create();
    if (!m_mpv) throw std::runtime_error("mpv_create failed");
    m_marks.insert("mpv_created_ms", processAgeMsNow());
    mpv_set_option_string(m_mpv, "vo", "libmpv");
    // libmpv has no cache folder of its own, so it compiled every video shader again on each start (~150 ms
    // before the first frame, 2026-10-31). v1's mpv keeps them in its cache folder; so does EchoPlay now.
    const QString shaders = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/mpv-shaders";
    QDir().mkpath(shaders);
    mpv_set_option_string(m_mpv, "gpu-shader-cache-dir", shaders.toUtf8().constData());
    // mpv's built-in Lua scripts (console, stats, ytdl, ...) are uosc-era tooling the Qt UI replaces;
    // skipping them saves startup time. Older/newer option names are both set; unknown ones are ignored.
    for (const char *opt : {"ytdl", "load-stats-overlay", "load-console", "load-osd-console", "load-auto-profiles",
                            "load-select", "load-positioning", "load-commands"})
        mpv_set_option_string(m_mpv, opt, "no");
    mpv_set_wakeup_callback(
        m_mpv, [](void *ctx) { QMetaObject::invokeMethod(static_cast<MpvItem *>(ctx), "drainEvents", Qt::QueuedConnection); },
        this);
}

MpvItem::~MpvItem()
{
    setKeepingAwake(false);
    if (renderContext) mpv_render_context_free(renderContext);
    mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
    mpv_terminate_destroy(m_mpv);
}

QQuickRhiItemRenderer *MpvItem::createRenderer()
{
    window()->setPersistentGraphics(true);
    window()->setPersistentSceneGraph(true);
    return new MpvRenderer(this);
}

void MpvItem::setOption(const QString &name, const QString &value)
{
    mpv_set_option_string(m_mpv, name.toUtf8().constData(), value.toUtf8().constData());
}

void MpvItem::start()
{
    if (mpv_initialize(m_mpv) < 0) throw std::runtime_error("mpv_initialize failed");
    // core-idle: paused, or no file. Its own reply id keeps it out of propertyChanged/observed().
    mpv_request_log_messages(m_mpv, "warn");
    mpv_observe_property(m_mpv, KeepAwakeReply, "core-idle", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, PathReply, "path", MPV_FORMAT_STRING); // for track names mpv cannot read
    mpv_observe_property(m_mpv, OutParamsReply, "video-out-params", MPV_FORMAT_NODE); // HDR screenshots
    mpv_observe_property(m_mpv, ShotDirReply, "screenshot-dir", MPV_FORMAT_STRING);
}

// The same flags as mpv's own window (video/out/w32_common.c). Per thread: always the GUI thread here.
void MpvItem::setKeepingAwake(bool on)
{
    if (on == m_keepingAwake) return;
    m_keepingAwake = on;
#ifdef Q_OS_WIN
    SetThreadExecutionState(on ? ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED : ES_CONTINUOUS);
#endif
    emit keepingAwakeChanged();
}

void MpvItem::command(const QVariantList &args)
{
    NodeBuilder b;
    mpv_node node;
    b.build(args, node);
    mpv_command_node_async(m_mpv, 0, &node);
}

// mpv's screenshots of an HDR10 or HLG video convert it in software, which does not tone-map: Copy frame clipped the
// highlights, F1 wrote a PQ-coded PNG that viewers show pale (Jellyfin's HDR10 test clip, 2026-11-17: PSNR against the
// window 16.4 and 13.5 dB). Such a frame comes from the renderer instead, as the screen shows it. That picture always
// has the subtitles shown on screen: for HDR "video" and "window" take it too, at the video's size.
bool MpvItem::renderHdrFrame(quint64 reply)
{
    if (!m_hdr || m_videoSize.isEmpty() || !renderContext) return false;
    pendingFrame = {reply, m_videoSize};
    update();
    return true;
}

void MpvItem::renderedFrame(quint64 reply, const QImage &frame)
{
    if (reply != ScreenshotReply) return frameReady(reply, frame);
    if (frame.isNull()) return emit screenshotFinished(false);
    const QString dir = m_screenshotDir;
    QPointer<MpvItem> self(this);
    QThreadPool::globalInstance()->start([self, dir, frame] { // a 4K PNG takes a while
        const bool saved = QDir().mkpath(dir.isEmpty() ? "." : dir) && frame.save(nextShotPath(dir), "PNG");
        QMetaObject::invokeMethod(self, [self, saved] { if (self) emit self->screenshotFinished(saved); }, Qt::QueuedConnection);
    });
}

void MpvItem::frameReady(quint64 reply, const QImage &frame)
{
    if (reply == CopyFrameReply) {
        if (!frame.isNull()) QGuiApplication::clipboard()->setImage(frame);
        emit frameCopied(!frame.isNull());
    } else if (reply == CaptureFrameReply) {
        emit frameCaptured(frame);
    }
}

void MpvItem::screenshot(const QString &mode)
{
    if (renderHdrFrame(ScreenshotReply)) return;
    NodeBuilder b;
    mpv_node node;
    b.build(QVariantList{"screenshot", mode}, node);
    mpv_command_node_async(m_mpv, ScreenshotReply, &node);
}

// Asynchronous like screenshot(): the frame waits for the video output, which waits for the render thread.
void MpvItem::requestFrame(quint64 reply)
{
    if (renderHdrFrame(reply)) return;
    NodeBuilder b;
    mpv_node node;
    b.build(QVariantList{"screenshot-raw", "subtitles", "bgr0"}, node);
    mpv_command_node_async(m_mpv, reply, &node);
}

void MpvItem::copyFrame() { requestFrame(CopyFrameReply); }

void MpvItem::captureFrame() { requestFrame(CaptureFrameReply); }

QVariant MpvItem::getProperty(const QString &name) const
{
    mpv_node node;
    if (mpv_get_property(m_mpv, name.toUtf8().constData(), MPV_FORMAT_NODE, &node) < 0) return {};
    QVariant out = fromNode(node);
    mpv_free_node_contents(&node);
    return out;
}

void MpvItem::setProperty(const QString &name, const QVariant &value)
{
    NodeBuilder b;
    mpv_node node;
    b.build(value, node);
    mpv_set_property_async(m_mpv, 0, name.toUtf8().constData(), MPV_FORMAT_NODE, &node);
}

void MpvItem::observeProperty(const QString &name)
{
    if (m_observedNames.contains(name)) return; // mpv would send every change twice
    m_observedNames.insert(name);
    mpv_observe_property(m_mpv, 0, name.toUtf8().constData(), MPV_FORMAT_NODE);
}

qint64 MpvItem::processAgeMsNow()
{
#ifdef Q_OS_WIN
    FILETIME created, exited, kernel, user, now;
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) return -1;
    GetSystemTimePreciseAsFileTime(&now);
    auto ticks = [](const FILETIME &f) { return (qint64(f.dwHighDateTime) << 32) | f.dwLowDateTime; };
    return (ticks(now) - ticks(created)) / 10000; // 100 ns units -> ms
#else
    return -1;
#endif
}

void MpvItem::drainEvents()
{
    while (true) {
        mpv_event *e = mpv_wait_event(m_mpv, 0);
        if (e->event_id == MPV_EVENT_NONE) break;
        if (e->event_id == MPV_EVENT_PLAYBACK_RESTART) {
            m_recentLog.clear(); // it plays: what came before says nothing about a later failure. Not at START_FILE, which
            emit playbackRestarted(); // a client reads after the log messages that follow it
        }
        if (e->event_id == MPV_EVENT_START_FILE) emit fileStarted();
        if (e->event_id == MPV_EVENT_COMMAND_REPLY && e->reply_userdata == ScreenshotReply) emit screenshotFinished(e->error >= 0);
        if (e->event_id == MPV_EVENT_COMMAND_REPLY && (e->reply_userdata == CopyFrameReply || e->reply_userdata == CaptureFrameReply))
            frameReady(e->reply_userdata, frameFromReply(*e));
        if (e->event_id == MPV_EVENT_LOG_MESSAGE) {
            const auto *message = static_cast<mpv_event_log_message *>(e->data);
            m_recentLog << QString("[%1] %2").arg(message->prefix, QString::fromUtf8(message->text).trimmed());
            if (m_recentLog.size() > RecentLogLines) m_recentLog.removeFirst();
        }
        if (e->event_id == MPV_EVENT_END_FILE) {
            const auto *end = static_cast<mpv_event_end_file *>(e->data);
            switch (end->reason) {
            case MPV_END_FILE_REASON_EOF: emit fileEnded("eof", {}, end->playlist_entry_id); break;
            case MPV_END_FILE_REASON_STOP: emit fileEnded("stop", {}, end->playlist_entry_id); break;
            case MPV_END_FILE_REASON_QUIT: emit fileEnded("quit", {}, end->playlist_entry_id); break;
            case MPV_END_FILE_REASON_ERROR: emit fileEnded("error", QString::fromUtf8(mpv_error_string(end->error)), end->playlist_entry_id); break;
            case MPV_END_FILE_REASON_REDIRECT: emit fileEnded("redirect", {}, end->playlist_entry_id); break;
            }
        }
        if (e->event_id == MPV_EVENT_PROPERTY_CHANGE && e->reply_userdata == KeepAwakeReply) {
            const auto *p = static_cast<mpv_event_property *>(e->data);
            setKeepingAwake(p->format == MPV_FORMAT_FLAG && !*static_cast<int *>(p->data));
        } else if (e->event_id == MPV_EVENT_PROPERTY_CHANGE && e->reply_userdata == OutParamsReply) {
            const auto *p = static_cast<mpv_event_property *>(e->data);
            const QVariantMap params = p->format == MPV_FORMAT_NODE ? fromNode(*static_cast<mpv_node *>(p->data)).toMap() : QVariantMap();
            const QString gamma = params.value("gamma").toString();
            m_hdr = gamma == "pq" || gamma == "hlg";
            m_videoSize = QSize(params.value("dw").toInt(), params.value("dh").toInt());
            if (params.value("rotate").toInt() % 180 == 90) m_videoSize.transpose();
        } else if (e->event_id == MPV_EVENT_PROPERTY_CHANGE && e->reply_userdata == ShotDirReply) {
            const auto *p = static_cast<mpv_event_property *>(e->data);
            m_screenshotDir = p->format == MPV_FORMAT_STRING ? QString::fromUtf8(*static_cast<char **>(p->data)) : QString();
        } else if (e->event_id == MPV_EVENT_PROPERTY_CHANGE && e->reply_userdata == PathReply) {
            const auto *p = static_cast<mpv_event_property *>(e->data);
            const QString path = p->format == MPV_FORMAT_STRING ? QString::fromUtf8(*static_cast<char **>(p->data)) : QString();
            if (path == m_namesPath) continue;
            m_namesPath = path;
            m_names = path.isEmpty() || path.contains("://") ? QVariantMap() : readTrackNames(path);
            // The track list may have come first: send it again with the names.
            if (m_observed.contains("track-list")) {
                const QVariant tracks = withNames(m_rawTrackList);
                m_observed.insert("track-list", tracks);
                emit propertyChanged("track-list", tracks);
            }
        } else if (e->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            const auto *p = static_cast<mpv_event_property *>(e->data);
            QVariant value = p->format == MPV_FORMAT_NODE ? fromNode(*static_cast<mpv_node *>(p->data)) : QVariant();
            const QString name = QString::fromUtf8(p->name);
            if (name == "track-list") {
                m_rawTrackList = value;
                value = withNames(value);
            }
            m_observed.insert(name, value);
            emit propertyChanged(name, value);
        }
    }
}

QVariantMap MpvItem::readTrackNames(const QString &path)
{
    const Mp4TrackNames names = Mp4TrackNames::read(path);
    return {{"audio", names.audio}, {"sub", names.subtitles}};
}

// A track without a title gets the name its file keeps where mpv does not look (Mp4TrackNames): the n-th audio track
// the n-th audio name. External subtitle files are not in the container: they are skipped.
QVariant MpvItem::withNames(const QVariant &trackList) const
{
    if (m_names.isEmpty()) return trackList;
    QVariantList tracks = trackList.toList();
    QHash<QString, int> seen;
    for (QVariant &entry : tracks) {
        QVariantMap track = entry.toMap();
        const QString type = track.value("type").toString();
        if (type != "audio" && type != "sub") continue;
        if (track.value("external").toBool()) continue;
        const QStringList names = m_names.value(type).toStringList();
        const int index = seen[type]++;
        if (track.value("title").toString().isEmpty() && index < names.size() && !names[index].isEmpty()) {
            track.insert("title", names[index]);
            entry = track;
        }
    }
    return tracks;
}
