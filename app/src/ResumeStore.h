#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QtQml/qqmlregistration.h>

// Where the user stopped watching each file. Playback always starts at 0:00; when a file has a saved
// spot, the UI asks "Resume from 1:24?". Lives in its own resume.json, not state.json: it grows with every
// file watched. Keyed by path, with / and \ treated alike. Loaded once in the constructor; an unreadable or
// damaged file, or a bad entry, is skipped.
class ResumeStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static constexpr int Schema = 1;
    static constexpr int MaxEntries = 300;

    explicit ResumeStore(const QString &path, QObject *parent = nullptr); // loads the file
    ~ResumeStore() override;                                               // writes pending changes

    // Playback reached `pos` of `duration` seconds in `path`. Near the start (nothing to resume) or near
    // the end (finished) drops the entry instead; an unknown duration is ignored.
    Q_INVOKABLE void update(const QString &path, double pos, double duration);
    // The saved position in seconds, or -1 when there is none.
    Q_INVOKABLE double position(const QString &path) const;
    // When that position was saved, invalid when there is none: "You stopped here on Oct 30."
    Q_INVOKABLE QDateTime savedAt(const QString &path) const;
    // Keeps the newest MaxEntries and writes now. The UI calls it when a file ends.
    Q_INVOKABLE void save();

    static ResumeStore *create(QQmlEngine *, QJSEngine *)
    {
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
    static void setInstance(ResumeStore *store) { s_instance = store; } // main.cpp, before QML loads

private:
    struct Entry
    {
        double pos = 0;
        double dur = 0;
        qint64 at = 0; // seconds since the epoch
    };

    void load();

    static inline ResumeStore *s_instance = nullptr;
    QString m_path;
    QHash<QString, Entry> m_entries;
    bool m_dirty = false;
};
