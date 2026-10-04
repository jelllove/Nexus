#include "GlobalHotkey.h"
#include <QCoreApplication>
#include <QAbstractNativeEventFilter>
#include <QGuiApplication>
#include <QSocketNotifier>

#ifdef Q_OS_WIN
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <Carbon/Carbon.h>
#elif defined(NEXUS_HAVE_X11)
#include <X11/Xlib.h>
#include <X11/keysym.h>

namespace {
int x11Error = 0;
int recordX11Error(Display *, XErrorEvent *event)
{
    x11Error = event->error_code;
    return 0;
}
}
#endif

class GlobalHotkey::NativeEventFilter : public QAbstractNativeEventFilter
{
public:
    NativeEventFilter(GlobalHotkey *parent) : m_parent(parent) {}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override
#else
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override
#endif
    {
        Q_UNUSED(result);
#ifdef Q_OS_WIN
        if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
            MSG *msg = static_cast<MSG*>(message);
            if (msg->message == WM_HOTKEY && msg->wParam == (WPARAM)m_parent->m_hotkeyId) {
                emit m_parent->hotkeyPressed();
                return true;
            }
        }
#else
        Q_UNUSED(eventType);
        Q_UNUSED(message);
#endif
        return false;
    }

private:
    GlobalHotkey *m_parent;
};

GlobalHotkey::GlobalHotkey()
    : QObject(nullptr)
{
    m_filter = new NativeEventFilter(this);
    QCoreApplication::instance()->installNativeEventFilter(m_filter);
}

GlobalHotkey::~GlobalHotkey()
{
    unregisterHotkey();
    if (auto *app = QCoreApplication::instance())
        app->removeNativeEventFilter(m_filter);
    delete m_filter;
}

GlobalHotkey& GlobalHotkey::instance()
{
    static GlobalHotkey inst;
    return inst;
}

QString GlobalHotkey::availabilityMessage()
{
    const QString platform = QGuiApplication::platformName();
    if (platform.startsWith("wayland"))
        return QStringLiteral("Global hotkeys are unavailable on Wayland. Use the desktop's "
                              "shortcut settings to launch Nexus and restore its window.");
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    if (platform != "offscreen" && platform != "minimal") return {};
#elif defined(NEXUS_HAVE_X11)
    if (platform == "xcb") return {};
#endif
    return QStringLiteral("Global hotkeys are unavailable on this desktop/build.");
}

bool GlobalHotkey::registerHotkey()
{
    unregisterHotkey();
    m_error = availabilityMessage();
    if (!m_error.isEmpty()) return false;
#ifdef Q_OS_WIN
    m_registered = RegisterHotKey(nullptr, m_hotkeyId, MOD_CONTROL | MOD_SHIFT, 'N');
    if (!m_registered)
        m_error = QString("Failed to register Ctrl+Shift+N (Windows error %1).").arg(GetLastError());
#elif defined(Q_OS_MACOS)
    const EventTypeSpec eventType{ kEventClassKeyboard, kEventHotKeyPressed };
    EventHandlerRef handler = nullptr;
    const OSStatus handlerStatus = InstallApplicationEventHandler(
        [](EventHandlerCallRef, EventRef event, void *context) -> OSStatus {
            EventHotKeyID id{};
            const OSStatus status = GetEventParameter(event, kEventParamDirectObject,
                typeEventHotKeyID, nullptr, sizeof(id), nullptr, &id);
            if (status != noErr || id.signature != 0x4e585553 || id.id != 1)
                return eventNotHandledErr;
            auto *hotkey = static_cast<GlobalHotkey *>(context);
            QMetaObject::invokeMethod(hotkey, [hotkey]() { emit hotkey->hotkeyPressed(); },
                                      Qt::QueuedConnection);
            return noErr;
        }, 1, &eventType, this, &handler);
    if (handlerStatus != noErr) {
        m_error = QString("Failed to install macOS hotkey handler (%1).").arg(handlerStatus);
        return false;
    }
    m_eventHandlerRef = handler;
    EventHotKeyRef hotkey = nullptr;
    const EventHotKeyID id{0x4e585553, 1};
    const OSStatus status = RegisterEventHotKey(kVK_ANSI_N, controlKey | shiftKey,
        id, GetApplicationEventTarget(), 0, &hotkey);
    if (status != noErr) {
        m_error = QString("Failed to register Ctrl+Shift+N on macOS (%1).").arg(status);
        unregisterHotkey();
        return false;
    }
    m_hotkeyRef = hotkey;
    m_registered = true;
#elif defined(NEXUS_HAVE_X11)
    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        m_error = QStringLiteral("Failed to open the X11 display for global hotkeys.");
        return false;
    }
    m_display = display;
    const KeyCode key = XKeysymToKeycode(display, XK_n);
    if (!key) {
        m_error = QStringLiteral("The X11 keyboard has no key for Ctrl+Shift+N.");
        unregisterHotkey();
        return false;
    }
    unsigned int lockMask = LockMask;
    const KeyCode numLock = XKeysymToKeycode(display, XK_Num_Lock);
    XModifierKeymap *modifierMap = XGetModifierMapping(display);
    if (!modifierMap) {
        m_error = QStringLiteral("Failed to read X11 keyboard modifiers.");
        unregisterHotkey();
        return false;
    }
    for (int modifier = 0; modifier < 8; ++modifier) {
        for (int slot = 0; slot < modifierMap->max_keypermod; ++slot) {
            if (numLock && modifierMap->modifiermap[modifier * modifierMap->max_keypermod + slot] == numLock)
                lockMask |= 1u << modifier;
        }
    }
    XFreeModifiermap(modifierMap);
    // Ignore Caps Lock and Num Lock, but not unrelated shortcut modifiers.
    XSync(display, False);
    x11Error = 0;
    const auto previousHandler = XSetErrorHandler(recordX11Error);
    for (unsigned int locks = 0; locks < 256; ++locks) {
        if (locks & ~lockMask) continue;
        XGrabKey(display, key, ControlMask | ShiftMask | locks, DefaultRootWindow(display),
                 False, GrabModeAsync, GrabModeAsync);
    }
    XSync(display, False);
    XSetErrorHandler(previousHandler);
    if (x11Error) {
        m_error = QString("Failed to register Ctrl+Shift+N (X11 error %1).").arg(x11Error);
        unregisterHotkey();
        return false;
    }
    m_notifier = new QSocketNotifier(ConnectionNumber(display), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this, display, key]() {
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == KeyPress && event.xkey.keycode == key)
                emit hotkeyPressed();
        }
    });
    m_registered = true;
#endif
    return m_registered;
}

void GlobalHotkey::unregisterHotkey()
{
#ifdef Q_OS_WIN
    if (m_registered) {
        UnregisterHotKey(nullptr, m_hotkeyId);
    }
#elif defined(Q_OS_MACOS)
    if (m_hotkeyRef) UnregisterEventHotKey(static_cast<EventHotKeyRef>(m_hotkeyRef));
    if (m_eventHandlerRef) RemoveEventHandler(static_cast<EventHandlerRef>(m_eventHandlerRef));
    m_hotkeyRef = nullptr;
    m_eventHandlerRef = nullptr;
#elif defined(NEXUS_HAVE_X11)
    delete m_notifier;
    m_notifier = nullptr;
    if (m_display) {
        Display *display = static_cast<Display *>(m_display);
        XUngrabKey(display, AnyKey, AnyModifier, DefaultRootWindow(display));
        XCloseDisplay(display);
        m_display = nullptr;
    }
#endif
    m_registered = false;
}
