#include "TaskExportService.h"
#include <QMap>
#include <QRegularExpression>
#include <QTextStream>

namespace {

QString normalizeHeading(const QString &value, const QString &fallback)
{
    QString text = value.trimmed();
    if (text.isEmpty()) {
        return fallback;
    }
    return text;
}

TaskPriority normalizedPriority(TaskPriority priority)
{
    switch (priority) {
        case TaskPriority::Critical:
        case TaskPriority::High:
        case TaskPriority::Medium:
        case TaskPriority::Low:
            return priority;
    }
    return TaskPriority::Medium;
}

QString priorityCode(TaskPriority priority)
{
    switch (priority) {
        case TaskPriority::Critical:
            return "P0";
        case TaskPriority::High:
            return "P1";
        case TaskPriority::Medium:
            return "P2";
        case TaskPriority::Low:
            return "P3";
    }
    return "P2";
}

QString priorityEmoji(TaskPriority priority)
{
    switch (priority) {
        case TaskPriority::Critical:
            return QString::fromUtf8("🔴");
        case TaskPriority::High:
            return QString::fromUtf8("🟠");
        case TaskPriority::Medium:
            return QString::fromUtf8("🔵");
        case TaskPriority::Low:
            return QString::fromUtf8("⚪");
    }
    return QString::fromUtf8("🔵");
}

QString mainTaskPriorityTitleHtml(TaskPriority priority,
                                  const QString &workStatusIcon,
                                  const QString &title)
{
    const QColor bgColor = Task::priorityBackgroundColor(priority);
    const QColor accentColor = Task::priorityColor(priority);
    const QString safeTitle = normalizeHeading(title, "📝 Untitled Task").toHtmlEscaped();
    const QString safeWorkIcon = workStatusIcon.trimmed().isEmpty()
        ? Task::workStatusIcon(TaskWorkStatus::NotStarted)
        : workStatusIcon.trimmed();
    return QString(
               "<span style=\"display:inline-block; background-color:%1; color:#2c3e50; "
               "padding:2px 8px; border-radius:6px;\">"
               "%3 %4 <span style=\"color:%2;\"><strong>[%5]</strong></span>"
               "</span>")
        .arg(bgColor.name(QColor::HexRgb),
             accentColor.name(QColor::HexRgb),
             safeWorkIcon,
             safeTitle,
             priorityCode(priority));
}

QString subtaskStatusIcon(const ExportSubTaskItem &subtask)
{
    if (!subtask.workStatusIcon.trimmed().isEmpty()) {
        return subtask.workStatusIcon.trimmed();
    }
    return Task::workStatusIcon(subtask.workStatus);
}

} // namespace

QString TaskExportService::extractPlainText(const QString &html)
{
    QString text = html;
    text.replace(QRegularExpression("(?i)<\\s*br\\s*/?>"), "\n");
    text.replace(QRegularExpression("(?i)</\\s*p\\s*>"), "\n");
    text.replace(QRegularExpression("(?i)</\\s*li\\s*>"), "\n");
    text.remove(QRegularExpression("<[^>]*>"));
    text.replace("&nbsp;", " ");
    text.replace("&amp;", "&");
    text.replace("&lt;", "<");
    text.replace("&gt;", ">");
    text.replace("&quot;", "\"");
    text.replace("&#39;", "'");
    return text.simplified();
}

QString TaskExportService::fallbackSimpleDescription(const QString &html, int maxChars)
{
    const QString plain = extractPlainText(html);
    if (plain.isEmpty()) {
        return "📝 (empty)";
    }
    if (plain.length() <= maxChars) {
        return plain;
    }
    return plain.left(maxChars) + "...";
}

QString TaskExportService::buildMarkdown(const QList<ExportTaskItem> &tasks, const QDateTime &exportedAt)
{
    QString markdown;
    QTextStream stream(&markdown);
    stream << "# 📦 Task Export\n\n";
    stream << "> Exported at " << exportedAt.toString("yyyy-MM-dd HH:mm:ss") << "\n\n";

    if (tasks.isEmpty()) {
        stream << "> ℹ️ No tasks selected.\n";
        return markdown;
    }

    QMap<QString, QList<ExportTaskItem>> groupedProducts;
    for (const ExportTaskItem &item : tasks) {
        const QString productKey = normalizeHeading(item.productName, "📁 Unknown Product");
        groupedProducts[productKey].append(item);
    }

    bool hasPrintedProduct = false;
    for (auto it = groupedProducts.cbegin(); it != groupedProducts.cend(); ++it) {
        if (hasPrintedProduct) {
            stream << "\n\n\n---\n---\n\n\n";
        }
        hasPrintedProduct = true;

        const QString productName = it.key();
        stream << "## 📚 " << productName << "\n\n";

        QMap<int, QList<ExportTaskItem>> groupedPriorities;
        for (const ExportTaskItem &item : it.value()) {
            const TaskPriority priority = normalizedPriority(item.priority);
            groupedPriorities[static_cast<int>(priority)].append(item);
        }

        const QList<TaskPriority> priorityOrder = {
            TaskPriority::Critical,
            TaskPriority::High,
            TaskPriority::Medium,
            TaskPriority::Low
        };

        bool hasPrintedPriorityGroup = false;
        for (TaskPriority priority : priorityOrder) {
            const int priorityKey = static_cast<int>(priority);
            if (!groupedPriorities.contains(priorityKey) ||
                groupedPriorities.value(priorityKey).isEmpty()) {
                continue;
            }

            if (hasPrintedPriorityGroup) {
                stream << "\n---\n\n";
            }
            hasPrintedPriorityGroup = true;

            stream << "### " << priorityEmoji(priority) << " "
                   << Task::priorityToString(priority) << "\n\n";

            for (const ExportTaskItem &item : groupedPriorities.value(priorityKey)) {
                const QString taskTitle = normalizeHeading(item.title, "📝 Untitled Task");
                const QString workStatusText = item.workStatusText.trimmed().isEmpty()
                    ? QString("Not Started")
                    : item.workStatusText.trimmed();
                const QString workStatusIcon = item.workStatusIcon.trimmed().isEmpty()
                    ? QString::fromUtf8("⏯️")
                    : item.workStatusIcon.trimmed();

                stream << "- " << mainTaskPriorityTitleHtml(priority, workStatusIcon, taskTitle) << "\n";
                stream << "  - **Status:** " << workStatusIcon << " " << workStatusText << "\n";

                if (!item.simpleDescription.trimmed().isEmpty()) {
                    stream << "  - 🧠 **AI Summary:** " << item.simpleDescription.trimmed() << "\n";
                }

                const QString previewText = fallbackSimpleDescription(item.contentHtml, 240);
                if (!previewText.trimmed().isEmpty()) {
                    stream << "  - 📝 **Content Preview:** " << previewText << "\n";
                }

                if (!item.subtasks.isEmpty()) {
                    stream << "  - 🪜 **Selected Sub Tasks:**\n";
                    for (const ExportSubTaskItem &subtask : item.subtasks) {
                        const QString subTitle = normalizeHeading(subtask.title, "Untitled sub task");
                        stream << "    - " << subtaskStatusIcon(subtask) << " " << subTitle << "\n";
                    }
                }

                stream << "\n";
            }
        }

        stream << "\n";
    }

    return markdown;
}
