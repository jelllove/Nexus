#include <QtTest>
#include <QDateTime>
#include <QListWidget>

#include "export/TaskSummaryMarkdownExporter.h"
#include "models/Task.h"
#include "ui/ExportTaskSummaryDialog.h"

class TaskSummaryMarkdownExporterTests : public QObject
{
    Q_OBJECT

private:
    static Task makeTask(int id, const QString &title, TaskWorkStatus status, const QString &content)
    {
        Task task;
        task.id = id;
        task.title = title;
        task.workStatus = status;
        task.content = content;
        return task;
    }

private slots:
    void validationRejectsMissingProduct()
    {
        const QList<Task> tasks{makeTask(1, "Task A", TaskWorkStatus::NotStarted, "<p>note</p>")};
        const QList<int> selectedIds{1};
        QCOMPARE(
            validateTaskSummaryExportInputs(-1, tasks, selectedIds),
            TaskSummaryExportIssue::NoProductSelected);
    }

    void validationRejectsNoActiveTasks()
    {
        const QList<Task> noTasks;
        const QList<int> selectedIds;
        QCOMPARE(
            validateTaskSummaryExportInputs(10, noTasks, selectedIds),
            TaskSummaryExportIssue::NoActiveTasks);
    }

    void summaryStripsHtmlNormalizesWhitespaceAndTruncates()
    {
        const QString html =
            "<h1>Hello</h1><p>line&nbsp;one</p><p>line two with more words for truncation</p>";
        const QString summary = summarizeTaskContentForMarkdown(html, 20);
        QCOMPARE(summary, QString("Hello line one line…"));
    }

    void markdownIncludesHeaderAndRows()
    {
        const QDateTime ts(QDate(2026, 8, 20), QTime(13, 45, 0), Qt::UTC);
        const QList<Task> tasks{
            makeTask(1, "Task A", TaskWorkStatus::Ongoing, "<p>Build export feature</p>")
        };
        const QString markdown = buildTaskSummaryMarkdown("Nexus Product", ts, tasks, 120);

        QVERIFY(markdown.contains("# Task Summary Export"));
        QVERIFY(markdown.contains("**Product:** Nexus Product"));
        QVERIFY(markdown.contains("| Status | Title | Summary |"));
        QVERIFY(markdown.contains("| Ongoing | Task A | Build export feature |"));
    }

    void markdownEscapesPipeCharactersInCells()
    {
        const QDateTime ts(QDate(2026, 8, 20), QTime(13, 45, 0), Qt::UTC);
        const QList<Task> tasks{
            makeTask(2, "Task | B", TaskWorkStatus::Paused, "<p>a | b</p>")
        };
        const QString markdown = buildTaskSummaryMarkdown("Product", ts, tasks, 120);
        QVERIFY(markdown.contains("| Paused | Task \\| B | a \\| b |"));
    }

    void dialogPreselectsAllTasksByDefault()
    {
        QList<Task> tasks{
            makeTask(11, "T1", TaskWorkStatus::NotStarted, "<p>a</p>"),
            makeTask(12, "T2", TaskWorkStatus::Completed, "<p>b</p>")
        };
        ExportTaskSummaryDialog dialog(tasks);
        QCOMPARE(dialog.selectedTaskIds(), QList<int>({11, 12}));
    }

    void dialogReturnsOnlyCheckedTaskIds()
    {
        QList<Task> tasks{
            makeTask(11, "T1", TaskWorkStatus::NotStarted, "<p>a</p>"),
            makeTask(12, "T2", TaskWorkStatus::Completed, "<p>b</p>")
        };
        ExportTaskSummaryDialog dialog(tasks);
        auto *list = dialog.findChild<QListWidget *>("exportTaskList");
        QVERIFY(list);
        list->item(1)->setCheckState(Qt::Unchecked);
        QCOMPARE(dialog.selectedTaskIds(), QList<int>({11}));
    }
};

QTEST_MAIN(TaskSummaryMarkdownExporterTests)
#include "TaskSummaryMarkdownExporterTests.moc"
