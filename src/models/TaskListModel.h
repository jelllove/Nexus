#pragma once

#include <QAbstractListModel>
#include <QList>
#include "models/Task.h"

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
        WorkStatusIconRole
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
    void loadSearchResults(const QList<Task> &tasks);
    int taskIdAt(int row) const;
    int rowForTaskId(int taskId) const;
    Task taskAt(int row) const;
    int addTask(int productId, const QString &title, TaskPriority priority = TaskPriority::Medium, const QDateTime &dueDate = QDateTime());
    void removeTask(int row);
    void refresh();

    int currentProductId() const { return m_currentProductId; }
    TaskStatus currentStatus() const { return m_currentStatus; }

private:
    QList<Task> m_tasks;
    int m_currentProductId = -1;
    TaskStatus m_currentStatus = TaskStatus::Active;
};
