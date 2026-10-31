#include "SilenceCheck.h"

#include <cmath>
#include <cstring>
#include <mpv/client.h>

namespace {

// reply_userdata of the hidden player's async requests: the kind, plus the load it belongs to (replies about
// an earlier file or track are dropped).
enum Kind : quint64 { TrackList = 1, AtEnd = 2, Peak = 3 };
quint64 tag(Kind kind, quint64 generation) { return generation << 2 | kind; }

const mpv_node *find(const mpv_node &map, const char *key)
{
    if (map.format != MPV_FORMAT_NODE_MAP) return nullptr;
    for (int i = 0; i < map.u.list->num; ++i)
        if (std::strcmp(map.u.list->keys[i], key) == 0) return &map.u.list->values[i];
    return nullptr;
}

// af-metadata/probe: astats' running totals as strings; "-inf" is digital silence. NaN when not there yet.
double peakOf(const mpv_node &metadata)
{
    const mpv_node *v = find(metadata, "lavfi.astats.Overall.Peak_level");
    if (!v || v->format != MPV_FORMAT_STRING) return NAN;
    if (std::strcmp(v->u.string, "-inf") == 0) return -INFINITY;
    bool ok = false;
    const double peak = QByteArray(v->u.string).toDouble(&ok);
    return ok ? peak : NAN;
}

} // namespace

SilenceCheck::SilenceCheck(QObject *parent) : QObject(parent)
{
    m_startTimer.setSingleShot(true);
    m_startTimer.setInterval(StartDelayMs);
    connect(&m_startTimer, &QTimer::timeout, this, &SilenceCheck::start);
    m_poll.setInterval(PollMs);
    // eof-reached first: replies come back in order, so a peak read after the end was seen is the final one.
    connect(&m_poll, &QTimer::timeout, this, [this] {
        mpv_get_property_async(m_mpv, tag(AtEnd, m_generation), "eof-reached", MPV_FORMAT_FLAG);
        mpv_get_property_async(m_mpv, tag(Peak, m_generation), "af-metadata/probe", MPV_FORMAT_NODE);
    });
}

SilenceCheck::~SilenceCheck()
{
    if (!m_mpv) return;
    mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
    mpv_terminate_destroy(m_mpv);
}

void SilenceCheck::setFile(const QString &file)
{
    if (file == m_file) return;
    m_file = file;
    ++m_generation;
    // Events still queued about the previous file (FILE_LOADED, END_FILE) are skipped until the next load's
    // START_FILE; taken for this file, they could finish it before it is checked.
    m_starting = m_mpv != nullptr;
    m_done = false;
    m_tracks.clear();
    m_current = -1;
    m_silent.clear();
    m_poll.stop();
    m_startTimer.stop();
    if (m_mpv) {
        const char *stop[] = {"stop", nullptr};
        mpv_command_async(m_mpv, 0, stop);
    }
    if (!file.isEmpty()) m_startTimer.start();
    emit fileChanged();
    emit changed();
}

QVariantList SilenceCheck::silent() const
{
    QVariantList out;
    for (int id : m_silent) out.append(id);
    return out;
}

bool SilenceCheck::ensurePlayer()
{
    if (m_mpv) return true;
    m_mpv = mpv_create();
    if (!m_mpv) return false;
    // Audio only, into no device, untimed: decoding runs as fast as the CPU allows. astats keeps running
    // totals (reset=0) and hands them out as af-metadata, which keep-open keeps readable at the end;
    // keep-open-pause=no, or the pause at one track's end carries over and the next track never plays.
    const std::pair<const char *, const char *> options[] = {
        {"vo", "null"}, {"vid", "no"}, {"sid", "no"}, {"ao", "null"}, {"ao-null-untimed", "yes"},
        {"af", "@probe:lavfi=[astats=metadata=1:reset=0]"}, {"keep-open", "yes"}, {"keep-open-pause", "no"},
        {"idle", "yes"},
        {"cache", "no"}, {"audio-file-auto", "no"}, {"sub-auto", "no"}, {"ytdl", "no"}, {"load-scripts", "no"},
        {"input-default-bindings", "no"}, {"load-stats-overlay", "no"}, {"load-console", "no"},
        {"load-osd-console", "no"}, {"load-auto-profiles", "no"}, {"load-select", "no"},
        {"load-positioning", "no"}, {"load-commands", "no"}};
    for (const auto &[name, value] : options) mpv_set_option_string(m_mpv, name, value);
    if (mpv_initialize(m_mpv) < 0) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return false;
    }
    mpv_set_wakeup_callback(
        m_mpv, [](void *ctx) { QMetaObject::invokeMethod(static_cast<SilenceCheck *>(ctx), "drainEvents", Qt::QueuedConnection); },
        this);
    return true;
}

