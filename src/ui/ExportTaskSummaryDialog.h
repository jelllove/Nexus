#pragma once

#include <QDialog>
#include <QList>

#include "models/Task.h"

class QListWidget;
class QPushButton;

class ExportTaskSummaryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportTaskSummaryDialog(const QList<Task> &tasks, QWidget *parent = nullptr);
    QList<int> selectedTaskIds() const;

private:
    void updateExportButtonState();

    QListWidget *m_taskList = nullptr;
    QPushButton *m_exportButton = nullptr;
};
