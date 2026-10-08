# Task Export Tree + Markdown Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Upgrade export selection to a real Product -> Main Task -> Sub Task selectable tree and produce more visually rich, status-differentiated Markdown output.

**Architecture:** Keep export orchestration in `MainWindow`, but refactor Step-2 tree logic to support tri-state parent-child linkage and subtask-level selection. Keep Markdown generation in `TaskExportService` and extend it with color-like visual differentiation using Shields badges and emoji. Reuse existing `DatabaseManager`/`AIService` pathways and preserve current fallback behavior.

**Tech Stack:** C++17, Qt6 Widgets, Qt SQL, existing `DatabaseManager`, existing `AIService`, Markdown + GitHub shields badges.

---

## Scope Check

This is one subsystem refinement of the existing export feature. No decomposition needed.

## File Structure

- **Modify:** `src/app/MainWindow.cpp`
  - Replace current Step-2 selection internals with 3-level tri-state tree and selected-subtask extraction logic.
- **Modify:** `src/app/MainWindow.h`
  - Keep signatures aligned with `MainWindow.cpp` changes and update helper declarations if they are renamed.
- **Modify:** `src/services/TaskExportService.h`
  - Extend export DTO fields required for richer output metadata (work status display).
- **Modify:** `src/services/TaskExportService.cpp`
  - Render Product -> Main -> selected Sub hierarchy and add colorful badges + richer emoji sections.
- **Modify:** `README.md`
  - Mention enhanced tree selection and richer Markdown styling.

---

### Task 1: Extend export DTO for richer presentation metadata

**Files:**
- Modify: `src/services/TaskExportService.h`

- [ ] **Step 1: Add work status fields to export DTO**

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

- [ ] **Step 2: Rebuild to verify header/API compatibility**

Run:  
`"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus`  

Expected: Build succeeds with no struct member or signature mismatch.

- [ ] **Step 3: Commit DTO extension**

```bash
git add src/services/TaskExportService.h
git commit -m "feat: extend export dto for markdown visual metadata"
```

---

### Task 2: Implement true 3-level tri-state tree selection in Step-2 dialog

**Files:**
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Build Product -> Main -> Sub tree with all nodes checkable**

```cpp
auto *productItem = new QTreeWidgetItem(tree, {QString::fromUtf8("📚 ") + productName});
productItem->setFlags(productItem->flags() | Qt::ItemIsUserCheckable);
productItem->setCheckState(0, Qt::Unchecked);
productItem->setData(0, Qt::UserRole + 10, productId);

auto *taskItem = new QTreeWidgetItem(productItem, {Task::workStatusIcon(task.workStatus) + " " + taskTitle});
taskItem->setFlags(taskItem->flags() | Qt::ItemIsUserCheckable);
taskItem->setCheckState(0, Qt::Unchecked);
taskItem->setData(0, Qt::UserRole, task.id);
taskItem->setData(0, Qt::UserRole + 1, productId);
taskItem->setData(0, Qt::UserRole + 2, task.title);
taskItem->setData(0, Qt::UserRole + 3, Task::workStatusToString(task.workStatus));
taskItem->setData(0, Qt::UserRole + 4, Task::workStatusIcon(task.workStatus));

const QList<SubTask> subtasks = DatabaseManager::instance().getSubtasks(task.id);
for (const SubTask &subtask : subtasks) {
    auto *subItem = new QTreeWidgetItem(taskItem, {
        QString("%1 %2").arg(subtask.completed ? "✅" : "⬜", subtask.title.trimmed().isEmpty() ? "Untitled sub task" : subtask.title)
    });
    subItem->setFlags(subItem->flags() | Qt::ItemIsUserCheckable);
    subItem->setCheckState(0, Qt::Unchecked);
    subItem->setData(0, Qt::UserRole, subtask.id);
    subItem->setData(0, Qt::UserRole + 1, task.id);
    subItem->setData(0, Qt::UserRole + 2, subtask.title);
    subItem->setData(0, Qt::UserRole + 3, subtask.completed);
}
```

- [ ] **Step 2: Add parent-child check linkage and partial-state recomputation**

```cpp
bool updatingChecks = false;
auto updateParentState = [&](QTreeWidgetItem *parent) {
    if (!parent) return;
    int checked = 0;
    int partial = 0;
    const int count = parent->childCount();
    for (int i = 0; i < count; ++i) {
        Qt::CheckState s = parent->child(i)->checkState(0);
        if (s == Qt::Checked) checked++;
        else if (s == Qt::PartiallyChecked) partial++;
    }
    if (checked == count) parent->setCheckState(0, Qt::Checked);
    else if (checked == 0 && partial == 0) parent->setCheckState(0, Qt::Unchecked);
    else parent->setCheckState(0, Qt::PartiallyChecked);
};

connect(tree, &QTreeWidget::itemChanged, tree, [&, updateParentState](QTreeWidgetItem *item, int) {
    if (updatingChecks) return;
    updatingChecks = true;

    const Qt::CheckState state = item->checkState(0);
    for (int i = 0; i < item->childCount(); ++i) {
        item->child(i)->setCheckState(0, state == Qt::PartiallyChecked ? Qt::Checked : state);
    }

    QTreeWidgetItem *parent = item->parent();
    while (parent) {
        updateParentState(parent);
        parent = parent->parent();
    }

    updatingChecks = false;
});
```

