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
        DueDateIconRole
    };

    explicit TaskListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void loadTasks(int productId, TaskStatus status = TaskStatus::Active);
    void loadSearchResults(const QList<Task> &tasks);
    int taskIdAt(int row) const;
    int rowForTaskId(int taskId) const;
    Task taskAt(int row) const;
    void addTask(int productId, const QString &title, const QDateTime &dueDate = QDateTime());
    void removeTask(int row);
    void refresh();

    int currentProductId() const { return m_currentProductId; }
    TaskStatus currentStatus() const { return m_currentStatus; }

private:
    QList<Task> m_tasks;
    int m_currentProductId = -1;
    TaskStatus m_currentStatus = TaskStatus::Active;
};
