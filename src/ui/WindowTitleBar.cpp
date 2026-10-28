#include "WindowTitleBar.h"
#include <QAbstractButton>
#include <QGuiApplication>
#include <QDebug>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QStyle>
#include <QToolButton>
#include <QWindowStateChangeEvent>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <windowsx.h>
#endif

WindowTitleBar::WindowTitleBar(QMainWindow *window, QWidget *updateButton)
    : QWidget(window), m_window(window)
{
    setObjectName("windowTitleBar");
    setFixedHeight(36);
    setStyleSheet(
        "WindowTitleBar { background-color: #f3f3f3; }"
        "QToolButton { border: none; border-radius: 0; background: transparent; }"
        "QToolButton:hover, QToolButton[nativeHover=\"true\"] { background-color: #e0e0e0; }"
        "QToolButton:pressed { background-color: #cccccc; }"
        "QToolButton#windowCloseButton:hover { background-color: #e81123; }");
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 0, 0);
    layout->setSpacing(0);
    auto *icon = new QLabel(this);
    icon->setObjectName("windowTitleIcon");
    icon->setFixedWidth(24);
    connect(window, &QWidget::windowIconChanged, icon, [icon](const QIcon &value) {
        icon->setPixmap(value.pixmap(16, 16));
    });
    icon->setPixmap(window->windowIcon().pixmap(16, 16));
    layout->addWidget(icon);
    auto *title = new QLabel(window->windowTitle(), this);
    title->setObjectName("windowTitleLabel");
    title->setTextFormat(Qt::PlainText);
    title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    connect(window, &QWidget::windowTitleChanged, title, &QLabel::setText);
    layout->addWidget(title, 1);
    layout->addWidget(updateButton, 0, Qt::AlignVCenter);
    layout->addSpacing(8);
    auto addControl = [this, layout](const QString &name, const QString &text,
                                     QStyle::StandardPixmap symbol) {
        auto *button = new QToolButton(this);
        button->setObjectName(name);
        button->setAccessibleName(text);
        button->setToolTip(text);
        button->setIcon(style()->standardIcon(symbol));
        button->setFixedSize(46, height());
        layout->addWidget(button);
        return button;
    };
    auto *minimize = addControl("windowMinimizeButton", "Minimize", QStyle::SP_TitleBarMinButton);
    m_maximizeButton = addControl("windowMaximizeButton", "Maximize", QStyle::SP_TitleBarMaxButton);
    auto *close = addControl("windowCloseButton", "Close", QStyle::SP_TitleBarCloseButton);
    connect(minimize, &QToolButton::clicked, window, &QWidget::showMinimized);
    connect(m_maximizeButton, &QToolButton::clicked, window, [window]() {
        if (window->isMaximized()) window->showNormal();
        else window->showMaximized();
    });
    connect(close, &QToolButton::clicked, window, &QWidget::close);
    window->setWindowFlag(Qt::FramelessWindowHint);
    window->installEventFilter(this);
    syncWindowState();
}

void WindowTitleBar::syncWindowState()
{
    const bool maximized = m_window->isMaximized();
    const QString text = maximized ? "Restore" : "Maximize";
    m_maximizeButton->setToolTip(text);
    m_maximizeButton->setAccessibleName(text);
    m_maximizeButton->setIcon(style()->standardIcon(
        maximized ? QStyle::SP_TitleBarNormalButton : QStyle::SP_TitleBarMaxButton));
    setVisible(!m_window->isFullScreen());
}

bool WindowTitleBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window) {
        if (event->type() == QEvent::WindowStateChange) {
            const auto *change = static_cast<QWindowStateChangeEvent *>(event);
            if (m_window->isMinimized() && !change->oldState().testFlag(Qt::WindowMinimized))
                m_restoreMaximized = change->oldState().testFlag(Qt::WindowMaximized);
            syncWindowState();
            applyNativeFrame();
        } else if (event->type() == QEvent::WinIdChange || event->type() == QEvent::Show) {
            applyNativeFrame();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void WindowTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_window->isFullScreen()) {
        m_maximizeButton->click();
        event->accept();
    } else {
        QWidget::mouseDoubleClickEvent(event);
    }
}

void WindowTitleBar::applyNativeFrame()
{
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() != "windows" || !m_window->internalWinId()
        || m_window->isFullScreen()) return;
    const auto handle = reinterpret_cast<HWND>(m_window->internalWinId());
    const LONG_PTR style = GetWindowLongPtr(handle, GWL_STYLE);
    // Keep native resizing, system commands and snapping with the client-drawn caption.
    const LONG_PTR desired = style | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU
                                  | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    if (style == desired) return;
    SetLastError(0);
    if (!SetWindowLongPtr(handle, GWL_STYLE, desired) && GetLastError()) {
        qWarning() << "Cannot configure the Windows title bar:" << GetLastError();
        return;
    }
    if (!SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED))
        qWarning() << "Cannot refresh the Windows title bar:" << GetLastError();
#endif
}

void WindowTitleBar::setMaximizeHovered(bool hovered)
{
    if (m_maximizeHovered == hovered) return;
    m_maximizeHovered = hovered;
    m_maximizeButton->setProperty("nativeHover", hovered);
    m_maximizeButton->style()->unpolish(m_maximizeButton);
    m_maximizeButton->style()->polish(m_maximizeButton);
    m_maximizeButton->update();
}