- [ ] **Step 3: Keep Select All / Clear All covering all 3 levels**

```cpp
connect(selectAllBtn, &QPushButton::clicked, tree, [tree]() {
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        tree->topLevelItem(i)->setCheckState(0, Qt::Checked);
    }
});
connect(clearAllBtn, &QPushButton::clicked, tree, [tree]() {
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        tree->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
    }
});
```

- [ ] **Step 4: Rebuild and verify UI compiles**

Run:  
`"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus`  

Expected: Build succeeds; no Qt type/lambda capture compile errors.

- [ ] **Step 5: Commit tree selection behavior**

```bash
git add src/app/MainWindow.cpp
git commit -m "feat: add tri-state product-main-sub export tree selection"
```

---

### Task 3: Export selected Main/Sub combinations with explicit rules

**Files:**
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Extract checked state into export DTOs with sub-only support**

```cpp
selectedExportItems.clear();
for (int i = 0; i < tree->topLevelItemCount(); ++i) {
    QTreeWidgetItem *productItem = tree->topLevelItem(i);
    const int productId = productItem->data(0, Qt::UserRole + 10).toInt();
    const QString productName = productNames.value(productId);

    for (int j = 0; j < productItem->childCount(); ++j) {
        QTreeWidgetItem *taskItem = productItem->child(j);
        const int taskId = taskItem->data(0, Qt::UserRole).toInt();
        const Qt::CheckState taskState = taskItem->checkState(0);

        QList<ExportSubTaskItem> selectedSubs;
        for (int k = 0; k < taskItem->childCount(); ++k) {
            QTreeWidgetItem *subItem = taskItem->child(k);
            if (subItem->checkState(0) == Qt::Checked) {
                selectedSubs.append({
                    subItem->data(0, Qt::UserRole + 2).toString(),
                    subItem->data(0, Qt::UserRole + 3).toBool()
                });
            }
        }

        const bool shouldExportTask = (taskState == Qt::Checked || taskState == Qt::PartiallyChecked || !selectedSubs.isEmpty());
        if (!shouldExportTask) continue;

        const QList<Task> tasks = tasksByProduct.value(productId);
        for (const Task &task : tasks) {
            if (task.id != taskId) continue;
            ExportTaskItem item;
            item.productName = productName;
            item.statusText = "Active";
            item.title = task.title;
            item.contentHtml = task.content;
            item.updatedAt = task.updatedAt;
            item.workStatusText = Task::workStatusToString(task.workStatus);
            item.workStatusIcon = Task::workStatusIcon(task.workStatus);
            item.subtasks = selectedSubs;
            selectedExportItems.append(item);
            break;
        }
    }
}
```

- [ ] **Step 2: Keep existing AI/fallback summary flow unchanged**

```cpp
int fallbackCount = 0;
resolveSimpleDescriptions(selectedExportItems, includeDescription, fallbackCount);
```

- [ ] **Step 3: Rebuild and validate selection extraction compiles**

Run:  
`"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus`  

Expected: Build succeeds; export DTO assignment compiles.

- [ ] **Step 4: Commit selection-to-export mapping**

```bash
git add src/app/MainWindow.cpp
git commit -m "feat: export selected subtasks with sub-only task inclusion rule"
```

---

### Task 4: Upgrade Markdown visual styling with colorful badges + richer emoji layout

**Files:**
- Modify: `src/services/TaskExportService.cpp`

- [ ] **Step 1: Add helpers for badge URL encoding and work-status color mapping**

```cpp
#include <QUrl>

QString makeBadge(const QString &label, const QString &message, const QString &color)
{
    const QString l = QString::fromUtf8(QUrl::toPercentEncoding(label));
    const QString m = QString::fromUtf8(QUrl::toPercentEncoding(message));
    return QString("![%1](https://img.shields.io/badge/%2-%3-%4?style=flat-square)")
        .arg(label, l, m, color);
}

QString workColor(const QString &workStatusText)
{
    const QString s = workStatusText.trimmed().toLower();
    if (s == "completed") return "16a34a";
    if (s == "ongoing") return "2563eb";
    if (s == "paused") return "f59e0b";
    if (s == "waiting") return "9333ea";
    return "64748b";
}
```

- [ ] **Step 2: Render product-separated hierarchy with richer visual blocks**

