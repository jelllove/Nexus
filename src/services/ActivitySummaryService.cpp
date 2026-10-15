#include "ActivitySummaryService.h"
#include "TaskExportService.h"
#include "db/DatabaseManager.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

QDate ActivitySummaryService::presetStart(int preset, const QDate &today)
{
    switch (preset) {
    case 0: return today;
    case 1: return today.addDays(-6);
    case 2: return today.addDays(-13);
    case 3: return today.addMonths(-1).addDays(1);
    case 4: return today.addMonths(-3).addDays(1);
    default:
        qWarning() << "Invalid activity period preset:" << preset;
        return {};
    }
}

bool ActivitySummaryService::prepare(const QDate &start, const QDate &endInclusive,
                                    ActivityTimeBasis basis, QString &payload, QString &error)
{
    payload.clear();
    error.clear();
    auto fail = [&](const QString &message) {
        error = message;
        qWarning() << "Activity summary:" << message;
        return false;
    };
    if (!start.isValid() || !endInclusive.isValid() || start > endInclusive
        || !endInclusive.addDays(1).isValid())
        return fail("Invalid date range. The start date must not follow the end date.");
    const auto startUtc = start.startOfDay().toUTC();
    const auto endUtc = endInclusive.addDays(1).startOfDay().toUTC();
    QList<TaskActivity> items;
    if (!DatabaseManager::instance().collectTaskActivity(startUtc, endUtc, basis, items, error))
        return false;
    if (items.isEmpty()) return fail("No tasks or subtasks match the selected dates and time basis.");

    QJsonArray records;
    for (const auto &item : items) {
        QJsonArray changes;
        for (const auto &change : item.changes) {
            changes.append(QJsonObject{
                {"atUtc", change.occurredAt.toString(Qt::ISODateWithMs)},
                {"operation", change.operation}, {"changedFields", change.changedFields},
                {"oldLifecycleStatus", change.oldStatus}, {"newLifecycleStatus", change.newStatus},
                {"oldWorkStatus", change.oldWorkStatus}, {"newWorkStatus", change.newWorkStatus}});
        }
        records.append(QJsonObject{
            {"kind", item.tableName}, {"id", item.id}, {"product", item.productName},
            {"parentTask", item.parentTitle}, {"title", item.title},
            {"currentContent", TaskExportService::extractPlainText(item.content)},
            {"createdAtUtc", item.createdAt.toString(Qt::ISODateWithMs)},
            {"lastUpdatedAtUtc", item.updatedAt.toString(Qt::ISODateWithMs)},
            {"currentLifecycleStatus", item.lifecycleStatus}, {"currentWorkStatus", item.workStatus},
            {"eventsInRange", changes}});
    }
    const QString basisText = basis == ActivityTimeBasis::Created ? "created"
        : basis == ActivityTimeBasis::Modified ? "modified" : "created or modified";
    const QJsonObject source{
        {"localStartDate", start.toString(Qt::ISODate)},
        {"localEndDateInclusive", endInclusive.toString(Qt::ISODate)},
        {"startUtc", startUtc.toString(Qt::ISODateWithMs)},
        {"endUtcExclusive", endUtc.toString(Qt::ISODateWithMs)},
        {"timeBasis", basisText},
        {"limitations", "Content, titles, product names and current statuses are current saved values, "
                       "not historical snapshots. Only eventsInRange prove historical status transitions; "
                       "event history begins with schema v10 and is not backfilled. "
                       "Permanently deleted records cannot be reconstructed. "
                       "Modification can include ordering or metadata changes, not necessarily content edits. "
                       "Do not infer completion dates or work performed from current status alone."},
        {"workStatusLegend", "0 Not Started; 1 Ongoing; 2 Paused; 3 Completed; 4 Waiting; -1 unknown"},
        {"items", records}};
    const QString result = QString::fromUtf8(QJsonDocument(source).toJson(QJsonDocument::Indented));
    if (result.size() > MaximumPayloadCharacters)
        return fail("The summary input is too large (over 60000 characters). Choose a shorter range. "
                    "No items or content have been silently omitted.");
    payload = result;
    return true;
}
