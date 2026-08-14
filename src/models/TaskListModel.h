#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QSet>
#include "models/EditorTarget.h"
#include "models/SearchResult.h"
#include "models/Task.h"

struct DisplayRow {
    enum Type { MainTask, SubTaskRow };
    Type type = MainTask;
    Task task;
    SubTask subtask;
};

class TaskListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        ContentRole,
        PriorityRole,
        PriorityColorRole,
        PriorityTextRole,
        StatusRole,
        CreatedAtRole,
        UpdatedAtRole,
        DueDateRole,
        DueDateIconRole,
        WorkStatusRole,
        WorkStatusIconRole,
        IsSubTaskRole,
        SubTaskCompletedRole,
        SubTaskIdRole,
        ParentTaskIdRole,
        HasSubTasksRole,
        IsExpandedRole,
        IsActiveTaskRole
    };

    explicit TaskListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    Qt::DropActions supportedDropActions() const override;
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                  const QModelIndex &destinationParent, int destinationRow);
    bool canDropAt(int fromRow, int toRow) const;

    void loadTasks(int productId, TaskStatus status = TaskStatus::Active);
    void loadDeletedTasks();
    void loadSearchResults(const QList<SearchResult> &results);
    int taskIdAt(int row) const;
    int rowForTaskId(int taskId) const;
    EditorTarget targetAt(int row) const;
    int rowForTarget(const EditorTarget &target) const;
    Task taskAt(int row) const;
    int addTask(int productId, const QString &title, TaskPriority priority = TaskPriority::Medium, const QDateTime &dueDate = QDateTime());
    void removeTask(int row);
    void refresh();

    void toggleExpand(int taskId);
    bool isExpanded(int taskId) const;

    void setActiveTarget(const EditorTarget &target) { m_activeTarget = target; }
    EditorTarget activeTarget() const { return m_activeTarget; }

    int currentProductId() const { return m_currentProductId; }
    TaskStatus currentStatus() const { return m_currentStatus; }

private:
    void rebuildDisplayRows();

    QList<Task> m_tasks;
    QList<DisplayRow> m_displayRows;
    QSet<int> m_expandedTasks;
    EditorTarget m_activeTarget;
    int m_currentProductId = -1;
    TaskStatus m_currentStatus = TaskStatus::Active;
};
