#include "WindowChrome.h"

#include <QCoreApplication>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif

WindowChrome::WindowChrome(QObject *parent) : QObject(parent) {}

WindowChrome::~WindowChrome()
{
    if (m_window) QCoreApplication::instance()->removeNativeEventFilter(this);
}

void WindowChrome::attach(QQuickWindow *window)
{
    m_window = window;
#ifdef Q_OS_WIN
    // winId() creates the window, and creating it sends messages: the filter starts after that.
    const HWND hwnd = HWND(window->winId());
    m_handle = quintptr(hwnd);
    QCoreApplication::instance()->installNativeEventFilter(this);
    // Frameless to Qt (no frame margins in its geometry), a normal window to Windows.
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    SetWindowLongPtrW(hwnd, GWL_STYLE, (style & ~LONG_PTR(WS_POPUP)) | WS_OVERLAPPEDWINDOW);
    // A sliver of the frame inside the client area keeps the shadow DWM gives framed windows.
    const MARGINS margins{0, 0, 1, 0};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#endif
}

void WindowChrome::toggleMaximized()
{
    if (!m_window) return;
    m_window->visibility() == QWindow::Maximized ? m_window->showNormal() : m_window->showMaximized();
}

void WindowChrome::setMaximizeHovered(bool hovered)
{
    if (hovered == m_maximizeHovered) return;
    m_maximizeHovered = hovered;
    emit maximizeHoveredChanged();
}

bool WindowChrome::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG" || !m_window) return false;
    const MSG *msg = static_cast<MSG *>(message);
    if (quintptr(msg->hwnd) != m_handle) return false;

    switch (msg->message) {
    case WM_NCCALCSIZE:
        // The client area is the whole window. Maximized, Windows places the frame off the screen, and Qt's size
        // limit for a frameless window already keeps it to the work area.
        if (msg->wParam) {
            *result = 0;
            return true;
        }
        return false;
    case WM_NCHITTEST: {
        const bool windowed = m_window->visibility() == QWindow::Windowed;
        if (m_window->visibility() == QWindow::FullScreen) {
            *result = HTCLIENT;
            return true;
        }
        RECT r;
        GetWindowRect(msg->hwnd, &r);
        const qreal ratio = m_window->devicePixelRatio();
        const int x = GET_X_LPARAM(msg->lParam) - r.left, y = GET_Y_LPARAM(msg->lParam) - r.top;
        const int w = r.right - r.left, h = r.bottom - r.top;
        if (windowed) {
            const int edge = qRound(6 * ratio);
            const bool left = x < edge, right = x >= w - edge, top = y < edge, bottom = y >= h - edge;
            if (top && left) *result = HTTOPLEFT;
            else if (top && right) *result = HTTOPRIGHT;
            else if (bottom && left) *result = HTBOTTOMLEFT;
            else if (bottom && right) *result = HTBOTTOMRIGHT;
            else if (left) *result = HTLEFT;
            else if (right) *result = HTRIGHT;
            else if (top) *result = HTTOP;
            else if (bottom) *result = HTBOTTOM;
            else *result = HTNOWHERE;
            if (*result != HTNOWHERE) return true;
        }
        if (y < qRound(captionHeight() * ratio)) {
            const int button = qRound(buttonWidth() * ratio);
            if (x >= w - button) *result = HTCLIENT;          // close: QML's
            else if (x >= w - 2 * button) *result = HTMAXBUTTON; // Windows 11 shows the snap layouts here
            else if (x >= w - 3 * button) *result = HTCLIENT; // minimize: QML's
            else *result = HTCAPTION;
            return true;
        }
        *result = HTCLIENT;
        return true;
    }
    case WM_NCMOUSEMOVE:
        emit pointerMoved();
        setMaximizeHovered(msg->wParam == HTMAXBUTTON);
        return false;
    case WM_NCMOUSELEAVE:
    case WM_MOUSEMOVE:
        setMaximizeHovered(false);
        return false;
    case WM_NCLBUTTONDOWN:
        if (msg->wParam != HTMAXBUTTON) return false;
        *result = 0; // Windows would draw its own old-style button over ours
        return true;
    case WM_NCLBUTTONUP:
        if (msg->wParam != HTMAXBUTTON) return false;
        toggleMaximized();
        *result = 0;
        return true;
    default:
        return false;
    }
#else
    Q_UNUSED(eventType) Q_UNUSED(message) Q_UNUSED(result)
    return false;
#endif
}
