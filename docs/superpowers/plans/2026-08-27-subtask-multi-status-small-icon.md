# Sub Task Multi-Status Small Icon Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add main-task-like multi-status support to every sub task, with a smaller clickable status icon and persisted status migration for existing data.

**Architecture:** Extend `subtasks` persistence with `work_status`, map legacy `completed` values during migration, and make `work_status` the source of truth. Reuse the existing main-task status enum/icon system (`TaskWorkStatus`, `Task::workStatusIcon`) for consistency, while rendering sub task status with a smaller icon and wiring status-change menus in `TaskPane`.

**Tech Stack:** C++17, Qt6 Widgets/Model-View (`QListView`, `QStyledItemDelegate`, `QMenu`), SQLite via `QSqlQuery`, existing `DatabaseManager` + `TaskListModel`.

---

## Scope Check

This is a single coherent feature spanning one vertical slice (DB -> model -> list rendering -> interaction -> export completion mapping). No project split is needed.

## File Structure

- **Modify:** `src/models/Task.h`
  - Extend `SubTask` with `TaskWorkStatus workStatus`.
- **Modify:** `src/db/DatabaseManager.h`
  - Add sub task status update API.
- **Modify:** `src/db/DatabaseManager.cpp`
  - Add migration for `subtasks.work_status`.
  - Read/write `work_status` in sub task CRUD.
  - Keep `completed` compatibility synchronized.
- **Modify:** `src/models/TaskListModel.h`
  - Add sub task status roles.
- **Modify:** `src/models/TaskListModel.cpp`
  - Expose sub task status/icon/completed derived from `work_status`.
- **Modify:** `src/ui/TaskCardDelegate.h`
  - Add sub task status icon hitbox helper.
- **Modify:** `src/ui/TaskCardDelegate.cpp`
  - Replace sub task checkbox rendering with small status icon rendering.
- **Modify:** `src/ui/TaskPane.cpp`
  - Add sub task status popup/menu behavior.
  - Route sub task icon click to status menu, non-icon click to editor selection.
- **Modify:** `src/services/TaskExportService.cpp`
  - Treat sub task completion via `work_status == Completed`.
- **Modify:** `README.md`
  - Mention sub tasks now support multi-status with icon menu.

---

### Task 1: Add sub task `work_status` persistence and migration

**Files:**
- Modify: `src/models/Task.h`
- Modify: `src/db/DatabaseManager.h`
- Modify: `src/db/DatabaseManager.cpp`
- Test: manual DB migration verification in app runtime (no automated DB test target exists in repo)

- [ ] **Step 1: Extend `SubTask` model with status field**

```cpp
struct SubTask {
    int id = -1;
    int taskId = -1;
    QString title;
    QString content;
    bool completed = false;
    TaskWorkStatus workStatus = TaskWorkStatus::NotStarted;
    int sortOrder = 0;
    QDateTime createdAt;
    QDateTime updatedAt;
};
```

- [ ] **Step 2: Add DB API for direct sub task status update**

```cpp
// DatabaseManager.h
bool updateSubtaskWorkStatus(int subtaskId, TaskWorkStatus workStatus);
```

- [ ] **Step 3: Add migration for `subtasks.work_status` and legacy backfill**

```cpp
// In migrateDatabase(), next version gate after current latest
if (dbVersion < 9) {
    query.exec("PRAGMA table_info(subtasks)");
    bool hasWorkStatus = false;
    while (query.next()) {
        if (query.value(1).toString() == "work_status") {
            hasWorkStatus = true;
            break;
        }
    }

    if (!hasWorkStatus) {
        if (!query.exec("ALTER TABLE subtasks ADD COLUMN work_status INTEGER DEFAULT 0")) {
            qWarning() << "Failed to add subtasks.work_status column:" << query.lastError().text();
            return false;
        }
    }

    // User-approved migration rule
    if (!query.exec(
            "UPDATE subtasks "
            "SET work_status = CASE WHEN completed = 1 THEN 3 ELSE 0 END "
            "WHERE work_status IS NULL")) {
        qWarning() << "Failed to backfill subtasks.work_status from completed:" << query.lastError().text();
        return false;
    }

    setSetting("db_version", "9");
    dbVersion = 9;
}
```

- [ ] **Step 4: Read/write `work_status` in sub task CRUD and keep compatibility sync**

