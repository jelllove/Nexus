#include "TaskPane.h"
#include "ui/TaskCardDelegate.h"
#include "db/DatabaseManager.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QMenu>
#include <QAction>
#include <QHBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDateTimeEdit>
#include <QFormLayout>
#include <QEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QCheckBox>

TaskPane::TaskPane(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setupContextMenu();
}

void TaskPane::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Title
    m_titleLabel = new QLabel("Tasks", this);
    m_titleLabel->setStyleSheet(
        "QLabel { font-size: 14px; font-weight: bold; padding: 8px; "
        "background-color: #2c3e50; color: white; }");
    layout->addWidget(m_titleLabel);

    // Status filter toggle buttons
    auto *filterLayout = new QHBoxLayout();
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->setSpacing(0);

    m_activeButton = new QPushButton("Active", this);
    m_activeButton->setCheckable(true);
    m_activeButton->setChecked(true);
    m_activeButton->setCursor(Qt::PointingHandCursor);
    filterLayout->addWidget(m_activeButton, 3);  // Stretch factor 3

    m_archivedButton = new QPushButton("Archived", this);
    m_archivedButton->setCheckable(true);
    m_archivedButton->setChecked(false);
    m_archivedButton->setCursor(Qt::PointingHandCursor);
    filterLayout->addWidget(m_archivedButton, 1);  // Stretch factor 1

    m_deletedButton = new QPushButton("Deleted", this);
    m_deletedButton->setCheckable(true);
    m_deletedButton->setChecked(false);
    m_deletedButton->setCursor(Qt::PointingHandCursor);
    filterLayout->addWidget(m_deletedButton, 1);  // Stretch factor 1

    layout->addLayout(filterLayout);
    updateFilterButtonStyles();

    // List view with card delegate
    m_model = new TaskListModel(this);
    m_listView = new QListView(this);
    m_listView->setModel(m_model);
    m_listView->setItemDelegate(new TaskCardDelegate(this));
    m_listView->setSpacing(2);
    m_listView->setStyleSheet(
        "QListView { border: none; background-color: #ecf0f1; }"
        "QListView::item { background-color: white; border-radius: 4px; }"
        "QListView::item:selected { background-color: #d5e8f0; border: 1px solid #2980b9; }"
        "QListView::item:hover { background-color: #eaf2f8; }");
    m_listView->setDragEnabled(true);
    m_listView->setAcceptDrops(true);
    m_listView->setDropIndicatorShown(true);
    m_listView->setDragDropMode(QAbstractItemView::InternalMove);
    m_listView->setDefaultDropAction(Qt::MoveAction);
    m_listView->viewport()->installEventFilter(this);
    layout->addWidget(m_listView, 1);

    // Add button
    m_addButton = new QPushButton("+ Add Task", this);
    m_addButton->setStyleSheet(
        "QPushButton { background-color: #2980b9; color: white; border: none; "
        "padding: 8px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #3498db; }");
    layout->addWidget(m_addButton);

    connect(m_listView, &QListView::doubleClicked, this, &TaskPane::onTaskDoubleClicked);
    connect(m_addButton, &QPushButton::clicked, this, &TaskPane::onAddTask);

    connect(m_activeButton, &QPushButton::clicked, this, [this]() {
        m_showingArchived = false;
        m_showingDeleted = false;
        updateFilterButtonStyles();
        m_addButton->setEnabled(true);
        if (m_currentProductId > 0) {
            m_showingSearchResults = false;
            m_currentSearchQuery.clear();
            m_model->loadTasks(m_currentProductId, TaskStatus::Active);
            m_titleLabel->setText("Tasks");
        }
    });
    connect(m_archivedButton, &QPushButton::clicked, this, [this]() {
        m_showingArchived = true;
        m_showingDeleted = false;
        updateFilterButtonStyles();
        m_addButton->setEnabled(true);
        if (m_currentProductId > 0) {
            m_showingSearchResults = false;
            m_currentSearchQuery.clear();
            m_model->loadTasks(m_currentProductId, TaskStatus::Archived);
            m_titleLabel->setText("Tasks");
        }
    });
    connect(m_deletedButton, &QPushButton::clicked, this, [this]() {
        m_showingArchived = false;
        m_showingDeleted = true;
        updateFilterButtonStyles();
        m_addButton->setEnabled(false);
        m_showingSearchResults = false;
        m_currentSearchQuery.clear();
        m_model->loadDeletedTasks();
        m_titleLabel->setText("Tasks");
    });

    // Periodic refresh timer for live progress bar updates
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(60000);  // 60 seconds
    connect(m_refreshTimer, &QTimer::timeout, this, [this]() {
        m_listView->viewport()->update();
    });
    m_refreshTimer->start();
}

