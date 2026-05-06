#pragma once

#include <QWidget>
#include <QListView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include "models/TaskListModel.h"

class TaskPane : public QWidget
{
    Q_OBJECT

public:
    explicit TaskPane(QWidget *parent = nullptr);

    void loadTasks(int productId);
    void showArchived(bool archived);
    int selectedTaskId() const;

signals:
    void taskSelected(int taskId);

private slots:
    void onTaskClicked(const QModelIndex &index);
    void onAddTask();
    void onDeleteTask();
    void onArchiveTask();
    void onReactivateTask();
    void onChangePriority();
    void onStatusToggle(bool showArchived);

private:
    void setupUi();
    void setupContextMenu();
    void updateFilterButtonStyles();
    void showPriorityPopup(const QModelIndex &index, const QPoint &globalPos);
    void showWorkStatusPopup(const QModelIndex &index, const QPoint &globalPos);
    void handleDropReorder(int fromRow, int toRow);
    QDateTime showDueDateDialog(const QDateTime &current = QDateTime());

    QListView *m_listView;
    TaskListModel *m_model;
    QPushButton *m_addButton;
    QLabel *m_titleLabel;
    QPushButton *m_activeButton;
    QPushButton *m_archivedButton;
    QPushButton *m_deletedButton;
    QTimer *m_refreshTimer;
    int m_currentProductId = -1;
    bool m_showingArchived = false;
    bool m_showingDeleted = false;
};
