#include "TaskExportService.h"
#include <QHash>
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

QString normalizeLine(const QString &value, const QString &fallback = QString())
{
    const QString text = value.simplified();
    if (text.isEmpty()) {
        return fallback;
    }
    return text;
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

    QHash<QString, QList<ExportTaskItem>> groupedProducts;
    QList<QString> productOrder;
    for (const ExportTaskItem &item : tasks) {
        const QString productKey = normalizeHeading(item.productName, "📁 Unknown Product");
        if (!groupedProducts.contains(productKey)) {
            productOrder.append(productKey);
        }
        groupedProducts[productKey].append(item);
    }

    for (const QString &productName : productOrder) {
        stream << "## 📚 " << normalizeLine(productName, "📁 Unknown Product") << "\n\n";

        const QList<ExportTaskItem> &productTasks = groupedProducts[productName];
        for (const ExportTaskItem &item : productTasks) {
            const QString taskTitle = normalizeHeading(item.title, "📝 Untitled Task");
            const QString workStatusText = item.workStatusText.trimmed().isEmpty()
                ? QString("Not Started")
                : item.workStatusText.trimmed();
            const QString workStatusIcon = item.workStatusIcon.trimmed().isEmpty()
                ? QString::fromUtf8("⏯️")
                : item.workStatusIcon.trimmed();

            stream << "- " << normalizeLine(workStatusIcon, "⏯️")
                   << " **" << normalizeLine(taskTitle, "Untitled Task") << "**\n";
            stream << "  - **Status:** "
                   << normalizeLine(workStatusIcon + " " + workStatusText, "⏯️ Not Started")
                   << "\n";

            if (!item.simpleDescription.trimmed().isEmpty()) {
                stream << "  - 🧠 **AI Summary:** "
                       << normalizeLine(item.simpleDescription)
                       << "\n";
            }

            const QString previewText = fallbackSimpleDescription(item.contentHtml, 240);
            if (!previewText.trimmed().isEmpty()) {
                stream << "  - 📝 **Content Preview:** "
                       << normalizeLine(previewText, "📝 (empty)")
                       << "\n";
            }

            if (item.subtasks.isEmpty()) {
                stream << "  - 💤 **Sub Tasks:** (none)\n";
            } else {
                stream << "  - 🪜 **Sub Tasks:**\n";
                for (const ExportSubTaskItem &subtask : item.subtasks) {
                    const QString subTitle = normalizeHeading(subtask.title, "Untitled sub task");
                    stream << "    - [" << (subtask.completed ? "x" : " ") << "] "
                           << (subtask.completed ? "✅ " : "⬜ ")
                           << normalizeLine(subTitle, "Untitled sub task")
                           << "\n";
                }
            }

            stream << "\n";
        }
        stream << "\n";
    }

    return markdown;
}
