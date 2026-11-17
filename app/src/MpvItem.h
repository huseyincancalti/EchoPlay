#pragma once

#include <QImage>
#include <QQuickRhiItem>
#include <QSet>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

struct mpv_handle;
struct mpv_render_context;

// A QML item that shows libmpv's video through the render API, inside the Qt Quick scene,
// so other items (glass panels) can sample and blur it. Windows: Direct3D 11 on Qt's own device.
class MpvItem : public QQuickRhiItem
{
    Q_OBJECT
    QML_ELEMENT
    // The display and the PC stay awake while a file plays (not paused): mpv's own window does this
    // (stop-screensaver), but with the render API the window is ours.
    Q_PROPERTY(bool keepingAwake READ keepingAwake NOTIFY keepingAwakeChanged)

public:
    explicit MpvItem(QQuickItem *parent = nullptr);
    ~MpvItem() override;

    Q_INVOKABLE void command(const QVariantList &args);
    Q_INVOKABLE void screenshot(const QString &mode); // "subtitles", "video", "window"; screenshotFinished says how it went
    Q_INVOKABLE void copyFrame();                     // the current frame, subtitles on it, onto the clipboard; frameCopied says how it went
    void captureFrame();                              // the same request, but frameCaptured gets the image and the clipboard is left alone (the end-to-end test)
    Q_INVOKABLE void setOption(const QString &name, const QString &value); // before start() only
    Q_INVOKABLE void start();                                              // mpv_initialize
    // Synchronous: waits for mpv's core. Avoid it while playing. A GUI thread blocked on the core stalls Qt's
    // render loop, and mpv's VO waits for that render: seen 2026-10-31 as d3d11va failing with E_PENDING and
    // falling back to software decoding. UI code reads observed() values instead.
    Q_INVOKABLE QVariant getProperty(const QString &name) const;
    Q_INVOKABLE void setProperty(const QString &name, const QVariant &value);
    // propertyChanged now and on every change. A name already observed just keeps going: read observed() for its value.
    Q_INVOKABLE void observeProperty(const QString &name);
    // The last value of an observed property; never blocks.
    Q_INVOKABLE QVariant observed(const QString &name) const { return m_observed.value(name); }
    Q_INVOKABLE qint64 processAgeMs() const { return processAgeMsNow(); }
    // mpv's last warnings and errors, for the error report.
    Q_INVOKABLE QStringList recentLog() const { return m_recentLog; }
    static qint64 processAgeMsNow(); // ms since the OS created this process; -1 if unknown
    Q_INVOKABLE QVariantMap startupMarks() const { return m_marks; } // item_ms, mpv_created_ms

    bool keepingAwake() const { return m_keepingAwake; }
    mpv_handle *handle() const { return m_mpv; }
    mpv_render_context *renderContext = nullptr;

signals:
    void renderReady(); // render context exists: only now can a file with video be loaded
    void playbackRestarted();
    void fileStarted();
    // mpv's end-file reason: eof, stop, quit, error, redirect. With "error", why (mpv_error_string, English:
    // "unrecognized file format", "loading failed", ...) for the "This file can't be opened" state. entryId is its
    // id in mpv's playlist: mpv's path never shows a file that fails at once.
    void fileEnded(const QString &reason, const QString &error, qint64 entryId);
    void propertyChanged(const QString &name, const QVariant &value); // null value: property unavailable
    void keepingAwakeChanged();
    void screenshotFinished(bool saved);
    void frameCopied(bool ok);
    void frameCaptured(const QImage &image); // null when mpv could not make the frame

protected:
    QQuickRhiItemRenderer *createRenderer() override;

private slots:
    void drainEvents();

private:
    void setKeepingAwake(bool on);
    void requestFrame(quint64 reply);

    mpv_handle *m_mpv = nullptr;
    bool m_keepingAwake = false;
    QSet<QString> m_observedNames;
    QVariantMap m_observed;
    QVariant m_rawTrackList;   // mpv's own, before withNames()
    QString m_namesPath;       // the file m_names belong to
    QVariantMap m_names;       // {"audio": [...], "sub": [...]} from Mp4TrackNames
    static QVariantMap readTrackNames(const QString &path);
    QVariant withNames(const QVariant &trackList) const;
    QVariantMap m_marks;
    QStringList m_recentLog;
};
