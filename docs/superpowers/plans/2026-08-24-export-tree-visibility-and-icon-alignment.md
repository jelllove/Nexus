# Export Tree Visibility and Icon Alignment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ensure export Step-2 always shows a visible 3-level Product -> Main Task -> Sub Task tree and uses the same emoji status markers as the existing task UI.

**Architecture:** Keep the export flow in `MainWindow`, but make tree-node construction explicit with node-type metadata so Product/Main/Sub are always distinguishable and extractable. For Main tasks with no subtasks, add a disabled placeholder child to keep the third level visible without affecting export data. Keep markdown generation in `TaskExportService` and ensure only real subtasks are exported.

**Tech Stack:** C++17, Qt6 Widgets (`QTreeWidget`, tri-state checkbox behavior), existing `DatabaseManager`, existing `TaskExportService`.

---

## Scope Check

This is a focused refinement of one subsystem (export picker + export output mapping). No split is needed.

## File Structure

- **Modify:** `src/app/MainWindow.cpp`
  - Build explicit 3-level tree nodes, add disabled placeholder child nodes, keep emoji prefixes aligned with existing task status icons, and ignore placeholders during export extraction.
- **Modify:** `src/services/TaskExportService.cpp`
  - Ensure markdown subtask output only contains real subtasks and keeps Product -> Main -> Sub hierarchy wording consistent.
- **Modify:** `src/services/TaskExportService.h`
  - Keep DTO fields aligned with `MainWindow.cpp` output (no orphan fields/missing assignments).
- **Modify:** `README.md`
  - Confirm the feature line reflects mandatory 3-level tree visibility and icon alignment.

---

### Task 1: Make Step-2 tree explicitly 3-level with node metadata

**Files:**
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Define node roles/types for Product/Main/Sub and placeholder**

```cpp
enum NodeType {
    ProductNode = 1,
    MainTaskNode = 2,
    SubTaskNode = 3,
    EmptySubTaskPlaceholderNode = 4
};

constexpr int RoleNodeType = Qt::UserRole + 20;
constexpr int RoleId = Qt::UserRole;
constexpr int RoleProductId = Qt::UserRole + 1;
constexpr int RoleTitle = Qt::UserRole + 2;
constexpr int RoleSubtaskCompleted = Qt::UserRole + 3;
constexpr int RoleWorkStatusText = Qt::UserRole + 4;
constexpr int RoleWorkStatusIcon = Qt::UserRole + 5;
```

- [ ] **Step 2: Build Product and Main nodes with existing emoji conventions**

```cpp
auto *productItem = new QTreeWidgetItem(tree, {QString::fromUtf8("📚 ") + productName});
productItem->setFlags((productItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsSelectable);
productItem->setCheckState(0, Qt::Unchecked);
productItem->setData(0, RoleNodeType, ProductNode);
productItem->setData(0, RoleProductId, productId);

auto *taskItem = new QTreeWidgetItem(productItem, {Task::workStatusIcon(task.workStatus) + " " + title});
taskItem->setFlags((taskItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsSelectable);
taskItem->setCheckState(0, Qt::Unchecked);
taskItem->setData(0, RoleNodeType, MainTaskNode);
taskItem->setData(0, RoleId, task.id);
taskItem->setData(0, RoleProductId, productId);
taskItem->setData(0, RoleTitle, task.title);
taskItem->setData(0, RoleWorkStatusText, Task::workStatusToString(task.workStatus));
taskItem->setData(0, RoleWorkStatusIcon, Task::workStatusIcon(task.workStatus));
```

- [ ] **Step 3: Add real subtask children or one disabled placeholder child**

