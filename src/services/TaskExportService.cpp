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

    for (auto it = groupedProducts.cbegin(); it != groupedProducts.cend(); ++it) {
        const QString productName = it.key();

        stream << "<details open>\n";
        stream << "<summary>📚 " << productName.toHtmlEscaped() << "</summary>\n\n";

        for (const ExportTaskItem &item : it.value()) {
            const QString taskTitle = normalizeHeading(item.title, "📝 Untitled Task");
            const QString workStatusText = item.workStatusText.trimmed().isEmpty()
                ? QString("Not Started")
                : item.workStatusText.trimmed();
            const QString workStatusIcon = item.workStatusIcon.trimmed().isEmpty()
                ? QString::fromUtf8("⏯️")
                : item.workStatusIcon.trimmed();

            stream << "<details>\n";
            stream << "<summary>" << (workStatusIcon + " " + taskTitle).toHtmlEscaped() << "</summary>\n\n";
            stream << "- **Status:** " << (workStatusIcon + " " + workStatusText).toHtmlEscaped() << "\n\n";

            if (!item.simpleDescription.trimmed().isEmpty()) {
                stream << "> 🧠 **AI Summary**\n";
                stream << ">\n";
                stream << "> ✨ " << item.simpleDescription.trimmed() << "\n\n";
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
                stream << "#### 🪜 Selected Sub Tasks\n\n";
                for (const ExportSubTaskItem &subtask : item.subtasks) {
                    const QString subTitle = normalizeHeading(subtask.title, "Untitled sub task");
                    stream << "- [" << (subtask.completed ? "x" : " ") << "] "
                           << (subtask.completed ? "✅ " : "⬜ ")
                           << subTitle << "\n";
                }
                stream << "\n";
            }

            stream << "</details>\n\n";
        }

        stream << "</details>\n\n";
    }

    return markdown;
}
