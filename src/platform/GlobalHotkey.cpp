#include "GlobalHotkey.h"
#include <QApplication>
#include <QAbstractNativeEventFilter>

#ifdef Q_OS_WIN
#include <windows.h>
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
        if (eventType == "windows_generic_MSG") {
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
    QApplication::instance()->installNativeEventFilter(m_filter);
}

GlobalHotkey::~GlobalHotkey()
{
    unregisterHotkey();
    QApplication::instance()->removeNativeEventFilter(m_filter);
    delete m_filter;
}

GlobalHotkey& GlobalHotkey::instance()
{
    static GlobalHotkey inst;
    return inst;
}

bool GlobalHotkey::registerHotkey(int modifiers, int key)
{
#ifdef Q_OS_WIN
    unregisterHotkey();
    m_registered = RegisterHotKey(nullptr, m_hotkeyId, modifiers, key);
    return m_registered;
#else
    Q_UNUSED(modifiers);
    Q_UNUSED(key);
    return false;
#endif
}

void GlobalHotkey::unregisterHotkey()
{
#ifdef Q_OS_WIN
    if (m_registered) {
        UnregisterHotKey(nullptr, m_hotkeyId);
        m_registered = false;
    }
#endif
}
