#pragma once

#include <QObject>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class QAbstractNativeEventFilter;

class GlobalHotkey : public QObject
{
    Q_OBJECT

public:
    static GlobalHotkey& instance();

    bool registerHotkey(int modifiers, int key);
    void unregisterHotkey();

signals:
    void hotkeyPressed();

private:
    GlobalHotkey();
    ~GlobalHotkey();

    class NativeEventFilter;
    NativeEventFilter *m_filter = nullptr;
    int m_hotkeyId = 1;
    bool m_registered = false;
};
