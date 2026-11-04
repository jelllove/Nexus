#pragma once

#include "Product.h"
#include "Task.h"
#include <QList>

enum class TaskCreationKind {
    NewTask = 0,
    Subtask = 1,
    NewProject = 2
};

struct TaskCreationCatalog {
    QList<Product> projects;
    QList<Task> tasks;
};

struct TaskCreationDraft {
    static constexpr int MaximumTitleCharacters = 160;
    static constexpr int MaximumContextCharacters = 16000;
    static constexpr int MaximumProjectNameCharacters = 120;

    TaskCreationKind kind = TaskCreationKind::NewTask;
    int projectId = -1;
    int parentTaskId = -1;
    QString projectName;
    QString title;
    QString context;
    TaskPriority priority = TaskPriority::High;

    QString validationError(bool requireDestination = true) const
    {
        if (kind != TaskCreationKind::NewTask && kind != TaskCreationKind::Subtask
            && kind != TaskCreationKind::NewProject)
            return "Choose a valid creation type.";
        if (title.trimmed().isEmpty() || title.trimmed().size() > MaximumTitleCharacters)
            return QString("Enter a title of 1-%1 characters.").arg(MaximumTitleCharacters);
        if (context.trimmed().isEmpty() || context.trimmed().size() > MaximumContextCharacters)
            return QString("Enter context of 1-%1 characters.").arg(MaximumContextCharacters);
        if (priority < TaskPriority::Critical || priority > TaskPriority::Low)
            return "Choose a valid priority.";
        if (kind == TaskCreationKind::NewProject) {
            if (projectName.trimmed().isEmpty()
                || projectName.trimmed().size() > MaximumProjectNameCharacters)
                return QString("Enter a new project name of 1-%1 characters.").arg(MaximumProjectNameCharacters);
            if (projectId > 0 || parentTaskId > 0)
                return "A new project cannot use an existing project or parent task.";
        } else {
            if (!projectName.trimmed().isEmpty())
                return "Select an existing project instead of entering a new project name.";
            if (requireDestination && projectId <= 0)
                return "Choose a project.";
            if (kind == TaskCreationKind::NewTask && parentTaskId > 0)
                return "A new main task cannot have a parent task.";
            if (kind == TaskCreationKind::Subtask && requireDestination && parentTaskId <= 0)
                return "Choose a parent task.";
        }
        return {};
    }
};

struct TaskCreationResult {
    int projectId = -1;
    int taskId = -1;
    int subtaskId = -1;
};
