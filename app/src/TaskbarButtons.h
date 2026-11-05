#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

struct ITaskbarList3;

// Three buttons under the window's taskbar thumbnail: previous clip, play/pause, next clip. Windows only; elsewhere
// this does nothing. Windows lets a program add them once its taskbar button exists, which it announces with the
// "TaskbarButtonCreated" message (again after Explorer restarts), so set window before the window first shows.
class TaskbarButtons : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)             // the middle button shows play, else pause
    Q_PROPERTY(bool hasPrevious READ hasPrevious WRITE setHasPrevious NOTIFY hasPreviousChanged) // false: disabled
    Q_PROPERTY(bool hasNext READ hasNext WRITE setHasNext NOTIFY hasNextChanged)                 // false: disabled
    Q_PROPERTY(QStringList tips READ tips WRITE setTips NOTIFY tipsChanged) // tooltips: previous, play/pause, next
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)                   // the buttons are on the taskbar

public:
    explicit TaskbarButtons(QObject *parent = nullptr);
    ~TaskbarButtons() override;

    QQuickWindow *window() const { return m_window; }
    void setWindow(QQuickWindow *window);
    bool paused() const { return m_paused; }
    void setPaused(bool paused);
    bool hasPrevious() const { return m_hasPrevious; }
    void setHasPrevious(bool has);
    bool hasNext() const { return m_hasNext; }
    void setHasNext(bool has);
    QStringList tips() const { return m_tips; }
    void setTips(const QStringList &tips);
    bool ready() const { return m_ready; }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void windowChanged();
    void pausedChanged();
    void hasPreviousChanged();
    void hasNextChanged();
    void tipsChanged();
    void readyChanged();
    void clicked(const QString &button); // "previous", "playPause" or "next"

private:
    void pushButtons(bool add); // add: the taskbar button is new, the buttons go on it. Else the state changed.
    void setReady(bool ready);
    void destroyIcons();

    QPointer<QQuickWindow> m_window;
    quintptr m_handle = 0;
    unsigned m_buttonCreated = 0; // the registered "TaskbarButtonCreated" message
    ITaskbarList3 *m_taskbar = nullptr;
    void *m_icons[3] = {};        // HICONs, previous / play-pause / next
    bool m_paused = false;
    bool m_hasPrevious = false;
    bool m_hasNext = false;
    QStringList m_tips;
    bool m_ready = false;
};
