#pragma once

#include <QDialog>

class QProgressBar;
class QPlainTextEdit;
class QPushButton;

class ExportProgressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportProgressDialog(QWidget *parent = nullptr);

    void setProgress(int value, int maximum);
    void appendLog(const QString &line);
    bool isCancelled() const;

private:
    QProgressBar *m_progressBar = nullptr;
    QPlainTextEdit *m_logView = nullptr;
    QPushButton *m_cancelButton = nullptr;
    bool m_cancelled = false;
};

