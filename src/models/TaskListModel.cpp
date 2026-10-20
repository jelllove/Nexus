#include "TaskListModel.h"
#include "db/DatabaseManager.h"
#include <QHash>

TaskListModel::TaskListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TaskListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_displayRows.size();
}

QVariant TaskListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_displayRows.size())
        return QVariant();

    const DisplayRow &row = m_displayRows[index.row()];

    if (row.type == DisplayRow::SubTaskRow) {
        switch (role) {
            case Qt::DisplayRole:
            case TitleRole:
                return row.subtask.title;
            case Qt::ToolTipRole:
                return flags(index).testFlag(Qt::ItemIsDragEnabled)
                    ? "Drag to reorder, or drop on another main task to move. Esc cancels."
                    : "Restore the main task before moving this subtask.";
            case IsSubTaskRole:
                return true;
            case SubTaskCompletedRole:
                return row.subtask.workStatus == TaskWorkStatus::Completed;
            case SubTaskWorkStatusRole:
                return static_cast<int>(row.subtask.workStatus);
            case SubTaskWorkStatusIconRole:
                return Task::workStatusIcon(row.subtask.workStatus);
            case SubTaskIdRole:
                return row.subtask.id;
            case ParentTaskIdRole:
                return row.subtask.taskId;
            default:
                return QVariant();
        }
    }

    // MainTask
    const Task &t = row.task;
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
            return Task::statusToString(t.status);
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
        case IsSubTaskRole:
            return false;
        case HasSubTasksRole:
            if (m_showCompleted) return DatabaseManager::instance().getSubtaskCount(t.id) > 0;
            for (const auto &subtask : DatabaseManager::instance().getSubtasks(t.id)) {
                if (subtask.workStatus != TaskWorkStatus::Completed) return true;
            }
            return false;
        case IsExpandedRole:
            return m_expandedTasks.contains(t.id);
        case IsActiveTaskRole:
            return t.id == m_activeTaskId;
    }
    return QVariant();
}

void TaskListModel::loadTasks(int productId, TaskStatus status)
{
    m_searchResults = false;
    m_currentProductId = productId;
    m_currentStatus = status;
    beginResetModel();
    m_tasks = DatabaseManager::instance().getTasksForProduct(productId, status);
    rebuildDisplayRows();
    endResetModel();
}

void TaskListModel::loadDeletedTasks()
{
    m_searchResults = false;
    m_currentProductId = -1;
    m_currentStatus = TaskStatus::Deleted;
    beginResetModel();
    m_tasks = DatabaseManager::instance().getDeletedTasks();
    rebuildDisplayRows();
    endResetModel();
}

void TaskListModel::loadSearchResults(const QList<Task> &tasks)
{
    m_searchResults = true;
    beginResetModel();
    m_tasks = tasks;
    rebuildDisplayRows();
    endResetModel();
}

void TaskListModel::rebuildDisplayRows()
{
    m_displayRows.clear();
    for (const Task &t : m_tasks) {
        if (!m_showCompleted && t.workStatus == TaskWorkStatus::Completed) continue;
        DisplayRow mainRow;
        mainRow.type = DisplayRow::MainTask;
        mainRow.task = t;
        m_displayRows.append(mainRow);

        if (m_expandedTasks.contains(t.id)) {
            auto subtasks = DatabaseManager::instance().getSubtasks(t.id);
            for (const SubTask &st : subtasks) {
                if (!m_showCompleted && st.workStatus == TaskWorkStatus::Completed) continue;
                DisplayRow subRow;
                subRow.type = DisplayRow::SubTaskRow;
                subRow.subtask = st;
                m_displayRows.append(subRow);
            }
        }
    }
}

int TaskListModel::taskIdAt(int row) const
{
    if (row < 0 || row >= m_displayRows.size()) return -1;
    const DisplayRow &r = m_displayRows[row];
    if (r.type == DisplayRow::MainTask) return r.task.id;
    return -1;
}

int TaskListModel::rowForTaskId(int taskId) const
{
    for (int i = 0; i < m_displayRows.size(); ++i) {
        if (m_displayRows[i].type == DisplayRow::MainTask &&
            m_displayRows[i].task.id == taskId)
            return i;
    }
    return -1;
}

int TaskListModel::rowForSubTaskId(int subtaskId) const
{
    for (int i = 0; i < m_displayRows.size(); ++i) {
        if (m_displayRows[i].type == DisplayRow::SubTaskRow &&
            m_displayRows[i].subtask.id == subtaskId)
            return i;
    }
    return -1;
}

Task TaskListModel::taskAt(int row) const
{
    if (row < 0 || row >= m_displayRows.size()) return Task();
    if (m_displayRows[row].type == DisplayRow::MainTask)
        return m_displayRows[row].task;
    return Task();
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
    if (row < 0 || row >= m_displayRows.size()) return;
    if (m_displayRows[row].type != DisplayRow::MainTask) return;
    int id = m_displayRows[row].task.id;
    if (DatabaseManager::instance().deleteTask(id)) {
        refresh();
    }
}

