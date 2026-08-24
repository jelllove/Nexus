#include "ExportProgressDialog.h"

#include <QApplication>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

ExportProgressDialog::ExportProgressDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Export Progress");
    setModal(true);
    setMinimumSize(620, 380);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto *title = new QLabel("Exporting tasks. Please wait...", this);
    layout->addWidget(title);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    layout->addWidget(m_progressBar);

    m_logView = new QPlainTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setMinimumHeight(220);
    layout->addWidget(m_logView, 1);

    auto *buttons = new QDialogButtonBox(this);
    m_cancelButton = buttons->addButton("Cancel", QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);

    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        if (m_cancelled) {
            return;
        }
        m_cancelled = true;
        m_cancelButton->setEnabled(false);
        appendLog("Cancellation requested by user.");
    });
}

void ExportProgressDialog::setProgress(int value, int maximum)
{
    m_progressBar->setMaximum(maximum > 0 ? maximum : 1);
    m_progressBar->setValue(qBound(0, value, m_progressBar->maximum()));
    qApp->processEvents();
}

void ExportProgressDialog::appendLog(const QString &line)
{
    const QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_logView->appendPlainText(QString("[%1] %2").arg(timestamp, line));
    qApp->processEvents();
}

bool ExportProgressDialog::isCancelled() const
{
    return m_cancelled;
}

