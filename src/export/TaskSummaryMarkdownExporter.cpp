#include "TaskSummaryMarkdownExporter.h"

#include <QRegularExpression>
#include <QTextDocument>

namespace {
QString escapeMarkdownCell(QString value)
{
    value.replace("\\", "\\\\");
    value.replace("|", "\\|");
    value.replace("\n", " ");
    value.replace("\r", " ");
    value.replace(QRegularExpression("\\s+"), " ");
    return value.trimmed();
}
}

TaskSummaryExportIssue validateTaskSummaryExportInputs(
    int productId,
    const QList<Task> &availableTasks,
    const QList<int> &selectedTaskIds)
{
    if (productId <= 0) {
        return TaskSummaryExportIssue::NoProductSelected;
    }
    if (availableTasks.isEmpty()) {
        return TaskSummaryExportIssue::NoActiveTasks;
    }
    if (selectedTaskIds.isEmpty()) {
        return TaskSummaryExportIssue::NoTaskSelected;
    }
    return TaskSummaryExportIssue::None;
}

QString summarizeTaskContentForMarkdown(const QString &htmlContent, int summaryMaxChars)
{
    QTextDocument document;
    document.setHtml(htmlContent);

    QString plain = document.toPlainText();
    plain.replace(QRegularExpression("\\s+"), " ");
    plain = plain.trimmed();

    if (summaryMaxChars > 0 && plain.size() > summaryMaxChars) {
        const int preservedChars = qMax(summaryMaxChars - 1, 1);
        plain = plain.left(preservedChars).trimmed() + QString::fromUtf8("…");
    }
    return plain;
}

QString buildTaskSummaryMarkdown(
    const QString &productName,
    const QDateTime &exportedAt,
    const QList<Task> &selectedTasks,
    int summaryMaxChars)
{
    QString markdown;
    markdown += "# Task Summary Export\n\n";
    markdown += QString("**Product:** %1  \n").arg(escapeMarkdownCell(productName));
    markdown += QString("**Exported At:** %1\n\n")
                    .arg(exportedAt.toLocalTime().toString("yyyy-MM-dd HH:mm:ss"));
    markdown += "| Status | Title | Summary |\n";
    markdown += "| --- | --- | --- |\n";

    for (const Task &task : selectedTasks) {
        markdown += QString("| %1 | %2 | %3 |\n")
                        .arg(escapeMarkdownCell(Task::workStatusToString(task.workStatus)))
                        .arg(escapeMarkdownCell(task.title))
                        .arg(escapeMarkdownCell(
                            summarizeTaskContentForMarkdown(task.content, summaryMaxChars)));
    }

    return markdown;
}