void TaskListModel::refresh()
{
    if (m_searchResults) {
        QList<Task> refreshed;
        for (const auto &task : m_tasks) {
            const Task current = DatabaseManager::instance().getTask(task.id);
            if (current.id > 0) refreshed.append(current);
        }
        loadSearchResults(refreshed);
        return;
    }
    if (m_currentStatus == TaskStatus::Deleted) {
        loadDeletedTasks();
    } else if (m_currentProductId > 0) {
        loadTasks(m_currentProductId, m_currentStatus);
    }
}

void TaskListModel::setShowCompleted(bool show)
{
    if (m_showCompleted == show) return;
    beginResetModel();
    m_showCompleted = show;
    rebuildDisplayRows();
    endResetModel();
}

int TaskListModel::visibleTaskCount() const
{
    int count = 0;
    for (const auto &task : m_tasks) {
        if (m_showCompleted || task.workStatus != TaskWorkStatus::Completed) ++count;
    }
    return count;
}

void TaskListModel::toggleExpand(int taskId)
{
    if (m_expandedTasks.contains(taskId)) {
        m_expandedTasks.remove(taskId);
    } else {
        m_expandedTasks.insert(taskId);
    }
    beginResetModel();
    rebuildDisplayRows();
    endResetModel();
}

bool TaskListModel::isExpanded(int taskId) const
{
    return m_expandedTasks.contains(taskId);
}

Qt::ItemFlags TaskListModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QAbstractListModel::flags(index);
    if (!index.isValid()) return defaultFlags | Qt::ItemIsDropEnabled;
    if (index.row() < m_displayRows.size() &&
        m_displayRows[index.row()].type == DisplayRow::MainTask) {
        return defaultFlags | Qt::ItemIsDragEnabled;
    }
    if (index.row() < m_displayRows.size()) {
        const int parentId = m_displayRows[index.row()].subtask.taskId;
        for (const Task &task : m_tasks) {
            if (task.id == parentId && task.status != TaskStatus::Deleted)
                return defaultFlags | Qt::ItemIsDragEnabled;
        }
    }
    return defaultFlags;
}

Qt::DropActions TaskListModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

bool TaskListModel::canDropAt(int fromRow, int toRow) const
{
    if (fromRow < 0 || fromRow >= m_displayRows.size()) return false;
    if (m_displayRows[fromRow].type != DisplayRow::MainTask) return false;
    if (toRow < 0 || toRow > m_displayRows.size()) return false;

    int fromPriority = static_cast<int>(m_displayRows[fromRow].task.priority);

    int targetPriority;
    if (toRow == m_displayRows.size()) {
        // Find last main task
        for (int i = m_displayRows.size() - 1; i >= 0; --i) {
            if (m_displayRows[i].type == DisplayRow::MainTask) {
                targetPriority = static_cast<int>(m_displayRows[i].task.priority);
                break;
            }
        }
    } else if (toRow == fromRow || toRow == fromRow + 1) {
        return true;
    } else {
        if (m_displayRows[toRow].type == DisplayRow::MainTask) {
            targetPriority = static_cast<int>(m_displayRows[toRow].task.priority);
        } else {
            return false;
        }
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

    // For simplicity, just refresh after reorder at task level
    // Find the task and its target position in m_tasks
    int fromTaskId = m_displayRows[sourceRow].task.id;
    int fromPriority = static_cast<int>(m_displayRows[sourceRow].task.priority);
    const Task sourceTask = m_displayRows[sourceRow].task;
    auto inSourceGroup = [&sourceTask](const Task &task) {
        return task.priority == sourceTask.priority && task.productId == sourceTask.productId &&
               task.status == sourceTask.status;
    };

    // Collect task ids in this priority group in display order
    QList<int> groupIds;
    for (const auto &dr : m_displayRows) {
        if (dr.type == DisplayRow::MainTask &&
            inSourceGroup(dr.task)) {
            groupIds.append(dr.task.id);
        }
    }

    int fromIdx = groupIds.indexOf(fromTaskId);
    if (fromIdx < 0) return false;

    // Determine target index in group
    int targetIdx = 0;
    int mainCount = 0;
    for (int i = 0; i < m_displayRows.size() && i < destinationRow; ++i) {
        if (m_displayRows[i].type == DisplayRow::MainTask &&
            inSourceGroup(m_displayRows[i].task)) {
            mainCount++;
        }
    }
    targetIdx = mainCount;
    if (destinationRow > sourceRow) targetIdx--;

    groupIds.move(fromIdx, targetIdx);
    // Reorder visible slots in the complete group so hidden tasks keep their slots.
    QList<int> completeGroup;
    int visiblePosition = 0;
    for (const auto &task : DatabaseManager::instance().getTasksForProduct(
             sourceTask.productId, sourceTask.status)) {
        if (static_cast<int>(task.priority) != fromPriority) continue;
        if (groupIds.contains(task.id)) completeGroup.append(groupIds[visiblePosition++]);
        else completeGroup.append(task.id);
    }
    if (visiblePosition != groupIds.size()) return false;
    if (!DatabaseManager::instance().reorderTasks(completeGroup)) {
        return false;
    }
    if (m_searchResults) {
        // Preserve search-result slots for other groups while showing the move immediately.
        QHash<int, Task> tasksById;
        for (const auto &task : m_tasks) tasksById.insert(task.id, task);
        int position = 0;
        for (auto &task : m_tasks) {
            if (groupIds.contains(task.id)) task = tasksById.value(groupIds[position++]);
        }
    }
    refresh();
    return true;
}