```cpp
// SELECTs include work_status
query.prepare("SELECT id, task_id, title, content, completed, work_status, sort_order, created_at, updated_at "
              "FROM subtasks WHERE task_id = ? ORDER BY sort_order ASC, id ASC");

st.completed = query.value(4).toBool();
st.workStatus = static_cast<TaskWorkStatus>(query.value(5).toInt());

// New subtasks default to NotStarted and completed=0
query.prepare("INSERT INTO subtasks (task_id, title, completed, work_status, sort_order, updated_at) "
              "VALUES (?, ?, 0, 0, (SELECT COALESCE(MAX(sort_order), 0) + 1 FROM subtasks WHERE task_id = ?), CURRENT_TIMESTAMP)");

// Direct status update keeps completed in sync
bool DatabaseManager::updateSubtaskWorkStatus(int subtaskId, TaskWorkStatus workStatus)
{
    QSqlQuery query(m_db);
    query.prepare(
        "UPDATE subtasks "
        "SET work_status = ?, completed = ?, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = ?");
    query.addBindValue(static_cast<int>(workStatus));
    query.addBindValue(workStatus == TaskWorkStatus::Completed ? 1 : 0);
    query.addBindValue(subtaskId);
    return query.exec();
}

// Keep toggle API mapped to status for old call sites
bool DatabaseManager::toggleSubtask(int subtaskId, bool completed)
{
    const TaskWorkStatus next = completed ? TaskWorkStatus::Completed : TaskWorkStatus::NotStarted;
    return updateSubtaskWorkStatus(subtaskId, next);
}
```

- [ ] **Step 5: Build to verify migration/API compile**

Run:
`"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus`

Expected: build succeeds, no `TaskWorkStatus`/SQL field mismatch compile errors.

- [ ] **Step 6: Commit persistence slice**

```bash
git add src/models/Task.h src/db/DatabaseManager.h src/db/DatabaseManager.cpp
git commit -m "feat: add subtask work status persistence and migration"
```

---

### Task 2: Expose sub task status through `TaskListModel`

**Files:**
- Modify: `src/models/TaskListModel.h`
- Modify: `src/models/TaskListModel.cpp`
- Test: manual list rendering smoke check in app (model has no standalone test target in repo)

- [ ] **Step 1: Add roles for sub task status value and icon**

```cpp
// TaskListModel.h Roles enum
SubTaskWorkStatusRole,
SubTaskWorkStatusIconRole,
```

- [ ] **Step 2: Return derived sub task status data in `data()`**

```cpp
if (row.type == DisplayRow::SubTaskRow) {
    switch (role) {
        case SubTaskWorkStatusRole:
            return static_cast<int>(row.subtask.workStatus);
        case SubTaskWorkStatusIconRole:
            return Task::workStatusIcon(row.subtask.workStatus);
        case SubTaskCompletedRole:
            return row.subtask.workStatus == TaskWorkStatus::Completed;
        // ...existing cases
    }
}
```

- [ ] **Step 3: Build to verify role wiring compiles**

Run:
`"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus`

Expected: build succeeds, no missing-role enum or switch warnings/errors.

- [ ] **Step 4: Commit model slice**

```bash
git add src/models/TaskListModel.h src/models/TaskListModel.cpp
git commit -m "feat: expose subtask work status roles in task list model"
```

---

### Task 3: Replace sub task checkbox with small status icon and icon-click status menu

**Files:**
- Modify: `src/ui/TaskCardDelegate.h`
- Modify: `src/ui/TaskCardDelegate.cpp`
- Modify: `src/ui/TaskPane.cpp`
- Test: manual UI interaction test (list click + popup menu + persistence)

- [ ] **Step 1: Add sub task status icon hitbox helper**

```cpp
// TaskCardDelegate.h
static QRect subtaskStatusIconRect(const QStyleOptionViewItem &option);

// TaskCardDelegate.cpp
QRect TaskCardDelegate::subtaskStatusIconRect(const QStyleOptionViewItem &option)
{
    QRect rect = option.rect.adjusted(4, 1, -4, -1);
    return QRect(rect.left() + 28, rect.top() + 5, 20, 20);
}
```

- [ ] **Step 2: Render small status icon for sub tasks (remove checkbox paint)**

```cpp
// In subtask branch of TaskCardDelegate::paint
TaskWorkStatus subWs = static_cast<TaskWorkStatus>(
    index.data(TaskListModel::SubTaskWorkStatusRole).toInt());
const QString subIcon = Task::workStatusIcon(subWs);

QRect statusRect = subtaskStatusIconRect(option);
QFont statusFont = option.font;
statusFont.setPointSize(9); // smaller than main task icon
painter->setFont(statusFont);
painter->setPen(QColor("#2c3e50"));
painter->drawText(statusRect, Qt::AlignCenter, subIcon);

QRect titleRect(statusRect.right() + 8, rect.top() + 4, rect.width() - 72, 22);
```

- [ ] **Step 3: Add reusable sub task status popup in `TaskPane`**

