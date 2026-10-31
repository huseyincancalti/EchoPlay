#pragma once

#include <QQuickRhiItem>
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

public:
    explicit MpvItem(QQuickItem *parent = nullptr);
    ~MpvItem() override;

    Q_INVOKABLE void command(const QVariantList &args);
    Q_INVOKABLE void setOption(const QString &name, const QString &value); // before start() only
    Q_INVOKABLE void start();                                              // mpv_initialize
    // Synchronous: waits for mpv's core. Avoid it while playing. A GUI thread blocked on the core stalls Qt's
    // render loop, and mpv's VO waits for that render: seen 2026-10-31 as d3d11va failing with E_PENDING and
    // falling back to software decoding. UI code reads observed() values instead.
    Q_INVOKABLE QVariant getProperty(const QString &name) const;
    Q_INVOKABLE void setProperty(const QString &name, const QVariant &value);
    Q_INVOKABLE void observeProperty(const QString &name); // propertyChanged now and on every change
    // The last value of an observed property; never blocks.
    Q_INVOKABLE QVariant observed(const QString &name) const { return m_observed.value(name); }
    Q_INVOKABLE qint64 processAgeMs() const { return processAgeMsNow(); }
    static qint64 processAgeMsNow(); // ms since the OS created this process; -1 if unknown
    Q_INVOKABLE QVariantMap startupMarks() const { return m_marks; } // item_ms, mpv_created_ms

    mpv_handle *handle() const { return m_mpv; }
    mpv_render_context *renderContext = nullptr;

signals:
    void renderReady(); // render context exists: only now can a file with video be loaded
    void playbackRestarted();
    void fileStarted();
    // mpv's end-file reason: eof, stop, quit, error, redirect. With "error", why (mpv_error_string, English:
    // "unrecognized file format", "loading failed", ...) for the "This file can't be opened" state.
    void fileEnded(const QString &reason, const QString &error);
    void propertyChanged(const QString &name, const QVariant &value); // null value: property unavailable

protected:
    QQuickRhiItemRenderer *createRenderer() override;

private slots:
    void drainEvents();

private:
    mpv_handle *m_mpv = nullptr;
    QVariantMap m_observed;
    QVariantMap m_marks;
};