void TaskPane::updateFilterButtonStyles()
{
    QString selectedStyle =
        "QPushButton { background-color: #2980b9; color: white; border: none; "
        "padding: 8px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #3498db; }";
    QString unselectedStyle =
        "QPushButton { background-color: #3d566e; color: #95a5a6; border: none; "
        "padding: 8px; font-size: 12px; }"
        "QPushButton:hover { background-color: #4a6580; }";

    bool isActive = !m_showingArchived && !m_showingDeleted;
    m_activeButton->setStyleSheet(isActive ? selectedStyle : unselectedStyle);
    m_archivedButton->setStyleSheet(m_showingArchived ? selectedStyle : unselectedStyle);
    m_deletedButton->setStyleSheet(m_showingDeleted ? selectedStyle : unselectedStyle);
    m_activeButton->setChecked(isActive);
    m_archivedButton->setChecked(m_showingArchived);
    m_deletedButton->setChecked(m_showingDeleted);
}

void TaskPane::setupContextMenu()
{
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listView, &QListView::customContextMenuRequested, this, [this](const QPoint &pos) {
        QModelIndex index = m_listView->indexAt(pos);
        if (!index.isValid()) return;

        m_listView->setCurrentIndex(index);

        QMenu menu(this);
        menu.setStyleSheet(
            "QMenu { background-color: #2c3e50; color: white; border: 1px solid #3d566e; }"
            "QMenu::item:selected { background-color: #2980b9; }");

        // SubTask context menu
        bool isSubTask = index.data(TaskListModel::IsSubTaskRole).toBool();
        if (isSubTask) {
            int subtaskId = index.data(TaskListModel::SubTaskIdRole).toInt();
            bool completed = index.data(TaskListModel::SubTaskCompletedRole).toBool();
            const EditorTarget activeTarget = m_model->activeTarget();

            QAction *toggleAction = menu.addAction(completed ? "Mark Incomplete" : "Mark Complete");
            QAction *renameAction = menu.addAction("Rename");
            menu.addSeparator();
            QAction *deleteAction = menu.addAction("Delete");

            QAction *selected = menu.exec(m_listView->viewport()->mapToGlobal(pos));
            if (selected == toggleAction) {
                DatabaseManager::instance().toggleSubtask(subtaskId, !completed);
                refreshCurrentView();
            } else if (selected == renameAction) {
                QString currentTitle = index.data(TaskListModel::TitleRole).toString();
                bool ok;
                QString newTitle = QInputDialog::getText(this, "Rename Sub Task",
                    "Title:", QLineEdit::Normal, currentTitle, &ok);
                if (ok && !newTitle.trimmed().isEmpty()) {
                    DatabaseManager::instance().renameSubtask(subtaskId, newTitle.trimmed());
                    refreshCurrentView();
                    if (activeTarget == EditorTarget::subtask(subtaskId)) {
                        const int row = m_model->rowForTarget(activeTarget);
                        if (row >= 0) {
                            m_listView->setCurrentIndex(m_model->index(row, 0));
                        }
                        emit itemSelected(activeTarget);
                    }
                }
            } else if (selected == deleteAction) {
                DatabaseManager::instance().deleteSubtask(subtaskId);
                refreshCurrentView();
                if (activeTarget == EditorTarget::subtask(subtaskId)) {
                    setActiveTarget(EditorTarget());
                    emit itemSelected(EditorTarget());
                }
            }
            return;
        }

        int taskId = m_model->taskIdAt(index.row());
        const QString rowStatus = index.data(TaskListModel::StatusRole).toString();
        const bool isDeletedTask = m_showingSearchResults
            ? (rowStatus == "deleted")
            : m_showingDeleted;
        const bool isArchivedTask = m_showingSearchResults
            ? (rowStatus == "archived")
            : (m_model->currentStatus() == TaskStatus::Archived);

        // Deleted tasks have a simplified context menu
        if (isDeletedTask) {
            QAction *restoreAction = menu.addAction("Restore");
            menu.addSeparator();
            QAction *permDeleteAction = menu.addAction("Permanently Delete");

            QAction *selected = menu.exec(m_listView->viewport()->mapToGlobal(pos));
            if (selected == restoreAction) {
                DatabaseManager::instance().restoreTask(taskId);
                refreshCurrentView();
                setActiveTarget(EditorTarget());
                emit itemSelected(EditorTarget());
            } else if (selected == permDeleteAction) {
                auto result = QMessageBox::warning(this, "Permanently Delete",
                    "This task will be permanently deleted and cannot be recovered.\nAre you sure?",
                    QMessageBox::Yes | QMessageBox::No);
                if (result == QMessageBox::Yes) {
                    DatabaseManager::instance().permanentlyDeleteTask(taskId);
                    refreshCurrentView();
                    setActiveTarget(EditorTarget());
                    emit itemSelected(EditorTarget());
                }
            }
            return;
        }

        // Priority submenu
        QMenu *priorityMenu = menu.addMenu("Set Priority");
        priorityMenu->setStyleSheet(menu.styleSheet());
        priorityMenu->addAction("P0 - Critical");
        priorityMenu->addAction("P1 - High");
        priorityMenu->addAction("P2 - Medium");
        priorityMenu->addAction("P3 - Low");

        // Due date actions
        QDateTime currentDue = index.data(TaskListModel::DueDateRole).toDateTime();
        QAction *dueDateAction = nullptr;
        QAction *clearDueDateAction = nullptr;
        if (currentDue.isValid()) {
            dueDateAction = menu.addAction("Change Due Date...");
            clearDueDateAction = menu.addAction("Clear Due Date");
        } else {
            dueDateAction = menu.addAction("Set Due Date...");
        }

        // Work status submenu
        int currentWorkStatus = index.data(TaskListModel::WorkStatusRole).toInt();
        QMenu *statusMenu = menu.addMenu("Set Status");
        statusMenu->setStyleSheet(menu.styleSheet());
        QAction *statusActions[5];
        const char *statusLabels[] = {
            "\xE2\x8F\xAF\xEF\xB8\x8F Not Started",
            "\xF0\x9F\x8F\x83 Ongoing",
            "\xE2\x8F\xB8\xEF\xB8\x8F Paused",
            "\xE2\x9C\x85 Completed",
            "\xE2\x8F\xB3 Waiting"
        };
        for (int i = 0; i < 5; ++i) {
            statusActions[i] = statusMenu->addAction(QString::fromUtf8(statusLabels[i]));
            if (i == currentWorkStatus) {
                statusActions[i]->setEnabled(false);
            }
        }

        // Sub tasks
        menu.addSeparator();
        QAction *addSubTaskAction = menu.addAction("Add Sub Task...");

        // Archive/Reactivate
        QAction *archiveAction = nullptr;
        if (isArchivedTask) {
            archiveAction = menu.addAction("Reactivate");
        } else {
            archiveAction = menu.addAction("Archive");
        }

        menu.addSeparator();
        QAction *deleteAction = menu.addAction("Delete");

        QAction *selected = menu.exec(m_listView->viewport()->mapToGlobal(pos));
        if (!selected) return;

        // Check priority actions
        for (int i = 0; i < priorityMenu->actions().size(); ++i) {
            if (selected == priorityMenu->actions()[i]) {
                DatabaseManager::instance().updateTaskPriority(taskId, static_cast<TaskPriority>(i));
                refreshCurrentView();
                return;
            }
        }

        if (selected == dueDateAction) {
            QDateTime newDate = showDueDateDialog(currentDue);
            if (newDate.isValid()) {
                DatabaseManager::instance().updateTaskDueDate(taskId, newDate);
                refreshCurrentView();
            }
        } else if (selected == clearDueDateAction) {
            DatabaseManager::instance().updateTaskDueDate(taskId, QDateTime());
            refreshCurrentView();
        }

        // Check status actions
        for (int i = 0; i < 5; ++i) {
            if (selected == statusActions[i]) {
                DatabaseManager::instance().updateTaskWorkStatus(taskId, static_cast<TaskWorkStatus>(i));
                refreshCurrentView();
                return;
            }
        }

        if (selected == archiveAction) {
            if (isArchivedTask) {
                onReactivateTask();
            } else {
                onArchiveTask();
            }
        } else if (selected == deleteAction) {
            onDeleteTask();
        } else if (selected == addSubTaskAction) {
            bool ok;
            QString title = QInputDialog::getText(this, "Add Sub Task",
                "Sub task title:", QLineEdit::Normal, QString(), &ok);
            if (ok && !title.trimmed().isEmpty()) {
                DatabaseManager::instance().addSubtask(taskId, title.trimmed());
                if (!m_showingSearchResults && !m_model->isExpanded(taskId)) {
                    m_model->toggleExpand(taskId);
                } else {
                    refreshCurrentView();
                }
            }
        }
    });
}

