#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

#include "models/Task.h"

enum class TaskSummaryExportIssue {
    None,
    NoProductSelected,
    NoActiveTasks,
    NoTaskSelected
};

TaskSummaryExportIssue validateTaskSummaryExportInputs(
    int productId,
    const QList<Task> &availableTasks,
    const QList<int> &selectedTaskIds);

QString summarizeTaskContentForMarkdown(const QString &htmlContent, int summaryMaxChars = 120);

QString buildTaskSummaryMarkdown(
    const QString &productName,
    const QDateTime &exportedAt,
    const QList<Task> &selectedTasks,
    int summaryMaxChars = 120);
