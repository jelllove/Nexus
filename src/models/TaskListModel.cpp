#include "TaskListModel.h"
#include "db/DatabaseManager.h"

TaskListModel::TaskListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TaskListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_tasks.size();
}

QVariant TaskListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_tasks.size())
        return QVariant();

    const Task &t = m_tasks[index.row()];

    switch (role) {
        case Qt::DisplayRole:
        case TitleRole:
            return t.title;
        case IdRole:
            return t.id;
        case ContentRole:
            return t.content;
        case PriorityRole:
            return static_cast<int>(t.priority);
        case PriorityColorRole:
            return Task::priorityColor(t.priority);
        case PriorityTextRole:
            return Task::priorityToString(t.priority);
        case StatusRole:
            return (t.status == TaskStatus::Active) ? "active" : "archived";
        case CreatedAtRole:
            return t.createdAt;
        case UpdatedAtRole:
            return t.updatedAt;
        case DueDateRole:
            return t.dueDate;
        case DueDateIconRole:
            return Task::dueDateIcon(t.id);
        case CompletedRole:
            return t.completed;
    }
    return QVariant();
}

void TaskListModel::loadTasks(int productId, TaskStatus status)
{
    m_currentProductId = productId;
    m_currentStatus = status;
    beginResetModel();
    m_tasks = DatabaseManager::instance().getTasksForProduct(productId, status);
    endResetModel();
}

void TaskListModel::loadSearchResults(const QList<Task> &tasks)
{
    beginResetModel();
    m_tasks = tasks;
    endResetModel();
}

int TaskListModel::taskIdAt(int row) const
{
    if (row < 0 || row >= m_tasks.size()) return -1;
    return m_tasks[row].id;
}

int TaskListModel::rowForTaskId(int taskId) const
{
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].id == taskId) return i;
    }
    return -1;
}

Task TaskListModel::taskAt(int row) const
{
    if (row < 0 || row >= m_tasks.size()) return Task();
    return m_tasks[row];
}

void TaskListModel::addTask(int productId, const QString &title, const QDateTime &dueDate)
{
    int id = DatabaseManager::instance().addTask(productId, title, dueDate);
    if (id > 0) {
        refresh();
    }
}

void TaskListModel::removeTask(int row)
{
    if (row < 0 || row >= m_tasks.size()) return;
    int id = m_tasks[row].id;
    if (DatabaseManager::instance().deleteTask(id)) {
        refresh();
    }
}

void TaskListModel::refresh()
{
    if (m_currentProductId > 0) {
        loadTasks(m_currentProductId, m_currentStatus);
    }
}