void TaskPane::loadTasks(int productId)
{
    m_currentProductId = productId;
    TaskStatus status = m_showingArchived ? TaskStatus::Archived : TaskStatus::Active;
    m_showingSearchResults = false;
    m_currentSearchQuery.clear();
    m_addButton->setEnabled(!m_showingDeleted);
    m_model->loadTasks(productId, status);
    m_titleLabel->setText("Tasks");
    syncSelectionToActiveTarget();
}

void TaskPane::showSearchResults(const QString &query, const QList<SearchResult> &results)
{
    m_showingSearchResults = true;
    m_currentSearchQuery = query;
    m_addButton->setEnabled(false);
    m_model->loadSearchResults(results);
    m_titleLabel->setText("Search Results");
    syncSelectionToActiveTarget();
}

void TaskPane::clearSearchResults()
{
    m_showingSearchResults = false;
    m_currentSearchQuery.clear();
    m_addButton->setEnabled(!m_showingDeleted);

    if (m_showingDeleted) {
        m_model->loadDeletedTasks();
    } else {
        const TaskStatus status = m_showingArchived ? TaskStatus::Archived : TaskStatus::Active;
        m_model->loadTasks(m_currentProductId, status);
    }

    m_titleLabel->setText("Tasks");
    syncSelectionToActiveTarget();
}