bool WindowTitleBar::handleNativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (!m_window || QGuiApplication::platformName() != "windows"
        || (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG"))
        return false;
    const auto *msg = static_cast<MSG *>(message);
    const auto handle = reinterpret_cast<HWND>(m_window->internalWinId());
    if (!handle || msg->hwnd != handle) return false;
    switch (msg->message) {
    case WM_NCCALCSIZE:
        *result = 0;
        return true;
    case WM_SYSCOMMAND:
        // Native restore alone leaves Qt's frameless-window state maximized.
        switch (msg->wParam & 0xfff0) {
        case SC_MINIMIZE:
            m_window->showMinimized();
            break;
        case SC_MAXIMIZE:
            m_window->showMaximized();
            break;
        case SC_RESTORE:
            if (m_window->isMinimized() && m_restoreMaximized)
                m_window->showMaximized();
            else
                m_window->showNormal();
            break;
        case SC_CLOSE:
            m_window->close();
            break;
        default:
            return false;
        }
        *result = 0;
        return true;
    case WM_GETMINMAXINFO: {
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (!GetMonitorInfo(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor)) {
            qWarning() << "Cannot read the Windows monitor work area:" << GetLastError();
            return false;
        }
        auto *limits = reinterpret_cast<MINMAXINFO *>(msg->lParam);
        limits->ptMaxPosition = {monitor.rcWork.left - monitor.rcMonitor.left,
                                 monitor.rcWork.top - monitor.rcMonitor.top};
        limits->ptMaxSize = {monitor.rcWork.right - monitor.rcWork.left,
                             monitor.rcWork.bottom - monitor.rcWork.top};
        const QSize minimum = m_window->minimumSize().expandedTo(m_window->minimumSizeHint());
        limits->ptMinTrackSize.x = qMax(limits->ptMinTrackSize.x,
            LONG(qRound(minimum.width() * m_window->devicePixelRatioF())));
        limits->ptMinTrackSize.y = qMax(limits->ptMinTrackSize.y,
            LONG(qRound(minimum.height() * m_window->devicePixelRatioF())));
        *result = 0;
        return true;
    }
    case WM_NCHITTEST: {
        POINT origin{};
        RECT bounds{};
        if (!ClientToScreen(handle, &origin) || !GetWindowRect(handle, &bounds)) {
            qWarning() << "Cannot hit-test the Windows title bar:" << GetLastError();
            return false;
        }
        const int x = GET_X_LPARAM(msg->lParam);
        const int y = GET_Y_LPARAM(msg->lParam);
        if (!IsZoomed(handle) && !m_window->isFullScreen()) {
            const UINT dpi = GetDpiForWindow(handle);
            const int borderX = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi)
                              + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
            const int borderY = GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi)
                              + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
            const bool left = x < bounds.left + borderX;
            const bool right = x >= bounds.right - borderX;
            const bool top = y < bounds.top + borderY;
            const bool bottom = y >= bounds.bottom - borderY;
            const int edge = top ? (left ? HTTOPLEFT : right ? HTTOPRIGHT : HTTOP)
                : bottom ? (left ? HTBOTTOMLEFT : right ? HTBOTTOMRIGHT : HTBOTTOM)
                : left ? HTLEFT : right ? HTRIGHT : 0;
            if (edge) {
                *result = edge;
                return true;
            }
        }
        const qreal scale = m_window->devicePixelRatioF();
        const QPoint point = mapFrom(m_window,
            QPoint(qRound((x - origin.x) / scale), qRound((y - origin.y) / scale)));
        if (!rect().contains(point) || !isVisible()) return false;
        const QWidget *child = childAt(point);
        if (child == m_maximizeButton) *result = HTMAXBUTTON;
        else if (qobject_cast<const QAbstractButton *>(child)) *result = HTCLIENT;
        else if (child && child->objectName() == "windowTitleIcon") *result = HTSYSMENU;
        else *result = HTCAPTION;
        return true;
    }
    case WM_NCMOUSEMOVE:
        setMaximizeHovered(msg->wParam == HTMAXBUTTON);
        break;
    case WM_NCMOUSELEAVE:
        setMaximizeHovered(false);
        break;
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
        if (msg->wParam == HTMAXBUTTON) {
            m_nativeMaximizePressed = true;
            m_maximizeButton->setDown(true);
            SetCapture(handle);
            *result = 0;
            return true;
        }
        break;
    case WM_NCLBUTTONUP:
    case WM_LBUTTONUP:
        if (m_nativeMaximizePressed) {
            POINT point{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
            if (msg->message == WM_LBUTTONUP && !ClientToScreen(handle, &point)) {
                qWarning() << "Cannot release the Windows maximize control:" << GetLastError();
                return false;
            }
            POINT origin{};
            if (!ClientToScreen(handle, &origin)) {
                qWarning() << "Cannot locate the Windows maximize control:" << GetLastError();
                return false;
            }
            const qreal scale = m_window->devicePixelRatioF();
            const QPoint position = m_maximizeButton->mapFrom(m_window,
                QPoint(qRound((point.x - origin.x) / scale),
                       qRound((point.y - origin.y) / scale)));
            m_nativeMaximizePressed = false;
            m_maximizeButton->setDown(false);
            ReleaseCapture();
            if (m_maximizeButton->rect().contains(position))
                m_maximizeButton->click();
            *result = 0;
            return true;
        }
        break;
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        m_nativeMaximizePressed = false;
        m_maximizeButton->setDown(false);
        setMaximizeHovered(false);
        break;
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return false;
}