```cpp
const QList<SubTask> subtasks = DatabaseManager::instance().getSubtasks(task.id);
if (subtasks.isEmpty()) {
    auto *placeholder = new QTreeWidgetItem(taskItem, {QString::fromUtf8("🫥 (No Sub Task)")});
    placeholder->setFlags((placeholder->flags() & ~Qt::ItemIsUserCheckable) & ~Qt::ItemIsSelectable);
    placeholder->setData(0, RoleNodeType, EmptySubTaskPlaceholderNode);
    placeholder->setDisabled(true);
} else {
    for (const SubTask &subtask : subtasks) {
        const QString subTitle = subtask.title.trimmed().isEmpty() ? "Untitled sub task" : subtask.title;
        auto *subtaskItem = new QTreeWidgetItem(taskItem, {
            QString("%1 %2").arg(subtask.completed ? "✅" : "⬜", subTitle)
        });
        subtaskItem->setFlags((subtaskItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsSelectable);
        subtaskItem->setCheckState(0, Qt::Unchecked);
        subtaskItem->setData(0, RoleNodeType, SubTaskNode);
        subtaskItem->setData(0, RoleId, subtask.id);
        subtaskItem->setData(0, RoleTitle, subtask.title);
        subtaskItem->setData(0, RoleSubtaskCompleted, subtask.completed);
    }
}
```

- [ ] **Step 4: Keep tri-state propagation but skip disabled placeholder from child toggles**

```cpp
const Qt::CheckState state = item->checkState(0);
if (state != Qt::PartiallyChecked) {
    for (int i = 0; i < item->childCount(); ++i) {
        QTreeWidgetItem *child = item->child(i);
        if (!child->isDisabled()) {
            child->setCheckState(0, state);
        }
    }
}
```

- [ ] **Step 5: Build to validate tree logic compiles**

Run:  
`"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus`

Expected: Build succeeds with no enum/role/type compile errors.

- [ ] **Step 6: Commit tree visibility change**

```bash
git add src/app/MainWindow.cpp
git commit -m "fix: always show 3-level export tree with empty-subtask placeholders"
```

---

### Task 2: Extract selections while ignoring placeholders and preserving hierarchy

**Files:**
- Modify: `src/app/MainWindow.cpp`
- Modify: `src/services/TaskExportService.h`

- [ ] **Step 1: Extract only real subtask selections (ignore placeholder nodes)**

```cpp
QList<ExportSubTaskItem> selectedSubtasks;
for (int k = 0; k < taskItem->childCount(); ++k) {
    auto *subtaskItem = taskItem->child(k);
    const int nodeType = subtaskItem->data(0, RoleNodeType).toInt();
    if (nodeType != SubTaskNode) {
        continue;
    }
    if (subtaskItem->checkState(0) != Qt::Checked) {
        continue;
    }
    selectedSubtasks.append({
        subtaskItem->data(0, RoleTitle).toString(),
        subtaskItem->data(0, RoleSubtaskCompleted).toBool()
    });
}
```

- [ ] **Step 2: Keep Main export rule explicit**

```cpp
const bool shouldExportTask = (taskState == Qt::Checked ||
                               taskState == Qt::PartiallyChecked ||
                               !selectedSubtasks.isEmpty());
if (!shouldExportTask) {
    continue;
}
```

- [ ] **Step 3: Verify DTO assignment contains status icon data**

```cpp
exportItem.workStatusText = taskItem->data(0, RoleWorkStatusText).toString();
exportItem.workStatusIcon = taskItem->data(0, RoleWorkStatusIcon).toString();
exportItem.subtasks = selectedSubtasks;
```

- [ ] **Step 4: Confirm `ExportTaskItem` declaration stays in sync**

```cpp
struct ExportTaskItem {
    QString productName;
    QString statusText;
    QString title;
    QString contentHtml;
    QDateTime updatedAt;
    QList<ExportSubTaskItem> subtasks;
    QString simpleDescription;
    QString workStatusText;
    QString workStatusIcon;
};
```

- [ ] **Step 5: Build to validate extraction path**

Run:  
`"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus`

Expected: Build succeeds with no DTO mismatch errors.

- [ ] **Step 6: Commit extraction fix**

```bash
git add src/app/MainWindow.cpp src/services/TaskExportService.h
git commit -m "fix: export only real selected subtasks and keep status metadata mapping"
```

---

### Task 3: Keep markdown hierarchy strict and placeholder-free

