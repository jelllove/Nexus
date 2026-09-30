#pragma once

#include <QListView>
#include <QTimer>

class TaskListModel;

class TaskListView : public QListView
{
    Q_OBJECT

public:
    explicit TaskListView(TaskListModel *model, QWidget *parent = nullptr);

signals:
    void subTaskMoveRequested(int subtaskId, int destinationTaskId, int position);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    struct DropTarget {
        int taskId = -1;
        int position = -1;
        QRect indicator;
        bool onMainTask = false;
    };

    DropTarget dropTargetAt(const QPoint &position) const;
    void updateDropTarget();
    void clearSubtaskDrag();
    void cancelSubtaskDrag();
    int scrollDirection() const;

    TaskListModel *m_taskModel;
    QTimer m_scrollTimer;
    int m_pressedSubtaskId = -1;
    QPoint m_pressPosition;
    QPoint m_dragPosition;
    bool m_draggingSubtask = false;
    bool m_suppressClick = false;
    DropTarget m_dropTarget;
};
