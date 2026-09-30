#include "TaskListView.h"
#include "models/TaskListModel.h"
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSignalBlocker>

TaskListView::TaskListView(TaskListModel *model, QWidget *parent)
    : QListView(parent), m_taskModel(model)
{
    setModel(model);
    connect(model, &QAbstractItemModel::modelAboutToBeReset,
            this, &TaskListView::cancelSubtaskDrag);
    m_scrollTimer.setInterval(75);
    connect(&m_scrollTimer, &QTimer::timeout, this, [this]() {
        auto *bar = verticalScrollBar();
        const int before = bar->value();
        bar->setValue(before + scrollDirection() * bar->singleStep());
        updateDropTarget();
        if (before == bar->value()) m_scrollTimer.stop();
    });
}

void TaskListView::mousePressEvent(QMouseEvent *event)
{
    clearSubtaskDrag();
    m_suppressClick = false;
    const QModelIndex index = indexAt(event->position().toPoint());
    if (event->button() == Qt::LeftButton &&
        index.data(TaskListModel::IsSubTaskRole).toBool() &&
        index.flags().testFlag(Qt::ItemIsDragEnabled)) {
        m_pressedSubtaskId = index.data(TaskListModel::SubTaskIdRole).toInt();
        m_pressPosition = event->position().toPoint();
    }
    QListView::mousePressEvent(event);
}

void TaskListView::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons().testFlag(Qt::LeftButton) && m_pressedSubtaskId > 0) {
        m_dragPosition = event->position().toPoint();
        if ((m_dragPosition - m_pressPosition).manhattanLength() >= QApplication::startDragDistance())
            m_draggingSubtask = true;
        if (m_draggingSubtask) updateDropTarget();
        event->accept();
        return;
    }
    if (m_suppressClick && event->buttons().testFlag(Qt::LeftButton)) {
        event->accept();
        return;
    }
    if (m_draggingSubtask) cancelSubtaskDrag();
    QListView::mouseMoveEvent(event);
}

void TaskListView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QListView::mouseReleaseEvent(event);
        return;
    }
    const int subtaskId = m_pressedSubtaskId;
    const DropTarget target = m_draggingSubtask
        ? dropTargetAt(event->position().toPoint()) : DropTarget();
    const bool suppressClick = m_draggingSubtask || m_suppressClick;
    clearSubtaskDrag();
    m_suppressClick = false;
    if (!suppressClick) {
        QListView::mouseReleaseEvent(event);
        return;
    }
    // Finish the view's press/release bookkeeping without opening an editor or status popup.
    {
        const QSignalBlocker blocker(this);
        QListView::mouseReleaseEvent(event);
    }
    event->accept();
    if (target.taskId > 0)
        emit subTaskMoveRequested(subtaskId, target.taskId, target.position);
}

void TaskListView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_pressedSubtaskId > 0) {
        cancelSubtaskDrag();
        event->accept();
        return;
    }
    QListView::keyPressEvent(event);
}

void TaskListView::focusOutEvent(QFocusEvent *event)
{
    cancelSubtaskDrag();
    QListView::focusOutEvent(event);
}

void TaskListView::hideEvent(QHideEvent *event)
{
    cancelSubtaskDrag();
    QListView::hideEvent(event);
}