**Files:**
- Modify: `src/services/TaskExportService.cpp`

- [ ] **Step 1: Keep hierarchy headings explicit**

```cpp
stream << "## 📚 " << currentProduct << "\n\n";
stream << "### " << statusEmoji(status) << " " << taskTitle << "\n\n";
stream << "#### 🪜 Selected Sub Tasks\n\n";
```

- [ ] **Step 2: Ensure placeholder text is never emitted in markdown**

```cpp
if (item.subtasks.isEmpty()) {
    stream << "- 💤 No sub tasks\n\n";
} else {
    for (const ExportSubTaskItem &subtask : item.subtasks) {
        const QString subTitle = normalizeHeading(subtask.title, "Untitled sub task");
        stream << "- [" << (subtask.completed ? "x" : " ") << "] "
               << (subtask.completed ? "✅ " : "⬜ ")
               << subTitle << "\n";
    }
    stream << "\n";
}
```

- [ ] **Step 3: Keep task-status emoji and existing visual badges intact**

```cpp
stream << makeBadge("Task Status", status, statusColor(status)) << " "
       << makeBadge("Work", workStatusText, workStatusColor(workStatusText)) << " "
       << makeBadge("Subs", QString("%1/%2 done").arg(doneSubs).arg(totalSubs), subBadgeColor)
       << "\n\n";
```

- [ ] **Step 4: Build to verify markdown renderer**

Run:  
`"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus`

Expected: Build succeeds and export path links.

- [ ] **Step 5: Commit markdown hierarchy fix**

```bash
git add src/services/TaskExportService.cpp
git commit -m "fix: keep export markdown hierarchy and remove placeholder leakage"
```

---

### Task 4: Docs and end-to-end validation

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Update README export bullet to mention mandatory 3-level tree**

```md
- **Markdown Export** — Export selected Active tasks via a mandatory Product → Main Task → Sub Task tree selector (including visible empty-subtask placeholders in picker), with colorful badge + emoji Markdown output and optional AI-generated one-line descriptions
```

- [ ] **Step 2: Build before manual checks**

Run:  
`"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus`

Expected: PASS.

- [ ] **Step 3: Manual check A (product with subtasks)**

Flow:
1. Open app -> File -> Export Tasks to Markdown.
2. Step-1 choose single product with known subtasks.
3. Step-2 verify Product > Main > Sub is visible.
4. Select one main + subset of subtasks.

Expected:
- Third level visible.
- Main icon uses same status emoji style as task list.
- Export contains only checked subtasks under selected main.

- [ ] **Step 4: Manual check B (product/main without subtasks)**

Flow:
1. In Step-2 find a main task with no subtasks.

Expected:
- Disabled `🫥 (No Sub Task)` child is visible.
- It cannot be checked.
- Export does not contain placeholder text.

- [ ] **Step 5: Manual check C (product-level selection)**

Flow:
1. Check a Product root.
2. Confirm all real descendants become checked.
3. Export markdown.

Expected:
- Product -> Main -> Sub hierarchy preserved in output.

- [ ] **Step 6: Commit docs/verification polish**

```bash
git add README.md
git commit -m "docs: clarify export picker 3-level tree visibility behavior"
```

---

### Task 5: Final integration and push

**Files:**
- Modify: none (verification and release prep only)

- [ ] **Step 1: Diff scope check**

Run: `git --no-pager diff --stat`

Expected: Changes limited to:
- `src/app/MainWindow.cpp`
- `src/services/TaskExportService.h`
- `src/services/TaskExportService.cpp`
- `README.md`

- [ ] **Step 2: Final build**

Run:  
`"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus`

Expected: PASS.

- [ ] **Step 3: Create integration commit**

```bash
git add src/app/MainWindow.cpp src/services/TaskExportService.h src/services/TaskExportService.cpp README.md
git commit -m "fix: enforce visible 3-level export tree and status icon alignment"
```

- [ ] **Step 4: Push branch**

```bash
git push origin agents/export-functionality-check
```

