#pragma once

#include <QWidget>
#include <QListView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QString>
#include "models/TaskListModel.h"

class TaskPane : public QWidget
{
    Q_OBJECT

public:
    enum class ViewMode {
        Active = 0,
        Archived = 1,
        Deleted = 2,
        SearchResults = 3
    };

    explicit TaskPane(QWidget *parent = nullptr);

    void loadTasks(int productId);
    void showArchived(bool archived);
    void loadSearchResults(const QList<Task> &tasks, const QString &query = QString());
    void restoreView(int productId, ViewMode mode, int selectedTaskId);
    ViewMode viewMode() const;
    int selectedTaskId() const;

signals:
    void taskSelected(int taskId);
    void subTaskSelected(int subTaskId);

private slots:
    void onTaskClicked(const QModelIndex &index);
    void onTaskDoubleClicked(const QModelIndex &index);
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
    void refreshCurrentList();
    void showPriorityPopup(const QModelIndex &index, const QPoint &globalPos);
    void showWorkStatusPopup(const QModelIndex &index, const QPoint &globalPos);
    void showSubTaskStatusPopup(int subtaskId, int currentStatus, const QPoint &globalPos);
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
    bool m_showingSearchResults = false;
    QString m_searchQuery;
};