void TaskPane::refreshCurrentView()
{
    if (m_showingSearchResults && !m_currentSearchQuery.isEmpty()) {
        m_addButton->setEnabled(false);
        m_model->loadSearchResults(DatabaseManager::instance().searchItems(m_currentSearchQuery));
        m_titleLabel->setText("Search Results");
        syncSelectionToActiveTarget();
        return;
    }

    m_showingSearchResults = false;
    m_currentSearchQuery.clear();
    m_addButton->setEnabled(!m_showingDeleted);

    if (m_showingDeleted) {
        m_model->loadDeletedTasks();
    } else if (m_currentProductId > 0) {
        const TaskStatus status = m_showingArchived ? TaskStatus::Archived : TaskStatus::Active;
        m_model->loadTasks(m_currentProductId, status);
    }

    m_titleLabel->setText("Tasks");
    syncSelectionToActiveTarget();
}

void TaskPane::setActiveTarget(const EditorTarget &target)
{
    m_model->setActiveTarget(target);
    syncSelectionToActiveTarget();
}

EditorTarget TaskPane::activeTarget() const
{
    return m_model->activeTarget();
}

bool TaskPane::containsTarget(const EditorTarget &target) const
{
    return target.isValid() && m_model->rowForTarget(target) >= 0;
}

void TaskPane::showArchived(bool archived)
{
    m_showingArchived = archived;
    updateFilterButtonStyles();
    if (m_currentProductId > 0) {
        loadTasks(m_currentProductId);
    }
}

int TaskPane::selectedTaskId() const
{
    QModelIndex index = m_listView->currentIndex();
    if (!index.isValid()) return -1;
    return m_model->taskIdAt(index.row());
}

