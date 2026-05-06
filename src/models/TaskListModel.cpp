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
        case WorkStatusRole:
            return static_cast<int>(t.workStatus);
        case WorkStatusIconRole:
            return Task::workStatusIcon(t.workStatus);
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

void TaskListModel::loadDeletedTasks()
{
    m_currentProductId = -1;
    m_currentStatus = TaskStatus::Deleted;
    beginResetModel();
    m_tasks = DatabaseManager::instance().getDeletedTasks();
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

int TaskListModel::addTask(int productId, const QString &title, TaskPriority priority, const QDateTime &dueDate)
{
    int id = DatabaseManager::instance().addTask(productId, title, priority, dueDate);
    if (id > 0) {
        refresh();
    }
    return id;
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
    if (m_currentStatus == TaskStatus::Deleted) {
        loadDeletedTasks();
    } else if (m_currentProductId > 0) {
        loadTasks(m_currentProductId, m_currentStatus);
    }
}

Qt::ItemFlags TaskListModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QAbstractListModel::flags(index);
    if (index.isValid()) {
        return defaultFlags | Qt::ItemIsDragEnabled;
    }
    return defaultFlags | Qt::ItemIsDropEnabled;
}

Qt::DropActions TaskListModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

bool TaskListModel::canDropAt(int fromRow, int toRow) const
{
    if (fromRow < 0 || fromRow >= m_tasks.size()) return false;
    if (toRow < 0 || toRow > m_tasks.size()) return false;

    int fromPriority = static_cast<int>(m_tasks[fromRow].priority);

    // Determine the priority at the drop position
    int targetPriority;
    if (toRow == m_tasks.size()) {
        targetPriority = static_cast<int>(m_tasks[toRow - 1].priority);
    } else if (toRow == fromRow || toRow == fromRow + 1) {
        return true; // No-op move
    } else {
        targetPriority = static_cast<int>(m_tasks[toRow].priority);
    }

    return fromPriority == targetPriority;
}

bool TaskListModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                              const QModelIndex &destinationParent, int destinationRow)
{
    Q_UNUSED(sourceParent);
    Q_UNUSED(destinationParent);
    if (count != 1) return false;
    if (!canDropAt(sourceRow, destinationRow)) return false;
    if (destinationRow == sourceRow || destinationRow == sourceRow + 1) return true;

    int destRow = destinationRow > sourceRow ? destinationRow - 1 : destinationRow;

    beginMoveRows(QModelIndex(), sourceRow, sourceRow,
                  QModelIndex(), destinationRow);
    m_tasks.move(sourceRow, destRow);
    endMoveRows();

    // Persist the new order for tasks with the same priority
    int priority = static_cast<int>(m_tasks[destRow].priority);
    QList<int> sameGroupIds;
    for (const auto &t : m_tasks) {
        if (static_cast<int>(t.priority) == priority) {
            sameGroupIds.append(t.id);
        }
    }
    DatabaseManager::instance().reorderTasks(sameGroupIds);

    return true;
}
