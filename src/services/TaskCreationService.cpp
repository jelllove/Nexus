#include "TaskCreationService.h"
#include "TaskExportService.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <cmath>
#include <limits>

bool TaskCreationService::buildPayload(const QString &input, const TaskCreationCatalog &catalog,
                                      QString &payload, QString &error)
{
    payload.clear();
    error.clear();
    const QString text = input.trimmed();
    if (text.isEmpty() || text.size() > MaximumInputCharacters) {
        error = QString("Enter 1-%1 characters before analyzing.").arg(MaximumInputCharacters);
        return false;
    }

    QJsonArray projects;
    QSet<int> projectIds;
    for (const auto &project : catalog.projects) {
        if (project.status != ProductStatus::Active) continue;
        projects.append(QJsonObject{{"id", project.id}, {"name", project.name}});
        projectIds.insert(project.id);
    }
    QJsonArray tasks;
    for (const auto &task : catalog.tasks) {
        if (!projectIds.contains(task.productId) || task.status != TaskStatus::Active
            || task.workStatus == TaskWorkStatus::Completed)
            continue;
        const QString context = TaskExportService::extractPlainText(task.content);
        tasks.append(QJsonObject{
            {"id", task.id}, {"projectId", task.productId}, {"title", task.title},
            {"context", context.left(MaximumNoteExcerptCharacters)},
            {"contextTruncated", context.size() > MaximumNoteExcerptCharacters}
        });
    }
    QJsonObject source{{"input", text}, {"projects", projects}, {"tasks", tasks}, {"notesIncluded", true}};
    auto encode = [&source]() {
        return QString::fromUtf8(QJsonDocument(source).toJson(QJsonDocument::Compact));
    };
    payload = encode();
    if (payload.size() > MaximumPayloadCharacters) {
        QJsonArray titlesOnly;
        for (const auto &value : tasks) {
            auto task = value.toObject();
            task.remove("context");
            task.remove("contextTruncated");
            titlesOnly.append(task);
        }
        source["tasks"] = titlesOnly;
        source["notesIncluded"] = false;
        payload = encode();
    }
    if (payload.size() > MaximumPayloadCharacters) {
        payload.clear();
        error = "The active project/task titles exceed the AI source limit. Use manual setup; no destinations were silently omitted.";
        return false;
    }
    return true;
}

bool TaskCreationService::parseSuggestion(const QString &response, const TaskCreationCatalog &catalog,
                                         TaskCreationSuggestion &suggestion, QString &error)
{
    suggestion = {};
    error.clear();
    auto fail = [&error](const QString &message) {
        error = "Invalid AI suggestion: " + message;
        return false;
    };
    QString text = response.trimmed();
    if (text.size() > MaximumPayloadCharacters)
        return fail("The response is too large.");
    if (text.startsWith("```json\n") && text.endsWith("```"))
        text = text.mid(8, text.size() - 11).trimmed();
    else if (text.startsWith("```\n") && text.endsWith("```"))
        text = text.mid(4, text.size() - 7).trimmed();
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail("Expected one JSON object. Retry or use manual setup.");
    const auto object = document.object();
    for (const auto &key : {"kind", "projectName", "title", "context", "reason"}) {
        if (!object.value(key).isString())
            return fail(QString("Missing or non-text %1.").arg(key));
    }
    if (!object.value("confident").isBool())
        return fail("Missing confidence decision.");

    TaskCreationSuggestion parsed;
    const QString kind = object["kind"].toString();
    if (kind == "subtask") parsed.draft.kind = TaskCreationKind::Subtask;
    else if (kind == "task") parsed.draft.kind = TaskCreationKind::NewTask;
    else if (kind == "project") parsed.draft.kind = TaskCreationKind::NewProject;
    else return fail("Unknown creation type.");

    auto readId = [&object](const QString &key, int &id) {
        const auto value = object.value(key);
        if (value.isNull()) {
            id = -1;
            return true;
        }
        if (!value.isDouble()) return false;
        const double number = value.toDouble();
        if (!std::isfinite(number) || number < 1 || number > std::numeric_limits<int>::max()
            || std::floor(number) != number)
            return false;
        id = static_cast<int>(number);
        return true;
    };
    if (!readId("projectId", parsed.draft.projectId) || !readId("parentTaskId", parsed.draft.parentTaskId))
        return fail("Project and parent IDs must be positive integers or null.");
    parsed.draft.projectName = object["projectName"].toString().trimmed();
    parsed.draft.title = object["title"].toString().trimmed();
    parsed.draft.context = object["context"].toString().trimmed();
    parsed.reason = object["reason"].toString().trimmed();
    parsed.confident = object["confident"].toBool();
    if (parsed.reason.isEmpty() || parsed.reason.size() > 2000)
        return fail("The matching explanation is empty or too long.");
    const QString validation = parsed.draft.validationError(parsed.confident);
    if (!validation.isEmpty()) return fail(validation);

    if (parsed.draft.projectId > 0) {
        bool found = false;
        for (const auto &project : catalog.projects)
            found = found || (project.id == parsed.draft.projectId && project.status == ProductStatus::Active);
        if (!found) return fail("The project ID was not in the supplied active projects.");
    }
    if (parsed.draft.parentTaskId > 0) {
        bool found = false;
        for (const auto &task : catalog.tasks) {
            found = found || (task.id == parsed.draft.parentTaskId && task.productId == parsed.draft.projectId
                              && task.status == TaskStatus::Active && task.workStatus != TaskWorkStatus::Completed);
        }
        if (!found) return fail("The parent ID was not an eligible task in the selected project.");
    }
    if (!parsed.confident) {
        parsed.draft.kind = TaskCreationKind::NewTask;
        parsed.draft.projectId = -1;
        parsed.draft.parentTaskId = -1;
        parsed.draft.projectName.clear();
    }
    suggestion = parsed;
    return true;
}