void SilenceCheck::start()
{
    if (m_file.isEmpty() || !ensurePlayer()) return;
    load(-1); // only for the track list, read on FILE_LOADED
}

// Opens the file again for track m_tracks[index], so astats starts from zero. Index -1: mpv's own pick;
// aid=no would end the file at once with "nothing to play" (no video either), before the track list is read.
void SilenceCheck::load(int index)
{
    ++m_generation;
    m_current = index;
    m_starting = true;
    m_atEnd = false;
    m_poll.stop();
    const QByteArray file = m_file.toUtf8();
    const QByteArray aid = "aid=" + (index < 0 ? QByteArray("auto") : QByteArray::number(m_tracks[index]));
    const char *load[] = {"loadfile", file.constData(), "replace", "-1", aid.constData(), nullptr};
    mpv_command_async(m_mpv, 0, load);
}

void SilenceCheck::finishTrack(bool silent)
{
    m_poll.stop();
    if (silent) {
        m_silent.append(m_tracks[m_current]);
        emit changed();
    }
    if (m_current + 1 < m_tracks.size()) {
        load(m_current + 1);
        return;
    }
    ++m_generation;
    m_done = true;
    const char *stop[] = {"stop", nullptr}; // frees the demuxer and decoder
    mpv_command_async(m_mpv, 0, stop);
    emit changed();
}

void SilenceCheck::drainEvents()
{
    while (m_mpv) {
        mpv_event *e = mpv_wait_event(m_mpv, 0);
        if (e->event_id == MPV_EVENT_NONE) break;
        if (e->event_id == MPV_EVENT_START_FILE) {
            m_starting = false; // events before this one were about the previous load
        } else if (m_starting || m_done || m_file.isEmpty()) {
            continue;
        } else if (e->event_id == MPV_EVENT_END_FILE) {
            const auto *end = static_cast<mpv_event_end_file *>(e->data);
            if (end->reason == MPV_END_FILE_REASON_ERROR) { // cannot be read: nothing to report
                ++m_generation;
                m_poll.stop();
                m_done = true;
                emit changed();
            }
        } else if (e->event_id == MPV_EVENT_FILE_LOADED) {
            if (m_current < 0) mpv_get_property_async(m_mpv, tag(TrackList, m_generation), "track-list", MPV_FORMAT_NODE);
            else m_poll.start();
        } else if (e->event_id == MPV_EVENT_GET_PROPERTY_REPLY) {
            if (e->reply_userdata >> 2 != m_generation) continue;
            const Kind kind = Kind(e->reply_userdata & 3);
            const auto *p = static_cast<mpv_event_property *>(e->data);
            if (kind == AtEnd) {
                m_atEnd = e->error >= 0 && p->format == MPV_FORMAT_FLAG && *static_cast<int *>(p->data);
                continue;
            }
            const mpv_node *node = e->error >= 0 && p->format == MPV_FORMAT_NODE ? static_cast<mpv_node *>(p->data) : nullptr;
            if (kind == TrackList) {
                for (int i = 0; node && node->format == MPV_FORMAT_NODE_ARRAY && i < node->u.list->num; ++i) {
                    const mpv_node &track = node->u.list->values[i];
                    const mpv_node *type = find(track, "type"), *id = find(track, "id");
                    if (type && type->format == MPV_FORMAT_STRING && std::strcmp(type->u.string, "audio") == 0 && id &&
                        id->format == MPV_FORMAT_INT64)
                        m_tracks.append(int(id->u.int64));
                }
                if (m_tracks.isEmpty()) {
                    m_current = 0;
                    finishTrack(false); // no audio: done
                } else {
                    load(0);
                }
            } else {
                const double peak = node ? peakOf(*node) : NAN;
                // A final peak that cannot be read is not reported as silent: the warning must be sure.
                if (peak > Threshold) finishTrack(false);
                else if (m_atEnd) finishTrack(peak <= Threshold);
            }
        }
    }
}