void TaskPane::syncSelectionToActiveTarget()
{
    const EditorTarget target = m_model->activeTarget();
    if (!target.isValid()) {
        m_listView->clearSelection();
        m_listView->setCurrentIndex(QModelIndex());
        m_listView->viewport()->update();
        return;
    }

    const int row = m_model->rowForTarget(target);
    if (row < 0) {
        m_model->setActiveTarget(EditorTarget());
        m_listView->clearSelection();
        m_listView->setCurrentIndex(QModelIndex());
        m_listView->viewport()->update();
        return;
    }

    m_listView->setCurrentIndex(m_model->index(row, 0));
    m_listView->viewport()->update();
}

bool TaskPane::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_listView->viewport() && event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            const QModelIndex index = m_listView->indexAt(mouseEvent->pos());
            if (index.isValid()) {
                handleItemClick(index, mouseEvent->pos());
                return true;
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

void TaskPane::handleItemClick(const QModelIndex &index, const QPoint &viewportPosition)
{
    QStyleOptionViewItem option;
    option.rect = m_listView->visualRect(index);
    option.font = m_listView->font();

    bool isSubTask = index.data(TaskListModel::IsSubTaskRole).toBool();

    if (isSubTask) {
        if (m_showingSearchResults) {
            const EditorTarget target = EditorTarget::subtask(
                index.data(TaskListModel::SubTaskIdRole).toInt());
            emit itemSelected(target);
            return;
        }

        const QRect checkRect = TaskCardDelegate::subtaskCheckboxRect(option);
        if (checkRect.contains(viewportPosition)) {
            int subtaskId = index.data(TaskListModel::SubTaskIdRole).toInt();
            bool completed = index.data(TaskListModel::SubTaskCompletedRole).toBool();
            DatabaseManager::instance().toggleSubtask(subtaskId, !completed);
            refreshCurrentView();
            return;
        }

        const EditorTarget target = EditorTarget::subtask(
            index.data(TaskListModel::SubTaskIdRole).toInt());
        emit itemSelected(target);
        return;
    }

    const int taskId = m_model->taskIdAt(index.row());

    if (m_showingSearchResults) {
        const EditorTarget target = EditorTarget::task(taskId);
        emit itemSelected(target);
        return;
    }

    const bool hasSubTasks = index.data(TaskListModel::HasSubTasksRole).toBool();
    if (hasSubTasks) {
        const QRect expandRect = TaskCardDelegate::expandIconRect(option, index);
        if (expandRect.contains(viewportPosition)) {
            m_model->toggleExpand(taskId);
            return;
        }
    }

    const QRect badgeRect = TaskCardDelegate::priorityBadgeRect(option, index);
    if (badgeRect.contains(viewportPosition)) {
        showPriorityPopup(index, QCursor::pos());
        return;
    }

    const QRect statusRect = TaskCardDelegate::workStatusIconRect(option, index);
    if (statusRect.contains(viewportPosition)) {
        showWorkStatusPopup(index, QCursor::pos());
        return;
    }
    const EditorTarget target = EditorTarget::task(taskId);
    emit itemSelected(target);
}

void TaskPane::onTaskDoubleClicked(const QModelIndex &index)
{
    if (m_showingSearchResults) return;

    bool isSubTask = index.data(TaskListModel::IsSubTaskRole).toBool();
    if (isSubTask) return;

    int taskId = m_model->taskIdAt(index.row());
    if (taskId > 0) {
        m_model->toggleExpand(taskId);
    }
}

void TaskPane::showPriorityPopup(const QModelIndex &index, const QPoint &globalPos)
{
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #2c3e50; color: white; border: 1px solid #3d566e; }"
        "QMenu::item:selected { background-color: #2980b9; }");

    menu.addAction("P0 - Critical");
    menu.addAction("P1 - High");
    menu.addAction("P2 - Medium");
    menu.addAction("P3 - Low");

    QAction *selected = menu.exec(globalPos);
    if (!selected) return;

    int taskId = m_model->taskIdAt(index.row());
    for (int i = 0; i < menu.actions().size(); ++i) {
        if (selected == menu.actions()[i]) {
            DatabaseManager::instance().updateTaskPriority(taskId, static_cast<TaskPriority>(i));
            refreshCurrentView();
            return;
        }
    }
}

