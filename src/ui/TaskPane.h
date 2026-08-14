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
    void showSearchResults(const QString &query, const QList<SearchResult> &results);
    void refreshCurrentView();
    void setActiveTarget(const EditorTarget &target);
    EditorTarget activeTarget() const;
    bool containsTarget(const EditorTarget &target) const;
    int selectedTaskId() const;

signals:
    void itemSelected(const EditorTarget &target);

private slots:
    void onTaskDoubleClicked(const QModelIndex &index);
    void onAddTask();
    void onDeleteTask();
    void onArchiveTask();
    void onReactivateTask();
    void onChangePriority();
    void onStatusToggle(bool showArchived);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUi();
    void setupContextMenu();
    void updateFilterButtonStyles();
    void syncSelectionToActiveTarget();
    void showPriorityPopup(const QModelIndex &index, const QPoint &globalPos);
    void showWorkStatusPopup(const QModelIndex &index, const QPoint &globalPos);
    void handleItemClick(const QModelIndex &index, const QPoint &viewportPosition);
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
    QString m_currentSearchQuery;
};
