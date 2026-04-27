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
#include <QLineEdit>
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
    layout->addWidget(m_listView, 1);

    // Add button
    m_addButton = new QPushButton("+ Add Task", this);
    m_addButton->setStyleSheet(
        "QPushButton { background-color: #2980b9; color: white; border: none; "
        "padding: 8px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #3498db; }");
    layout->addWidget(m_addButton);

    connect(m_listView, &QListView::clicked, this, &TaskPane::onTaskClicked);
    connect(m_addButton, &QPushButton::clicked, this, &TaskPane::onAddTask);

    connect(m_activeButton, &QPushButton::clicked, this, [this]() {
        onStatusToggle(false);
    });
    connect(m_archivedButton, &QPushButton::clicked, this, [this]() {
        onStatusToggle(true);
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

    m_activeButton->setStyleSheet(m_showingArchived ? unselectedStyle : selectedStyle);
    m_archivedButton->setStyleSheet(m_showingArchived ? selectedStyle : unselectedStyle);
    m_activeButton->setChecked(!m_showingArchived);
    m_archivedButton->setChecked(m_showingArchived);
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

        // Priority submenu
        QMenu *priorityMenu = menu.addMenu("Set Priority");
        priorityMenu->setStyleSheet(menu.styleSheet());
        priorityMenu->addAction("P0 - Critical");
        priorityMenu->addAction("P1 - High");
        priorityMenu->addAction("P2 - Medium");
        priorityMenu->addAction("P3 - Low");

        // Due date actions
        int taskId = m_model->taskIdAt(index.row());
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
            "\xE2\x8F\xAF\xEF\xB8\x8F Not Started",           // ⏯️
            "\xF0\x9F\x8F\x83 Ongoing",               // 🏃
            "\xE2\x8F\xB8\xEF\xB8\x8F Paused",       // ⏸️
            "\xE2\x9C\x85 Completed",                  // ✅
            "\xE2\x8F\xB3 Waiting"                     // ⏳
        };
        for (int i = 0; i < 5; ++i) {
            statusActions[i] = statusMenu->addAction(QString::fromUtf8(statusLabels[i]));
            if (i == currentWorkStatus) {
                statusActions[i]->setEnabled(false);
            }
        }

        // Archive/Reactivate
        bool isArchived = (m_model->currentStatus() == TaskStatus::Archived);
        QAction *archiveAction = nullptr;
        if (isArchived) {
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
                m_model->refresh();
                return;
            }
        }

        if (selected == dueDateAction) {
            QDateTime newDate = showDueDateDialog(currentDue);
            if (newDate.isValid()) {
                DatabaseManager::instance().updateTaskDueDate(taskId, newDate);
                m_model->refresh();
            }
        } else if (selected == clearDueDateAction) {
            DatabaseManager::instance().updateTaskDueDate(taskId, QDateTime());
            m_model->refresh();
        }

        // Check status actions
        for (int i = 0; i < 5; ++i) {
            if (selected == statusActions[i]) {
                DatabaseManager::instance().updateTaskWorkStatus(taskId, static_cast<TaskWorkStatus>(i));
                m_model->refresh();
                return;
            }
        }

        if (selected == archiveAction) {
            if (isArchived) {
                onReactivateTask();
            } else {
                onArchiveTask();
            }
        } else if (selected == deleteAction) {
            onDeleteTask();
        }
    });
}

void TaskPane::loadTasks(int productId)
{
    m_currentProductId = productId;
    TaskStatus status = m_showingArchived ? TaskStatus::Archived : TaskStatus::Active;
    m_model->loadTasks(productId, status);
    m_titleLabel->setText("Tasks");
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

void TaskPane::onTaskClicked(const QModelIndex &index)
{
    // Check if the click hit the priority badge
    QPoint clickPos = m_listView->viewport()->mapFromGlobal(QCursor::pos());
    QStyleOptionViewItem option;
    option.rect = m_listView->visualRect(index);
    option.font = m_listView->font();
    QRect badgeRect = TaskCardDelegate::priorityBadgeRect(option, index);

    if (badgeRect.contains(clickPos)) {
        showPriorityPopup(index, QCursor::pos());
        return;
    }

    int taskId = m_model->taskIdAt(index.row());
    emit taskSelected(taskId);
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
            m_model->refresh();
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
        QModelIndex idx = m_model->index(row);
        m_listView->setCurrentIndex(idx);
        emit taskSelected(newId);
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
        m_model->removeTask(index.row());
        emit taskSelected(-1);  // Clear editor
    }
}

void TaskPane::onArchiveTask()
{
    QModelIndex index = m_listView->currentIndex();
    if (!index.isValid()) return;

    int taskId = m_model->taskIdAt(index.row());
    DatabaseManager::instance().archiveTask(taskId);
    m_model->refresh();
    emit taskSelected(-1);
}

void TaskPane::onReactivateTask()
{
    QModelIndex index = m_listView->currentIndex();
    if (!index.isValid()) return;

    int taskId = m_model->taskIdAt(index.row());
    DatabaseManager::instance().reactivateTask(taskId);
    m_model->refresh();
    emit taskSelected(-1);
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