```cpp
auto showSubTaskStatusPopup = [&](int subtaskId, int currentStatus, const QPoint &pos) {
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

    QList<QAction*> actions;
    for (int i = 0; i < 5; ++i) {
        QAction *action = menu.addAction(QString::fromUtf8(labels[i]));
        if (i == currentStatus) action->setEnabled(false);
        actions.append(action);
    }

    QAction *selected = menu.exec(pos);
    if (!selected) return;
    for (int i = 0; i < actions.size(); ++i) {
        if (selected == actions[i]) {
            DatabaseManager::instance().updateSubtaskWorkStatus(subtaskId, static_cast<TaskWorkStatus>(i));
            refreshCurrentList();
            return;
        }
    }
};
```

- [ ] **Step 4: Wire click behavior: icon click opens status menu, row click still opens sub task editor**

```cpp
if (isSubTask) {
    const int subtaskId = index.data(TaskListModel::SubTaskIdRole).toInt();
    const int currentStatus = index.data(TaskListModel::SubTaskWorkStatusRole).toInt();
    QRect statusRect = TaskCardDelegate::subtaskStatusIconRect(option);

    if (statusRect.contains(clickPos)) {
        showSubTaskStatusPopup(subtaskId, currentStatus, QCursor::pos());
        return;
    }

    m_model->setActiveTaskId(-1);
    m_listView->viewport()->update();
    emit subTaskSelected(subtaskId);
    return;
}
```

- [ ] **Step 5: Replace sub task context menu `Mark Complete` with `Set Status` submenu**

```cpp
if (isSubTask) {
    int subtaskId = index.data(TaskListModel::SubTaskIdRole).toInt();
    int currentStatus = index.data(TaskListModel::SubTaskWorkStatusRole).toInt();

    QMenu *statusMenu = menu.addMenu("Set Status");
    statusMenu->setStyleSheet(menu.styleSheet());
    QAction *statusActions[5];
    // same 5 labels as main task
    // disable current one

    QAction *renameAction = menu.addAction("Rename");
    menu.addSeparator();
    QAction *deleteAction = menu.addAction("Delete");

    QAction *selected = menu.exec(m_listView->viewport()->mapToGlobal(pos));
    for (int i = 0; i < 5; ++i) {
        if (selected == statusActions[i]) {
            DatabaseManager::instance().updateSubtaskWorkStatus(subtaskId, static_cast<TaskWorkStatus>(i));
            refreshCurrentList();
            return;
        }
    }
    // existing rename/delete branches unchanged
}
```

- [ ] **Step 6: Build and run manual UI verification**

Run build:
`"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release`

Run app:
`C:\XQQ\Nexus.worktrees\export-functionality-check\build\Release\Nexus.exe`

Expected manual results:
- Sub task rows show small status icon (no checkbox).
- Icon click opens status popup and persists after refresh/reopen.
- Clicking non-icon row area still opens sub task content editor.

- [ ] **Step 7: Commit UI slice**

```bash
git add src/ui/TaskCardDelegate.h src/ui/TaskCardDelegate.cpp src/ui/TaskPane.cpp
git commit -m "feat: add subtask status icon and popup status menu"
```

---

### Task 4: Align export/preview completed mapping + docs + regression pass

**Files:**
- Modify: `src/services/TaskExportService.cpp`
- Modify: `README.md`
- Test: release build + export/preview manual regression

- [ ] **Step 1: Use status-driven completion in export/preview mapping**

```cpp
// Wherever subtask completion is evaluated for markdown/export payload
const bool isCompleted = (subTask.workStatus == TaskWorkStatus::Completed);
// replace legacy subTask.completed direct usage
```

- [ ] **Step 2: Update README feature bullets**

```md
- Sub Tasks now support full work status (Not Started / Ongoing / Paused / Completed / Waiting) with smaller status icons.
- Click a Sub Task status icon to change status via popup menu.
```

- [ ] **Step 3: Run full validation command set**

Run:
`"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release`

Run:
`"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir "C:\XQQ\Nexus.worktrees\export-functionality-check\build" -C Release --output-on-failure`

Expected:
- Build succeeds.
- `ctest` reports no failures (it may report "No tests were found", which is acceptable in this repo baseline).

- [ ] **Step 4: Manual regression checklist**

```text
1) Existing DB with old subtasks:
   - completed=1 rows show Completed icon
   - completed=0 rows show Not Started icon
2) New subtask defaults to Not Started.
3) Main task status icon/popup still works unchanged.
4) Export Markdown and Preview reflect subtask completed only when status is Completed.
```

- [ ] **Step 5: Commit integration + docs**

```bash
git add src/services/TaskExportService.cpp README.md
git commit -m "feat: align subtask export completion with work status"
```

---

## Self-Review (Completed)

- **Spec coverage:** all confirmed decisions (same status set, icon-click popup, checkbox replacement, migration rule, export completion mapping) are covered by Tasks 1-4.
- **Placeholder scan:** no TBD/TODO placeholders remain.
- **Type consistency:** all tasks use `TaskWorkStatus`, `updateSubtaskWorkStatus`, and `SubTaskWorkStatusRole` consistently.