TaskListView::DropTarget TaskListView::dropTargetAt(const QPoint &position) const
{
    const int sourceRow = m_taskModel->rowForSubTaskId(m_pressedSubtaskId);
    if (!viewport()->rect().contains(position) || sourceRow < 0 ||
        !m_taskModel->index(sourceRow).flags().testFlag(Qt::ItemIsDragEnabled))
        return {};

    QModelIndex target = indexAt(position);
    if (!target.isValid()) target = indexAt(position - QPoint(0, spacing() + 1));
    if (!target.isValid()) target = indexAt(position + QPoint(0, spacing() + 1));
    bool appendAtEnd = false;
    if (!target.isValid() && m_taskModel->rowCount() > 0) {
        const QModelIndex last = m_taskModel->index(m_taskModel->rowCount() - 1);
        if (position.y() > visualRect(last).bottom()) {
            target = last;
            appendAtEnd = true;
        }
    }
    if (!target.isValid()) return {};

    DropTarget result;
    const bool isSubtask = target.data(TaskListModel::IsSubTaskRole).toBool();
    result.taskId = target.data(isSubtask ? TaskListModel::ParentTaskIdRole
                                         : TaskListModel::IdRole).toInt();
    const QModelIndex parent = m_taskModel->index(m_taskModel->rowForTaskId(result.taskId));
    if (!parent.isValid() || parent.data(TaskListModel::StatusRole).toString() == "deleted")
        return {};

    const QRect targetRect = visualRect(target);
    if (!isSubtask) {
        result.onMainTask = true;
        result.indicator = targetRect.adjusted(3, 2, -3, -2);
    } else {
        const int targetId = target.data(TaskListModel::SubTaskIdRole).toInt();
        if (targetId == m_pressedSubtaskId && !appendAtEnd) return {};
        const bool after = appendAtEnd || position.y() >= targetRect.center().y();
        if (!appendAtEnd) {
            result.position = 0;
            for (int row = parent.row() + 1; row < target.row(); ++row) {
                if (m_taskModel->index(row).data(TaskListModel::SubTaskIdRole).toInt() != m_pressedSubtaskId)
                    ++result.position;
            }
            if (after) ++result.position;
        }
        result.indicator = QRect(targetRect.left() + 30,
                                 after ? targetRect.bottom() : targetRect.top(),
                                 targetRect.width() - 36, 2);
    }

    const int sourceParent = m_taskModel->index(sourceRow).data(TaskListModel::ParentTaskIdRole).toInt();
    if (sourceParent == result.taskId) {
        const int sourcePosition = sourceRow - parent.row() - 1;
        const bool sourceIsLast = sourceRow + 1 == m_taskModel->rowCount() ||
            !m_taskModel->index(sourceRow + 1).data(TaskListModel::IsSubTaskRole).toBool();
        if (result.position == sourcePosition || (result.position == -1 && sourceIsLast))
            return {};
    }
    return result;
}

int TaskListView::scrollDirection() const
{
    if (!m_draggingSubtask || !viewport()->rect().contains(m_dragPosition)) return 0;
    const int margin = qMin(24, viewport()->height() / 4);
    if (m_dragPosition.y() < margin) return -1;
    if (m_dragPosition.y() >= viewport()->height() - margin) return 1;
    return 0;
}

void TaskListView::updateDropTarget()
{
    if (!m_draggingSubtask) return;
    m_dropTarget = dropTargetAt(m_dragPosition);
    viewport()->setCursor(m_dropTarget.taskId > 0 ? Qt::ClosedHandCursor : Qt::ForbiddenCursor);
    if (scrollDirection() != 0) {
        if (!m_scrollTimer.isActive()) m_scrollTimer.start();
    } else {
        m_scrollTimer.stop();
    }
    viewport()->update();
}

void TaskListView::clearSubtaskDrag()
{
    m_scrollTimer.stop();
    m_pressedSubtaskId = -1;
    m_draggingSubtask = false;
    m_dropTarget = {};
    viewport()->unsetCursor();
    viewport()->update();
}

void TaskListView::cancelSubtaskDrag()
{
    if (m_pressedSubtaskId > 0) m_suppressClick = true;
    clearSubtaskDrag();
}

void TaskListView::scrollContentsBy(int dx, int dy)
{
    QListView::scrollContentsBy(dx, dy);
    updateDropTarget();
}

void TaskListView::paintEvent(QPaintEvent *event)
{
    QListView::paintEvent(event);
    if (!m_draggingSubtask || m_dropTarget.taskId <= 0) return;
    QPainter painter(viewport());
    painter.setPen(QPen(QColor("#2980b9"), 2));
    if (m_dropTarget.onMainTask) {
        painter.setBrush(QColor(41, 128, 185, 35));
        painter.drawRoundedRect(m_dropTarget.indicator, 4, 4);
    } else {
        painter.drawLine(m_dropTarget.indicator.topLeft(), m_dropTarget.indicator.topRight());
    }
}