void TaskPane::showWorkStatusPopup(const QModelIndex &index, const QPoint &globalPos)
{
    int currentStatus = index.data(TaskListModel::WorkStatusRole).toInt();

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #2c3e50; color: white; border: 1px solid #3d566e; }"
        "QMenu::item:selected { background-color: #2980b9; }"
        "QMenu::item:disabled { color: #7f8c8d; }");

    const char *labels[] = {
        "\xE2\x8F\xAF\xEF\xB8\x8F Not Started",
        "\xF0\x9F\x8F\x83 Ongoing",
        "\xE2\x8F\xB8\xEF\xB8\x8F Paused",
        "\xE2\x9C\x85 Completed",
        "\xE2\x8F\xB3 Waiting"
    };

    for (int i = 0; i < 5; ++i) {
        QAction *action = menu.addAction(QString::fromUtf8(labels[i]));
        if (i == currentStatus) {
            action->setEnabled(false);
        }
    }

    QAction *selected = menu.exec(globalPos);
    if (!selected) return;

    int taskId = m_model->taskIdAt(index.row());
    for (int i = 0; i < menu.actions().size(); ++i) {
        if (selected == menu.actions()[i]) {
            DatabaseManager::instance().updateTaskWorkStatus(taskId, static_cast<TaskWorkStatus>(i));
            refreshCurrentView();
            return;
        }
    }
}

QDateTime TaskPane::showDueDateDialog(const QDateTime &current)
{
    QDialog dialog(this);
    dialog.setWindowTitle("Set Due Date");
    dialog.setStyleSheet(
        "QDialog { background-color: #2c3e50; }"
        "QLabel { color: white; }"
        "QDateTimeEdit { background-color: #3d566e; color: white; border: 1px solid #5a7a96; "
        "padding: 6px; font-size: 12px; }"
        "QPushButton { background-color: #2980b9; color: white; border: none; padding: 6px 16px; }"
        "QPushButton:hover { background-color: #3498db; }");

    auto *formLayout = new QFormLayout(&dialog);
    auto *dateEdit = new QDateTimeEdit(&dialog);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("yyyy-MM-dd hh:mm");
    if (current.isValid()) {
        dateEdit->setDateTime(current);
    } else {
        dateEdit->setDateTime(QDateTime::currentDateTime().addDays(7));
    }
    dateEdit->setMinimumDateTime(QDateTime::currentDateTime());
    formLayout->addRow("Due Date:", dateEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    formLayout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        return dateEdit->dateTime();
    }
    return QDateTime();
}

