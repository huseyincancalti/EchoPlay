#include "MpvItem.h"

#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickWindow>
#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <deque>
#include <stdexcept>
#include <vector>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

void *getProcAddress(void *, const char *name)
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    return ctx ? reinterpret_cast<void *>(ctx->getProcAddress(QByteArray(name))) : nullptr;
}

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

class MpvRenderer : public QQuickFramebufferObject::Renderer
{
public:
    explicit MpvRenderer(MpvItem *item) : m_item(item) {}

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        if (!m_item->renderContext) {
            mpv_opengl_init_params gl{getProcAddress, nullptr};
            mpv_render_param params[] = {
                {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
                {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl},
                {MPV_RENDER_PARAM_INVALID, nullptr}};
            if (mpv_render_context_create(&m_item->renderContext, m_item->handle(), params) < 0)
                throw std::runtime_error("mpv_render_context_create failed");
            mpv_render_context_set_update_callback(
                m_item->renderContext,
                [](void *ctx) { QMetaObject::invokeMethod(static_cast<MpvItem *>(ctx), "update", Qt::QueuedConnection); },
                m_item);
            QMetaObject::invokeMethod(m_item, "renderReady", Qt::QueuedConnection);
        }
        return QQuickFramebufferObject::Renderer::createFramebufferObject(size);
    }

    void render() override
    {
        QOpenGLFramebufferObject *fbo = framebufferObject();
        mpv_opengl_fbo target{int(fbo->handle()), fbo->width(), fbo->height(), 0};
        int flipY = 0;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &target},
            {MPV_RENDER_PARAM_FLIP_Y, &flipY},
            {MPV_RENDER_PARAM_INVALID, nullptr}};
        m_item->window()->beginExternalCommands();
        mpv_render_context_render(m_item->renderContext, params);
        m_item->window()->endExternalCommands();
    }

private:
    MpvItem *m_item;
};

} // namespace

MpvItem::MpvItem(QQuickItem *parent) : QQuickFramebufferObject(parent), m_mpv(mpv_create())
{
    if (!m_mpv) throw std::runtime_error("mpv_create failed");
    mpv_set_option_string(m_mpv, "vo", "libmpv");
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
    if (renderContext) mpv_render_context_free(renderContext);
    mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
    mpv_terminate_destroy(m_mpv);
}

QQuickFramebufferObject::Renderer *MpvItem::createRenderer() const
{
    window()->setPersistentGraphics(true);
    window()->setPersistentSceneGraph(true);
    return new MpvRenderer(const_cast<MpvItem *>(this));
}

void MpvItem::setOption(const QString &name, const QString &value)
{
    mpv_set_option_string(m_mpv, name.toUtf8().constData(), value.toUtf8().constData());
}

void MpvItem::start()
{
    if (mpv_initialize(m_mpv) < 0) throw std::runtime_error("mpv_initialize failed");
}

void MpvItem::command(const QVariantList &args)
{
    NodeBuilder b;
    mpv_node node;
    b.build(args, node);
    mpv_command_node_async(m_mpv, 0, &node);
}

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
        if (e->event_id == MPV_EVENT_PLAYBACK_RESTART) emit playbackRestarted();
        if (e->event_id == MPV_EVENT_END_FILE) emit fileEnded();
    }
}
