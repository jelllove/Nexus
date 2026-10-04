#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class MainWindow;

class InstallerSmoke : public QObject
{
public:
    explicit InstallerSmoke(const QString &outputDirectory);
    bool prepare();
    void start(MainWindow *window);

private:
    void checkEditor();
    void capture();
    void fail(const QString &message);
    bool saveReport(const QString &status, const QString &message);

    QString m_outputDirectory;
    MainWindow *m_window = nullptr;
    QTimer m_poll;
    QTimer m_timeout;
    int m_productId = -1;
    int m_taskId = -1;
    bool m_checkPending = false;
    bool m_finished = false;
    bool m_rendered = false;
    bool m_saved = false;
};