```cpp
stream << "# 📦 Task Export\n\n";
stream << makeBadge("Main Tasks", QString::number(tasks.size()), "0ea5e9") << " "
       << makeBadge("Sub Tasks", QString::number(subTaskCount), "f97316") << " "
       << makeBadge("Exported", exportedAt.toString("yyyy-MM-dd HH:mm:ss"), "64748b") << "\n\n";

stream << "> 🎨 **Legend**: 🟢 Active · 🏃 Ongoing · ✅ Completed · ⏸️ Paused · ⏳ Waiting\n\n";

stream << "## 📚 " << currentProduct << "\n\n";
stream << "### " << statusEmoji(status) << " " << taskTitle << "\n\n";
stream << makeBadge("Task Status", status, "22c55e") << " "
       << makeBadge("Work", item.workStatusText.isEmpty() ? "Not Started" : item.workStatusText, workColor(item.workStatusText)) << " "
       << makeBadge("Subs", QString("%1/%2 done").arg(doneSubs).arg(totalSubs), doneSubs == totalSubs ? "16a34a" : "f59e0b")
       << "\n\n";

stream << "| Field | Value |\n";
stream << "|---|---|\n";
stream << "| Status | " << escapeTableCell(status) << " |\n";
stream << "| Work | " << escapeTableCell(item.workStatusIcon + " " + item.workStatusText) << " |\n";
stream << "| Updated | " << escapeTableCell(item.updatedAt.isValid() ? item.updatedAt.toString(\"yyyy-MM-dd HH:mm\") : \"N/A\") << " |\n\n";
```

- [ ] **Step 3: Make summary block visually stronger**

```cpp
if (!item.simpleDescription.trimmed().isEmpty()) {
    stream << "> 🧠 **AI Summary**\n";
    stream << ">\n";
    stream << "> ✨ " << item.simpleDescription.trimmed() << "\n\n";
}
```

- [ ] **Step 4: Keep selected-subtask-only rendering**

```cpp
if (item.subtasks.isEmpty()) {
    stream << "- 💤 No sub tasks selected\n\n";
} else {
    stream << "#### 🪜 Selected Sub Tasks\n\n";
    for (const ExportSubTaskItem &subtask : item.subtasks) {
        stream << "- [" << (subtask.completed ? "x" : " ") << "] "
               << (subtask.completed ? "✅ " : "⬜ ")
               << normalizeHeading(subtask.title, "Untitled sub task") << "\n";
    }
    stream << "\n";
}
```

- [ ] **Step 5: Rebuild and validate markdown renderer compiles**

Run:  
`"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus`  

Expected: Build succeeds; no QString/QUrl formatting errors.

- [ ] **Step 6: Commit markdown polish**

```bash
git add src/services/TaskExportService.cpp
git commit -m "feat: add colorful badge and emoji markdown export styling"
```

---

### Task 5: Documentation and end-to-end verification

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Update README export bullet for new behavior**

```md
- **Markdown Export** — Export selected Active tasks using a Product → Main Task → Sub Task tree selector, with colorful badge + emoji Markdown output and optional AI one-line summaries
```

- [ ] **Step 2: Rebuild for release confidence**

Run:  
`"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus`  

Expected: Build succeeds.

- [ ] **Step 3: Manual scenario A (full product branch selection)**

Run app:
1. File -> Export Tasks to Markdown
2. Step-1: All products, description on/off any
3. Step-2: check one Product root only
4. Export file

Expected:
- All main/sub under that Product exported.
- Markdown grouped by Product -> Main -> Sub.

- [ ] **Step 4: Manual scenario B (partial subtask selection)**

Run app:
1. In Step-2, keep Main partially selected by checking only some Subs
2. Export file

Expected:
- Parent Main exported.
- Only checked Subs exported.

- [ ] **Step 5: Manual scenario C (visual style check)**

Expected:
- Shields badges visible on GitHub markdown renderer.
- Emoji headings and summary callouts clearly visible.
- Status/work/subtask completion visually differentiated.

- [ ] **Step 6: Commit docs update**

```bash
git add README.md
git commit -m "docs: describe enhanced export tree and markdown visuals"
```

---

### Task 6: Final validation + push prep

**Files:**
- Modify: none (verification only)

- [ ] **Step 1: Verify changed files scope**

Run: `git --no-pager diff --stat`  
Expected: Only export-related source files and README changed.

- [ ] **Step 2: Final build**

Run:  
`"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus`  

Expected: PASS.

- [ ] **Step 3: Create integration commit**

```bash
git add src/app/MainWindow.cpp src/app/MainWindow.h src/services/TaskExportService.h src/services/TaskExportService.cpp README.md
git commit -m "feat: refine export tree selection and markdown visual hierarchy"
```

- [ ] **Step 4: Push branch**

```bash
git push origin agents/export-functionality-check
```
