#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>

// EchoPlay draws its own title bar, and Windows still treats the window as a normal one: the native frame stays
// (Aero Snap, resize edges, shadow, rounded corners, the system menu, snap layouts on the maximize button) while
// the client area covers all of it. QML keeps the minimize and close buttons; the maximize button belongs to
// Windows, for the snap layouts flyout, so its hover comes from here.
class WindowChrome : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool maximizeHovered READ maximizeHovered NOTIFY maximizeHoveredChanged)
    Q_PROPERTY(int captionHeight READ captionHeight CONSTANT) // in QML units
    Q_PROPERTY(int buttonWidth READ buttonWidth CONSTANT)     // one caption button, in QML units

public:
    explicit WindowChrome(QObject *parent = nullptr);
    ~WindowChrome() override;

    bool maximizeHovered() const { return m_maximizeHovered; }
    int captionHeight() const { return 40; }
    int buttonWidth() const { return 46; }

    // Call before the window first shows, with flags Qt.Window | Qt.FramelessWindowHint.
    Q_INVOKABLE void attach(QQuickWindow *window);
    Q_INVOKABLE void toggleMaximized();

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void maximizeHoveredChanged();
    void pointerMoved(); // over the caption: Windows reports those moves as non-client, so QML never sees them

private:
    void setMaximizeHovered(bool hovered);

    QPointer<QQuickWindow> m_window;
    quintptr m_handle = 0;
    bool m_maximizeHovered = false;
};