void TaskPane::onAddTask()
{
    if (m_currentProductId <= 0) {
        QMessageBox::information(this, "No Product", "Please select a product first.");
        return;
    }

    // Priority picker dialog
    QDialog dialog(this);
    dialog.setWindowTitle("New Task — Select Priority");
    dialog.setFixedSize(360, 120);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);

    auto *btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(10);

    TaskPriority selectedPriority = TaskPriority::Medium;
    bool accepted = false;

    struct PrioInfo { TaskPriority p; QString label; };
    PrioInfo prios[] = {
        { TaskPriority::Critical, "P0 Critical" },
        { TaskPriority::High,     "P1 High" },
        { TaskPriority::Medium,   "P2 Medium" },
        { TaskPriority::Low,      "P3 Low" }
    };

    for (auto &info : prios) {
        auto *btn = new QPushButton(info.label, &dialog);
        QColor bg = Task::priorityColor(info.p);
        QColor bgLight = Task::priorityBackgroundColor(info.p);
        btn->setFixedHeight(50);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton {"
            "  background: %1; color: white; border: none; border-radius: 8px;"
            "  font-size: 13px; font-weight: bold; padding: 8px 4px;"
            "}"
            "QPushButton:hover {"
            "  background: %2;"
            "}"
        ).arg(bg.name(), bg.darker(115).name()));

        TaskPriority cap = info.p;
        connect(btn, &QPushButton::clicked, &dialog, [&selectedPriority, &accepted, &dialog, cap]() {
            selectedPriority = cap;
            accepted = true;
            dialog.accept();
        });
        btnLayout->addWidget(btn);
    }

    layout->addLayout(btnLayout);
    dialog.exec();

    if (!accepted) return;

    // Task details dialog (title + optional due date)
    QDialog detailsDlg(this);
    detailsDlg.setWindowTitle("New Task — Details");
    detailsDlg.setFixedWidth(400);
    detailsDlg.setStyleSheet(
        "QDialog { background-color: #2c3e50; }"
        "QLabel { color: white; }"
        "QLineEdit { background-color: #3d566e; color: white; border: 1px solid #5a7a96; "
        "padding: 6px; font-size: 12px; }"
        "QDateTimeEdit { background-color: #3d566e; color: white; border: 1px solid #5a7a96; "
        "padding: 6px; font-size: 12px; }"
        "QCheckBox { color: white; }"
        "QPushButton { background-color: #2980b9; color: white; border: none; padding: 6px 16px; }"
        "QPushButton:hover { background-color: #3498db; }");

    auto *formLayout = new QFormLayout(&detailsDlg);
    formLayout->setContentsMargins(16, 16, 16, 16);

    auto *titleEdit = new QLineEdit(&detailsDlg);
    titleEdit->setPlaceholderText("Enter task title...");
    formLayout->addRow("Title:", titleEdit);

    auto *dueDateCheck = new QCheckBox("Set due date", &detailsDlg);
    formLayout->addRow(dueDateCheck);

    auto *dueDateEdit = new QDateTimeEdit(&detailsDlg);
    dueDateEdit->setCalendarPopup(true);
    dueDateEdit->setDisplayFormat("yyyy-MM-dd hh:mm");
    dueDateEdit->setDateTime(QDateTime::currentDateTime().addDays(7));
    dueDateEdit->setMinimumDateTime(QDateTime::currentDateTime());
    dueDateEdit->setEnabled(false);
    formLayout->addRow("Due Date:", dueDateEdit);

    connect(dueDateCheck, &QCheckBox::toggled, dueDateEdit, &QDateTimeEdit::setEnabled);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &detailsDlg);
    formLayout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &detailsDlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &detailsDlg, &QDialog::reject);

    if (detailsDlg.exec() != QDialog::Accepted) return;

    QString title = titleEdit->text().trimmed();
    QDateTime dueDate;
    if (dueDateCheck->isChecked()) {
        dueDate = dueDateEdit->dateTime();
    }

    int newId = m_model->addTask(m_currentProductId, title, selectedPriority, dueDate);
    // Select the newly added task
    int row = m_model->rowForTaskId(newId);
    if (row >= 0) {
        emit itemSelected(EditorTarget::task(newId));
    }
}

void TaskPane::onDeleteTask()
{
    QModelIndex index = m_listView->currentIndex();
    if (!index.isValid()) return;

    QString title = index.data(Qt::DisplayRole).toString();
    auto result = QMessageBox::question(this, "Delete Task",
        QString("Delete task '%1'?").arg(title),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
        const int taskId = m_model->taskIdAt(index.row());
        DatabaseManager::instance().deleteTask(taskId);
        refreshCurrentView();
        setActiveTarget(EditorTarget());
        emit itemSelected(EditorTarget());
    }
}

void TaskPane::onArchiveTask()
{
    QModelIndex index = m_listView->currentIndex();
    if (!index.isValid()) return;

    int taskId = m_model->taskIdAt(index.row());
    DatabaseManager::instance().archiveTask(taskId);
    refreshCurrentView();
    setActiveTarget(EditorTarget());
    emit itemSelected(EditorTarget());
}

void TaskPane::onReactivateTask()
{
    QModelIndex index = m_listView->currentIndex();
    if (!index.isValid()) return;

    int taskId = m_model->taskIdAt(index.row());
    DatabaseManager::instance().reactivateTask(taskId);
    refreshCurrentView();
    setActiveTarget(EditorTarget());
    emit itemSelected(EditorTarget());
}

void TaskPane::onChangePriority()
{
    // Handled via context menu and badge click
}

void TaskPane::onStatusToggle(bool showArchived)
{
    m_showingArchived = showArchived;
    updateFilterButtonStyles();
    if (m_currentProductId > 0) {
        TaskStatus status = m_showingArchived ? TaskStatus::Archived : TaskStatus::Active;
        m_model->loadTasks(m_currentProductId, status);
    }
}
