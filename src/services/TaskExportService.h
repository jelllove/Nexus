#pragma once

#include <QString>
#include <QDateTime>
#include <QList>
#include "models/Task.h"

struct ExportSubTaskItem {
    QString title;
    TaskWorkStatus workStatus = TaskWorkStatus::NotStarted;
    QString workStatusIcon;
};

struct ExportTaskItem {
    QString productName;
    QString statusText;
    QString title;
    TaskPriority priority = TaskPriority::Medium;
    QString contentHtml;
    QDateTime updatedAt;
    QList<ExportSubTaskItem> subtasks;
    QString simpleDescription;
    QString workStatusText;
    QString workStatusIcon;
};

class TaskExportService
{
public:
    static QString extractPlainText(const QString &html);
    static QString fallbackSimpleDescription(const QString &html, int maxChars = 120);
    static QString buildMarkdown(const QList<ExportTaskItem> &tasks, const QDateTime &exportedAt);
};
