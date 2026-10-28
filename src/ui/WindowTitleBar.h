#pragma once

#include <QPointer>
#include <QWidget>

class QMainWindow;
class QToolButton;

class WindowTitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit WindowTitleBar(QMainWindow *window, QWidget *updateButton);
    bool handleNativeEvent(const QByteArray &eventType, void *message, qintptr *result);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void syncWindowState();
    void applyNativeFrame();
    void setMaximizeHovered(bool hovered);

    QPointer<QMainWindow> m_window;
    QToolButton *m_maximizeButton;
    bool m_nativeMaximizePressed = false;
    bool m_maximizeHovered = false;
    bool m_restoreMaximized = false;
};
