#pragma once

#include <QObject>
#include <QString>

class QAbstractNativeEventFilter;
class QSocketNotifier;

class GlobalHotkey : public QObject
{
    Q_OBJECT

public:
    static GlobalHotkey& instance();

    bool registerHotkey();
    void unregisterHotkey();
    static QString availabilityMessage();
    QString errorString() const { return m_error; }

signals:
    void hotkeyPressed();

private:
    GlobalHotkey();
    ~GlobalHotkey();

    class NativeEventFilter;
    NativeEventFilter *m_filter = nullptr;
    int m_hotkeyId = 1;
    bool m_registered = false;
    QString m_error;
    void *m_hotkeyRef = nullptr;
    void *m_eventHandlerRef = nullptr;
    void *m_display = nullptr;
    QSocketNotifier *m_notifier = nullptr;
};
