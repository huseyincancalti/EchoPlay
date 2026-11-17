#include "CutSaver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <mpv/client.h>
#include <algorithm>
#include <cmath>

namespace {
// 95 s -> "1.35", 3725 s -> "1.02.05": a time in a file name (Windows forbids ':').
QString nameTime(double seconds)
{
    const qint64 s = qint64(std::floor(std::max(0.0, seconds)));
    const QString ms = QString("%1.%2").arg(s / 60 % 60).arg(s % 60, 2, 10, QChar('0'));
    return s >= 3600 ? QString("%1.%2").arg(s / 3600).arg(ms.rightJustified(5, '0')) : ms;
}
} // namespace

CutSaver::CutSaver(QObject *parent) : QObject(parent) {}

CutSaver::~CutSaver() { cancel(); }

QString CutSaver::outputPath(const QString &file, double start, double end)
{
    const QFileInfo source(file);
    const QString stem = source.dir().filePath(source.completeBaseName() + ' ' + nameTime(start) + '-' + nameTime(end));
    QString path = stem + ".mp4";
    for (int n = 2; QFileInfo::exists(path); ++n) path = QString("%1 (%2).mp4").arg(stem).arg(n);
    return path;
}

bool CutSaver::save(const QString &file, double start, double end, const QString &lavfiComplex, const QString &aid)
{
    if (m_state == "saving" || end <= start) return false;
    m_start = start;
    m_end = end;
    m_output = outputPath(file, start, end);
    m_error.clear();
    m_progress = 0;
    m_mpv = mpv_create();
    if (!m_mpv) {
        finish("failed", "mpv_create failed");
        return true;
    }
    const QByteArray out = QDir::toNativeSeparators(m_output).toUtf8(), from = QByteArray::number(start, 'f', 3),
                     to = QByteArray::number(end, 'f', 3), graph = lavfiComplex.toUtf8(),
                     track = (aid.isEmpty() ? QString("no") : aid).toUtf8();
    const std::pair<const char *, const char *> options[] = {
        {"o", out.constData()}, {"ovc", "libx264"}, {"ovcopts", "crf=18,preset=veryfast"}, {"oac", "aac"},
        {"oacopts", "b=192k"}, {"start", from.constData()}, {"end", to.constData()}, {"idle", "no"},
        // The same mix as the player: the graph names [aidN] inputs; without a graph aid picks the one track.
        {"lavfi-complex", graph.constData()}, {"aid", graph.isEmpty() ? track.constData() : "auto"},
        {"sid", "no"}, {"sub-auto", "no"}, {"audio-file-auto", "no"}, {"ytdl", "no"}, {"load-scripts", "no"},
        {"input-default-bindings", "no"}, {"load-stats-overlay", "no"}, {"load-console", "no"},
        {"load-osd-console", "no"}, {"load-auto-profiles", "no"}, {"load-select", "no"}, {"load-positioning", "no"},
        {"load-commands", "no"}};
    for (const auto &[name, value] : options) mpv_set_option_string(m_mpv, name, value);
    if (mpv_initialize(m_mpv) < 0) {
        finish("failed", "mpv_initialize failed");
        return true;
    }
    mpv_request_log_messages(m_mpv, "error");
    mpv_observe_property(m_mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_set_wakeup_callback(
        m_mpv, [](void *ctx) { QMetaObject::invokeMethod(static_cast<CutSaver *>(ctx), "drainEvents", Qt::QueuedConnection); },
        this);
    const QByteArray path = file.toUtf8();
    const char *load[] = {"loadfile", path.constData(), nullptr};
    mpv_command_async(m_mpv, 0, load);
    m_state = "saving";
    emit stateChanged();
    emit progressChanged();
    return true;
}

void CutSaver::drainEvents()
{
    while (m_mpv) {
        mpv_event *e = mpv_wait_event(m_mpv, 0);
        if (e->event_id == MPV_EVENT_NONE) break;
        if (e->event_id == MPV_EVENT_LOG_MESSAGE) {
            m_error = QString::fromUtf8(static_cast<mpv_event_log_message *>(e->data)->text).trimmed(); // the last one
        } else if (e->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            const auto *p = static_cast<mpv_event_property *>(e->data);
            if (p->format != MPV_FORMAT_DOUBLE) continue;
            const double done = std::clamp((*static_cast<double *>(p->data) - m_start) / (m_end - m_start), 0.0, 1.0);
            if (done > m_progress) {
                m_progress = done;
                emit progressChanged();
            }
        } else if (e->event_id == MPV_EVENT_END_FILE) {
            const auto *end = static_cast<mpv_event_end_file *>(e->data);
            const QString why = end->reason == MPV_END_FILE_REASON_ERROR ? QString::fromUtf8(mpv_error_string(end->error)) : QString();
            finish("done", why);
        }
    }
}

// mpv_terminate_destroy also writes the file's index (encode mode finishes the file on quit).
void CutSaver::finish(const QString &state, const QString &error)
{
    if (m_mpv) mpv_terminate_destroy(m_mpv);
    m_mpv = nullptr;
    QString why = error;
    if (state == "done" && why.isEmpty() && QFileInfo(m_output).size() <= 0) why = m_error.isEmpty() ? "no output" : m_error;
    if (!why.isEmpty()) {
        QFile::remove(m_output);
        m_state = "failed";
        m_error = why;
    } else {
        m_state = state;
        if (state == "done") m_error.clear();
    }
    m_progress = m_state == "done" ? 1 : m_progress;
    emit progressChanged();
    emit stateChanged();
}

void CutSaver::cancel()
{
    if (m_state != "saving") return;
    mpv_terminate_destroy(m_mpv);
    m_mpv = nullptr;
    QFile::remove(m_output);
    m_state = "idle";
    emit stateChanged();
}

void CutSaver::dismiss()
{
    if (m_state == "saving" || m_state == "idle") return;
    m_state = "idle";
    emit stateChanged();
}
