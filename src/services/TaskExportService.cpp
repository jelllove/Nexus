#include "TaskExportService.h"
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>

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

QString statusColor(const QString &statusText)
{
    QString normalized = statusText.trimmed().toLower();
    if (normalized == "active") {
        return "22c55e";
    }
    if (normalized == "archived") {
        return "f59e0b";
    }
    if (normalized == "deleted") {
        return "ef4444";
    }
    return "0ea5e9";
}

QString workStatusColor(const QString &workStatusText)
{
    QString normalized = workStatusText.trimmed().toLower();
    if (normalized == "completed") {
        return "16a34a";
    }
    if (normalized == "ongoing") {
        return "2563eb";
    }
    if (normalized == "paused") {
        return "f59e0b";
    }
    if (normalized == "waiting") {
        return "9333ea";
    }
    return "64748b";
}

QString makeBadge(const QString &label, const QString &message, const QString &color)
{
    const QString encodedLabel = QString::fromUtf8(QUrl::toPercentEncoding(label));
    const QString encodedMessage = QString::fromUtf8(QUrl::toPercentEncoding(message));
    return QString("![%1](https://img.shields.io/badge/%2-%3-%4?style=flat-square)")
        .arg(label, encodedLabel, encodedMessage, color);
}

int completedSubtaskCount(const QList<ExportSubTaskItem> &subtasks)
{
    int done = 0;
    for (const ExportSubTaskItem &subtask : subtasks) {
        if (subtask.completed) {
            ++done;
        }
    }
    return done;
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
    stream << makeBadge("Main Tasks", QString::number(tasks.size()), "0ea5e9") << " "
           << makeBadge("Sub Tasks", QString::number(subTaskCount), "f97316") << " "
           << makeBadge("Exported", exportedAt.toString("yyyy-MM-dd HH:mm:ss"), "64748b") << "\n\n";
    stream << "> 🎨 **Legend**: 🟢 Active · 🏃 Ongoing · ✅ Completed · ⏸️ Paused · ⏳ Waiting\n\n";

    if (tasks.isEmpty()) {
        stream << "> ℹ️ No tasks selected.\n";
        return markdown;
    }

    QString currentProduct;
    for (const ExportTaskItem &item : tasks) {
        const QString productName = normalizeHeading(item.productName, "📁 Unknown Product");
        const QString taskTitle = normalizeHeading(item.title, "📝 Untitled Task");
        const QString status = item.statusText.trimmed().isEmpty() ? "Active" : item.statusText.trimmed();
        const QString statusWithEmoji = statusEmoji(status) + " " + status;
        const QString workStatusText = item.workStatusText.trimmed().isEmpty()
            ? QString("Not Started")
            : item.workStatusText.trimmed();
        const QString workStatusIcon = item.workStatusIcon.trimmed().isEmpty()
            ? QString::fromUtf8("⏯️")
            : item.workStatusIcon.trimmed();
        const int doneSubs = completedSubtaskCount(item.subtasks);
        const int totalSubs = item.subtasks.size();
        const QString subBadgeColor = (totalSubs > 0 && doneSubs == totalSubs) ? "16a34a" : "f59e0b";

        if (productName != currentProduct) {
            currentProduct = productName;
            stream << "## 📚 " << currentProduct << "\n\n";
            stream << makeBadge("Product", currentProduct, "6366f1") << "\n\n";
        }

        stream << "### " << workStatusIcon << " " << taskTitle << "\n\n";
        stream << makeBadge("Task Status", statusWithEmoji, statusColor(status)) << " "
               << makeBadge("Work", workStatusText, workStatusColor(workStatusText)) << " "
               << makeBadge("Subs", QString("%1/%2 done").arg(doneSubs).arg(totalSubs), subBadgeColor)
               << "\n\n";
        stream << "| Field | Value |\n";
        stream << "|---|---|\n";
        stream << "| Status | " << escapeTableCell(statusWithEmoji) << " |\n";
        stream << "| Title | " << escapeTableCell(taskTitle) << " |\n";
        stream << "| Work | " << escapeTableCell(workStatusIcon + " " + workStatusText) << " |\n";
        stream << "| Updated | " << escapeTableCell(item.updatedAt.isValid() ? item.updatedAt.toString("yyyy-MM-dd HH:mm") : "N/A") << " |\n\n";

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

        stream << "---\n\n";
    }

    return markdown;
}
