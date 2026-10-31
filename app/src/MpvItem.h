#pragma once

#include <QQuickFramebufferObject>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

struct mpv_handle;
struct mpv_render_context;

// A QML item that shows libmpv's video through the render API, inside the Qt Quick scene,
// so other items (glass panels) can sample and blur it.
class MpvItem : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit MpvItem(QQuickItem *parent = nullptr);
    ~MpvItem() override;

    Renderer *createRenderer() const override;

    Q_INVOKABLE void command(const QVariantList &args);
    Q_INVOKABLE void setOption(const QString &name, const QString &value); // before start() only
    Q_INVOKABLE void start();                                              // mpv_initialize
    Q_INVOKABLE QVariant getProperty(const QString &name) const;
    Q_INVOKABLE void setProperty(const QString &name, const QVariant &value);
    Q_INVOKABLE qint64 processAgeMs() const { return processAgeMsNow(); }
    static qint64 processAgeMsNow(); // ms since the OS created this process; -1 if unknown

    mpv_handle *handle() const { return m_mpv; }
    mpv_render_context *renderContext = nullptr;

signals:
    void renderReady(); // render context exists: only now can a file with video be loaded
    void playbackRestarted();
    void fileEnded();

private slots:
    void drainEvents();

private:
    mpv_handle *m_mpv = nullptr;
};
