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
            case ContentRole:
                return row.subtask.content;
            case CreatedAtRole:
                return row.subtask.createdAt;
            case UpdatedAtRole:
                return row.subtask.updatedAt;
            case IsSubTaskRole:
                return true;
            case SubTaskCompletedRole:
                return row.subtask.completed;
            case SubTaskIdRole:
                return row.subtask.id;
            case ParentTaskIdRole:
                return row.subtask.taskId;
            case IsActiveTaskRole:
                return m_activeTarget == EditorTarget::subtask(row.subtask.id);
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
            if (t.status == TaskStatus::Deleted) {
                return "deleted";
            }
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
        case IsSubTaskRole:
            return false;
        case HasSubTasksRole:
            return !m_searchMode && DatabaseManager::instance().getSubtaskCount(t.id) > 0;
        case IsExpandedRole:
            return !m_searchMode && m_expandedTasks.contains(t.id);
        case IsActiveTaskRole:
            return m_activeTarget == EditorTarget::task(t.id);
    }
    return QVariant();
}

void TaskListModel::loadTasks(int productId, TaskStatus status)
{
    m_currentProductId = productId;
    m_currentStatus = status;
    m_searchMode = false;
    beginResetModel();
    m_tasks = DatabaseManager::instance().getTasksForProduct(productId, status);
    rebuildDisplayRows();
    endResetModel();
}

void TaskListModel::loadDeletedTasks()
{
    m_currentProductId = -1;
    m_currentStatus = TaskStatus::Deleted;
    m_searchMode = false;
    beginResetModel();
    m_tasks = DatabaseManager::instance().getDeletedTasks();
    rebuildDisplayRows();
    endResetModel();
}

void TaskListModel::loadSearchResults(const QList<SearchResult> &results)
{
    m_currentProductId = -1;
    m_searchMode = true;
    beginResetModel();
    m_tasks.clear();
    m_displayRows.clear();

    QSet<int> appendedParentIds;
    QList<int> parentOrder;
    QHash<int, Task> parentTasks;
    QHash<int, QList<SubTask>> matchedSubtasks;

    for (const SearchResult &result : results) {
        const int parentId = result.parentTask.id;
        if (!appendedParentIds.contains(parentId)) {
            appendedParentIds.insert(parentId);
            parentOrder.append(parentId);
            parentTasks.insert(parentId, result.parentTask);
        }

        if (result.isSubtaskMatch) {
            matchedSubtasks[parentId].append(result.matchedSubtask);
        }
    }

    for (int parentId : parentOrder) {
        DisplayRow mainRow;
        mainRow.type = DisplayRow::MainTask;
        mainRow.task = parentTasks.value(parentId);
        m_displayRows.append(mainRow);

        const QList<SubTask> subtasks = matchedSubtasks.value(parentId);
        for (const SubTask &subtask : subtasks) {
            DisplayRow subRow;
            subRow.type = DisplayRow::SubTaskRow;
            subRow.subtask = subtask;
            m_displayRows.append(subRow);
        }
    }

    endResetModel();
}

void TaskListModel::rebuildDisplayRows()
{
    m_displayRows.clear();
    for (const Task &t : m_tasks) {
        DisplayRow mainRow;
        mainRow.type = DisplayRow::MainTask;
        mainRow.task = t;
        m_displayRows.append(mainRow);

        if (m_expandedTasks.contains(t.id)) {
            auto subtasks = DatabaseManager::instance().getSubtasks(t.id);
            for (const SubTask &st : subtasks) {
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

EditorTarget TaskListModel::targetAt(int row) const
{
    if (row < 0 || row >= m_displayRows.size()) {
        return EditorTarget();
    }

    const DisplayRow &displayRow = m_displayRows[row];
    if (displayRow.type == DisplayRow::SubTaskRow) {
        return EditorTarget::subtask(displayRow.subtask.id);
    }

    return EditorTarget::task(displayRow.task.id);
}

int TaskListModel::rowForTarget(const EditorTarget &target) const
{
    for (int i = 0; i < m_displayRows.size(); ++i) {
        if (targetAt(i) == target) {
            return i;
        }
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
    if (m_currentStatus == TaskStatus::Deleted) {
        loadDeletedTasks();
    } else if (m_currentProductId > 0) {
        loadTasks(m_currentProductId, m_currentStatus);
    }
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
    if (m_searchMode) {
        return defaultFlags;
    }
    if (!index.isValid()) return defaultFlags | Qt::ItemIsDropEnabled;
    if (index.row() < m_displayRows.size() &&
        m_displayRows[index.row()].type == DisplayRow::MainTask) {
        return defaultFlags | Qt::ItemIsDragEnabled;
    }
    return defaultFlags;
}

Qt::DropActions TaskListModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

bool TaskListModel::canDropAt(int fromRow, int toRow) const
{
    if (m_searchMode) return false;
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
    if (m_searchMode) return false;
    Q_UNUSED(sourceParent);
    Q_UNUSED(destinationParent);
    if (count != 1) return false;
    if (!canDropAt(sourceRow, destinationRow)) return false;
    if (destinationRow == sourceRow || destinationRow == sourceRow + 1) return true;

    // For simplicity, just refresh after reorder at task level
    // Find the task and its target position in m_tasks
    int fromTaskId = m_displayRows[sourceRow].task.id;
    int fromPriority = static_cast<int>(m_displayRows[sourceRow].task.priority);

    // Collect task ids in this priority group in display order
    QList<int> groupIds;
    for (const auto &dr : m_displayRows) {
        if (dr.type == DisplayRow::MainTask &&
            static_cast<int>(dr.task.priority) == fromPriority) {
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
            static_cast<int>(m_displayRows[i].task.priority) == fromPriority) {
            mainCount++;
        }
    }
    targetIdx = mainCount;
    if (destinationRow > sourceRow) targetIdx--;

    groupIds.move(fromIdx, targetIdx);
    DatabaseManager::instance().reorderTasks(groupIds);
    refresh();
    return true;
}
