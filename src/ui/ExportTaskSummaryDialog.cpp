#include "ExportTaskSummaryDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

ExportTaskSummaryDialog::ExportTaskSummaryDialog(const QList<Task> &tasks, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Export Task Summary");
    resize(520, 420);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *description = new QLabel(
        "Select the active tasks to include in the Markdown export.", this);
    description->setWordWrap(true);
    layout->addWidget(description);

    m_taskList = new QListWidget(this);
    m_taskList->setObjectName("exportTaskList");
    layout->addWidget(m_taskList, 1);

    for (const Task &task : tasks) {
        auto *item = new QListWidgetItem(
            QString("[%1] %2")
                .arg(Task::workStatusToString(task.workStatus), task.title),
            m_taskList);
        item->setData(Qt::UserRole, task.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_exportButton = buttonBox->button(QDialogButtonBox::Ok);
    m_exportButton->setText("Export");
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_taskList, &QListWidget::itemChanged, this, [this](QListWidgetItem *) {
        updateExportButtonState();
    });

    updateExportButtonState();
}

QList<int> ExportTaskSummaryDialog::selectedTaskIds() const
{
    QList<int> ids;
    for (int i = 0; i < m_taskList->count(); ++i) {
        QListWidgetItem *item = m_taskList->item(i);
        if (item->checkState() == Qt::Checked) {
            ids.append(item->data(Qt::UserRole).toInt());
        }
    }
    return ids;
}

void ExportTaskSummaryDialog::updateExportButtonState()
{
    if (!m_exportButton) {
        return;
    }
    m_exportButton->setEnabled(!selectedTaskIds().isEmpty());
}
