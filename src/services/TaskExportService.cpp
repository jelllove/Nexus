#include "TaskExportService.h"
#include <QRegularExpression>
#include <QTextStream>

namespace {

QString escapeTableCell(const QString &value)
{
    QString escaped = value;
    escaped.replace("\r\n", "\n");
    escaped.replace("\r", "\n");
    escaped.replace("\n", "<br>");
    escaped.replace("|", "\\|");
    return escaped;
}

QString normalizeHeading(const QString &value, const QString &fallback)
{
    QString text = value.trimmed();
    if (text.isEmpty()) {
        return fallback;
    }
    return text;
}

QString statusEmoji(const QString &statusText)
{
    QString normalized = statusText.trimmed().toLower();
    if (normalized == "active") {
        return "🟢";
    }
    if (normalized == "archived") {
        return "🟡";
    }
    if (normalized == "deleted") {
        return "🔴";
    }
    return "🔵";
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
    int subTaskCount = 0;
    for (const ExportTaskItem &task : tasks) {
        subTaskCount += task.subtasks.size();
    }

    QString markdown;
    QTextStream stream(&markdown);
    stream << "# 📦 Task Export\n\n";
    stream << "- 🕒 Exported at: " << exportedAt.toString("yyyy-MM-dd HH:mm:ss") << "\n";
    stream << "- 📌 Main tasks: " << tasks.size() << "\n";
    stream << "- 🧩 Sub tasks: " << subTaskCount << "\n\n";

    if (tasks.isEmpty()) {
        stream << "> ℹ️ No tasks selected.\n";
        return markdown;
    }

    QString currentProduct;
    for (const ExportTaskItem &item : tasks) {
        const QString productName = normalizeHeading(item.productName, "📁 Unknown Product");
        const QString taskTitle = normalizeHeading(item.title, "📝 Untitled Task");
        const QString status = item.statusText.trimmed().isEmpty() ? "Active" : item.statusText.trimmed();

        if (productName != currentProduct) {
            currentProduct = productName;
            stream << "## 📚 " << currentProduct << "\n\n";
        }

        stream << "### " << statusEmoji(status) << " " << taskTitle << "\n\n";
        stream << "| Field | Value |\n";
        stream << "|---|---|\n";
        stream << "| Status | " << escapeTableCell(status) << " |\n";
        stream << "| Title | " << escapeTableCell(taskTitle) << " |\n";
        stream << "| Updated | " << escapeTableCell(item.updatedAt.isValid() ? item.updatedAt.toString("yyyy-MM-dd HH:mm") : "N/A") << " |\n\n";

        if (!item.simpleDescription.trimmed().isEmpty()) {
            stream << "> 🧠 " << item.simpleDescription.trimmed() << "\n\n";
        }

        const QString previewText = fallbackSimpleDescription(item.contentHtml, 240);
        if (!previewText.trimmed().isEmpty()) {
            stream << "<details>\n";
            stream << "<summary>📝 Content Preview</summary>\n\n";
            stream << previewText << "\n\n";
            stream << "</details>\n\n";
        }

        if (item.subtasks.isEmpty()) {
            stream << "- 💤 No sub tasks\n\n";
        } else {
            stream << "#### 🪜 Sub Tasks\n\n";
            for (const ExportSubTaskItem &subtask : item.subtasks) {
                const QString subTitle = normalizeHeading(subtask.title, "Untitled sub task");
                stream << "- [" << (subtask.completed ? "x" : " ") << "] "
                       << (subtask.completed ? "✅ " : "⬜ ")
                       << subTitle << "\n";
            }
            stream << "\n";
        }

        stream << "---\n\n";
    }

    return markdown;
}

