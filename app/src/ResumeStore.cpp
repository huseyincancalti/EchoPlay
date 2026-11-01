#include "ResumeStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>
#include <vector>

namespace {

constexpr double MinResumePos = 15;  // seconds; before this there is nothing worth resuming
constexpr double FinishedMargin = 20; // seconds from the end; after this the file counts as watched

// One key per file: Explorer hands over C:\Videos\a.mp4, a second launch C:/Videos/a.mp4. URLs stay as they are.
QString key(const QString &path)
{
    return path.contains("://") ? path : QDir::cleanPath(QDir::fromNativeSeparators(path));
}

} // namespace

ResumeStore::ResumeStore(const QString &path, QObject *parent) : QObject(parent), m_path(path)
{
    load();
}

ResumeStore::~ResumeStore()
{
    if (m_dirty) save();
}

void ResumeStore::update(const QString &path, double pos, double duration)
{
    if (duration <= 0) return;
    if (pos < MinResumePos || pos > duration - FinishedMargin) {
        if (m_entries.remove(key(path))) m_dirty = true;
        return;
    }
    m_entries.insert(key(path), {pos, duration, QDateTime::currentSecsSinceEpoch()});
    m_dirty = true;
}

double ResumeStore::position(const QString &path) const
{
    const auto it = m_entries.constFind(key(path));
    return it == m_entries.constEnd() ? -1 : it->pos;
}

QDateTime ResumeStore::savedAt(const QString &path) const
{
    const auto it = m_entries.constFind(key(path));
    return it == m_entries.constEnd() ? QDateTime() : QDateTime::fromSecsSinceEpoch(it->at);
}

// Each entry is read only when it has the expected shape; a bad one is skipped without a word.
void ResumeStore::load()
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly)) return; // first run
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (!doc.isObject()) {
        qWarning("ResumeStore: %s is not a JSON object (%s), starting empty", qPrintable(m_path),
                 qPrintable(err.errorString()));
        return;
    }
    const QJsonObject entries = doc.object().value("entries").toObject();
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        const QJsonObject e = it.value().toObject();
        if (!e.value("pos").isDouble() || !e.value("dur").isDouble() || !e.value("at").isDouble()) continue;
        const Entry entry{e.value("pos").toDouble(), e.value("dur").toDouble(), e.value("at").toInteger()};
        if (entry.pos >= 0 && entry.dur > 0) m_entries.insert(key(it.key()), entry);
    }
}

void ResumeStore::save()
{
    if (m_entries.size() > MaxEntries) {
        std::vector<std::pair<qint64, QString>> byAge; // (at, path); the path breaks ties, so the cut is stable
        byAge.reserve(m_entries.size());
        for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) byAge.emplace_back(it->at, it.key());
        std::sort(byAge.begin(), byAge.end());
        for (size_t i = 0; i < byAge.size() - MaxEntries; ++i) m_entries.remove(byAge[i].second);
    }

    QJsonObject entries;
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it)
        entries.insert(it.key(), QJsonObject{{"pos", it->pos}, {"dur", it->dur}, {"at", it->at}});
    const QJsonObject root{{"schema", Schema}, {"entries", entries}};

    QDir().mkpath(QFileInfo(m_path).absolutePath());
    // QSaveFile writes a temp file and renames it: a crash or power loss mid-write keeps the old file.
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) < 0 ||
        !f.commit()) {
        qWarning("ResumeStore: could not save %s: %s", qPrintable(m_path), qPrintable(f.errorString()));
        return;
    }
    m_dirty = false;
}
