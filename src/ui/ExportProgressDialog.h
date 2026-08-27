#pragma once

#include <QDialog>

class QProgressBar;
class QPlainTextEdit;
class QPushButton;
class QLabel;

class ExportProgressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportProgressDialog(QWidget *parent = nullptr);

    void setContextText(const QString &windowTitle, const QString &headline);
    void setProgress(int value, int maximum);
    void appendLog(const QString &line);
    bool isCancelled() const;

private:
    QLabel *m_titleLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPlainTextEdit *m_logView = nullptr;
    QPushButton *m_cancelButton = nullptr;
    bool m_cancelled = false;
};
