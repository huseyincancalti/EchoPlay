#include "TaskbarButtons.h"

#include <QCoreApplication>
#ifdef Q_OS_WIN
#include <QFont>
#include <QImage>
#include <QPainter>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shobjidl.h>
#endif

#ifdef Q_OS_WIN
namespace {

constexpr int ButtonCount = 3;
constexpr const char *ButtonNames[ButtonCount] = {"previous", "playPause", "next"}; // the button ids are indexes
// Codepoints of qml/Icons.js in the filled icon font.
constexpr char16_t SkipPrevious = 0xe045, PlayArrow = 0xe037, Pause = 0xe034, SkipNext = 0xe044;

// A white glyph on transparent, as big as a small icon is at this DPI.
HICON glyphIcon(char16_t glyph, const QSize &size)
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    QFont font("EchoPlay Icons Filled");
    font.setPixelSize(size.height());
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(image.rect(), Qt::AlignCenter, QString(QChar(glyph)));
    painter.end();
    return image.toHICON();
}

} // namespace
#endif

TaskbarButtons::TaskbarButtons(QObject *parent) : QObject(parent) {}

TaskbarButtons::~TaskbarButtons()
{
    if (QCoreApplication *app = QCoreApplication::instance()) app->removeNativeEventFilter(this);
    destroyIcons();
#ifdef Q_OS_WIN
    if (m_taskbar) m_taskbar->Release();
#endif
}

void TaskbarButtons::setWindow(QQuickWindow *window)
{
    if (!window || m_window) return; // set once
    m_window = window;
#ifdef Q_OS_WIN
    // winId() creates the window, and creating it sends messages: the filter starts after that.
    const HWND hwnd = HWND(window->winId());
    m_handle = quintptr(hwnd);
    m_buttonCreated = RegisterWindowMessageW(L"TaskbarButtonCreated");
    // An elevated EchoPlay would not get the message from the (not elevated) Explorer otherwise.
    ChangeWindowMessageFilterEx(hwnd, m_buttonCreated, MSGFLT_ALLOW, nullptr);
    QCoreApplication::instance()->installNativeEventFilter(this);
#endif
    emit windowChanged();
}

void TaskbarButtons::setPaused(bool paused)
{
    if (paused == m_paused) return;
    m_paused = paused;
    emit pausedChanged();
    pushButtons(false);
}

void TaskbarButtons::setHasPrevious(bool has)
{
    if (has == m_hasPrevious) return;
    m_hasPrevious = has;
    emit hasPreviousChanged();
    pushButtons(false);
}

void TaskbarButtons::setHasNext(bool has)
{
    if (has == m_hasNext) return;
    m_hasNext = has;
    emit hasNextChanged();
    pushButtons(false);
}

void TaskbarButtons::setTips(const QStringList &tips)
{
    if (tips == m_tips) return;
    m_tips = tips;
    emit tipsChanged();
    pushButtons(false);
}

void TaskbarButtons::destroyIcons()
{
#ifdef Q_OS_WIN
    for (void *&icon : m_icons) {
        if (icon) DestroyIcon(HICON(icon));
        icon = nullptr;
    }
#endif
}

// Adds the buttons (the taskbar button is new) or updates them (the state changed). The icons are drawn again each
// time: it is rare and cheap, and the DPI may have changed. The ones they replace are destroyed once the taskbar has
// the new ones.
void TaskbarButtons::pushButtons(bool add)
{
#ifdef Q_OS_WIN
    if (!m_handle || (!add && !m_taskbar)) return;
    const HWND hwnd = HWND(m_handle);
    if (add) {
        // A new taskbar button (the first, or Explorer restarted): the old interface is of no use.
        if (m_taskbar) m_taskbar->Release();
        m_taskbar = nullptr;
        setReady(false);
        if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskbarList3,
                                    reinterpret_cast<void **>(&m_taskbar)))
            || FAILED(m_taskbar->HrInit())) {
            if (m_taskbar) m_taskbar->Release();
            m_taskbar = nullptr;
            return;
        }
    }

    const UINT dpi = GetDpiForWindow(hwnd);
    const QSize iconSize(GetSystemMetricsForDpi(SM_CXSMICON, dpi), GetSystemMetricsForDpi(SM_CYSMICON, dpi));
    const char16_t glyphs[ButtonCount] = {SkipPrevious, m_paused ? PlayArrow : Pause, SkipNext};
    const bool enabled[ButtonCount] = {m_hasPrevious, true, m_hasNext};
    THUMBBUTTON buttons[ButtonCount] = {};
    for (int i = 0; i < ButtonCount; ++i) {
        buttons[i].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
        buttons[i].iId = i;
        buttons[i].hIcon = glyphIcon(glyphs[i], iconSize);
        m_tips.value(i).left(int(ARRAYSIZE(buttons[i].szTip)) - 1).toWCharArray(buttons[i].szTip); // the rest stays 0
        buttons[i].dwFlags = enabled[i] ? THBF_ENABLED : THBF_DISABLED;
    }
    const HRESULT result = add ? m_taskbar->ThumbBarAddButtons(hwnd, ButtonCount, buttons)
                               : m_taskbar->ThumbBarUpdateButtons(hwnd, ButtonCount, buttons);
    if (FAILED(result)) {
        for (const THUMBBUTTON &button : buttons)
            if (button.hIcon) DestroyIcon(button.hIcon);
        if (add) {
            m_taskbar->Release();
            m_taskbar = nullptr;
        }
        return;
    }
    destroyIcons();
    for (int i = 0; i < ButtonCount; ++i) m_icons[i] = buttons[i].hIcon;
    setReady(true);
#else
    Q_UNUSED(add)
#endif
}

void TaskbarButtons::setReady(bool ready)
{
    if (ready == m_ready) return;
    m_ready = ready;
    emit readyChanged();
}

bool TaskbarButtons::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG" || !m_handle) return false;
    const MSG *msg = static_cast<MSG *>(message);
    if (quintptr(msg->hwnd) != m_handle) return false;
    // Never write to result: messages that come through Qt's event dispatcher pass none.
    if (m_buttonCreated && msg->message == m_buttonCreated) {
        pushButtons(true);
    } else if (msg->message == WM_COMMAND && HIWORD(msg->wParam) == THBN_CLICKED) { // posted by the taskbar: Qt filters
        // a WM_COMMAND only when it comes through the message queue
        const int id = LOWORD(msg->wParam);
        if (id >= 0 && id < ButtonCount) emit clicked(ButtonNames[id]);
    }
    return false;
#else
    Q_UNUSED(eventType) Q_UNUSED(message) Q_UNUSED(result)
    return false;
#endif
}
