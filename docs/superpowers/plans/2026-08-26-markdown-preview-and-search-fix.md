# Markdown Preview Window + Search Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep file export unchanged, add a non-modal Markdown preview workflow that reuses export selection, and make global search actually render and restore correctly.

**Architecture:** Extract a shared “prepare markdown payload” path from `MainWindow` so both Export and Preview use the same selection/summarization/markdown generation behavior. Add a dedicated `MarkdownPreviewDialog` for modeless viewing/copy/save. Introduce explicit search mode wiring in task/product panes so global search results display in the middle pane and clearing search restores the pre-search browsing context.

**Tech Stack:** C++17, Qt6 Widgets (QDialog/QTextBrowser/QClipboard/QFileDialog), existing SQLite/FTS search in `DatabaseManager`, existing `TaskExportService`.

---

## File Structure Map

- **Create:** `src/ui/MarkdownPreviewDialog.h`
  - New non-modal preview window interface (set markdown, copy markdown/html, save as md).
- **Create:** `src/ui/MarkdownPreviewDialog.cpp`
  - Implementation for rendering markdown and copy/save actions.
- **Modify:** `src/app/MainWindow.h`
  - Add preview action slot, shared markdown-preparation result struct, search snapshot struct, preview dialog pointer.
- **Modify:** `src/app/MainWindow.cpp`
  - Add Preview menu action.
  - Extract shared prepare logic from export flow.
  - Keep existing export file-write path.
  - Add preview open path.
  - Fix search request/clear behavior to show results + restore context.
- **Modify:** `src/ui/TaskPane.h`
  - Add explicit search result loading + view mode getter/setter APIs used by `MainWindow`.
- **Modify:** `src/ui/TaskPane.cpp`
  - Implement task-pane search mode and restore behavior.
- **Modify:** `src/models/TaskListModel.h`
  - Add optional helper to select a task row by ID after view restore.
- **Modify:** `src/models/TaskListModel.cpp`
  - Keep search result list in model and support restore selection hooks.
- **Modify:** `src/ui/ProductPane.h`
  - Add public `selectProductById` API for context restore.
- **Modify:** `src/ui/ProductPane.cpp`
  - Implement product selection restore for active/archived sections.
- **Modify:** `CMakeLists.txt`
  - Register new preview dialog sources/headers.
- **Modify:** `README.md`
  - Mention Preview workflow and working Search behavior.

---

### Task 1: Extract shared markdown preparation path in MainWindow

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Test: Manual smoke test (export still works)

- [ ] **Step 1: Record failing baseline behavior**

Run app from current build and confirm:
1. Export works and writes file.
2. There is no preview menu entry.
3. Search shows “Found X” but task list does not switch to result list.

Expected: baseline reproduces current behavior before refactor.

- [ ] **Step 2: Add shared payload structs and method signatures in MainWindow header**

```cpp
// MainWindow.h (private)
struct PreparedMarkdownPayload {
    QList<ExportTaskItem> items;
    QString markdown;
    bool includeDescription = false;
    int fallbackCount = 0;
};

struct SearchViewSnapshot {
    int productId = -1;
    int selectedTaskId = -1;
    int taskPaneMode = 0; // cast from TaskPane mode enum
    bool valid = false;
};

bool prepareMarkdownPayload(PreparedMarkdownPayload &payload);
void showMarkdownPreview();
```

- [ ] **Step 3: Refactor `exportTasksToMarkdown()` to call shared prepare path**

```cpp
void MainWindow::exportTasksToMarkdown()
{
    PreparedMarkdownPayload payload;
    if (!prepareMarkdownPayload(payload)) {
        return;
    }
    // existing file save/write message path remains here
}
```

Keep Step1/Step2 tree selection, AI confirmation, AI fallback, and `TaskExportService::buildMarkdown` semantics unchanged.

- [ ] **Step 4: Build to verify refactor compiles**

Run:
```bash
cmake -S . -B build-plan -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build-plan --config Release
```

Expected: build succeeds with no new compile errors.

- [ ] **Step 5: Commit Task 1**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "refactor: share markdown preparation between export flows"
```

---

### Task 2: Add non-modal Markdown preview dialog and menu entry

**Files:**
- Create: `src/ui/MarkdownPreviewDialog.h`
- Create: `src/ui/MarkdownPreviewDialog.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Modify: `CMakeLists.txt`
- Test: Manual preview window behavior

- [ ] **Step 1: Create failing manual test checklist for preview**

Expected-to-fail before implementation:
1. File menu has no “Preview Selected Tasks as Markdown...” item.
2. No modeless preview window exists.
3. Cannot copy markdown/html separately.

- [ ] **Step 2: Add `MarkdownPreviewDialog` header**

```cpp
class MarkdownPreviewDialog : public QDialog {
    Q_OBJECT
public:
    explicit MarkdownPreviewDialog(QWidget *parent = nullptr);
    void setMarkdownContent(const QString &markdown);
private slots:
    void onCopyMarkdown();
    void onCopyHtml();
    void onSaveAs();
private:
    QString m_markdown;
    QTextBrowser *m_view = nullptr;
    QPushButton *m_copyMarkdownBtn = nullptr;
    QPushButton *m_copyHtmlBtn = nullptr;
    QPushButton *m_saveAsBtn = nullptr;
};
```

- [ ] **Step 3: Implement preview dialog logic**

```cpp
void MarkdownPreviewDialog::setMarkdownContent(const QString &markdown)
{
    m_markdown = markdown;
    m_view->setMarkdown(markdown);
}

void MarkdownPreviewDialog::onCopyMarkdown()
{
    QGuiApplication::clipboard()->setText(m_markdown);
}

void MarkdownPreviewDialog::onCopyHtml()
{
    QTextDocument doc;
    doc.setMarkdown(m_markdown);
    QMimeData *mime = new QMimeData();
    mime->setHtml(doc.toHtml());
    mime->setText(doc.toPlainText());
    QGuiApplication::clipboard()->setMimeData(mime);
}
```

- [ ] **Step 4: Wire menu action and non-modal single-instance behavior in MainWindow**

```cpp
QAction *previewAction = fileMenu->addAction("Preview Selected Tasks as &Markdown...");
connect(previewAction, &QAction::triggered, this, &MainWindow::showMarkdownPreview);
```

```cpp
if (!m_markdownPreviewDialog) {
    m_markdownPreviewDialog = new MarkdownPreviewDialog(this);
    m_markdownPreviewDialog->setModal(false);
}
m_markdownPreviewDialog->setMarkdownContent(payload.markdown);
m_markdownPreviewDialog->show();
m_markdownPreviewDialog->raise();
m_markdownPreviewDialog->activateWindow();
```

- [ ] **Step 5: Register new files in CMake**

```cmake
set(SOURCES
    ...
    src/ui/MarkdownPreviewDialog.cpp
)
set(HEADERS
    ...
    src/ui/MarkdownPreviewDialog.h
)
```

- [ ] **Step 6: Run build and manual pass check**

Run:
```bash
cmake --build build-plan --config Release
```

Manual verification:
1. File menu has new Preview entry.
2. Preview follows Step1/Step2 + AI behavior, then opens modeless window.
3. `Copy Markdown`, `Copy HTML`, `Save As .md` all work.
4. Main window remains editable while preview window is open.

- [ ] **Step 7: Commit Task 2**

```bash
git add src/ui/MarkdownPreviewDialog.h src/ui/MarkdownPreviewDialog.cpp src/app/MainWindow.h src/app/MainWindow.cpp CMakeLists.txt
git commit -m "feat: add modeless markdown preview workflow"
```

---

### Task 3: Implement task/product pane APIs for search mode + restore

**Files:**
- Modify: `src/ui/TaskPane.h`
- Modify: `src/ui/TaskPane.cpp`
- Modify: `src/ui/ProductPane.h`
- Modify: `src/ui/ProductPane.cpp`
- Modify: `src/models/TaskListModel.h`
- Modify: `src/models/TaskListModel.cpp`
- Test: Manual search-mode switching checks

- [ ] **Step 1: Add task pane mode/search APIs**

```cpp
// TaskPane.h
enum class ViewMode { Active, Archived, Deleted, SearchResults };
ViewMode viewMode() const;
void loadSearchResults(const QList<Task> &tasks);
void restoreView(int productId, ViewMode mode, int selectedTaskId);
```

- [ ] **Step 2: Implement task pane search mode behavior**

```cpp
void TaskPane::loadSearchResults(const QList<Task> &tasks)
{
    m_model->loadSearchResults(tasks);
    m_currentViewMode = ViewMode::SearchResults;
    m_titleLabel->setText("Tasks (Search Results)");
    m_addButton->setEnabled(false);
}
```

```cpp
void TaskPane::restoreView(int productId, ViewMode mode, int selectedTaskId)
{
    // restore active/archived/deleted mode + product list reload
    // then reselect selectedTaskId if still present
}
```

- [ ] **Step 3: Add product selection restore API**

```cpp
// ProductPane.h
bool selectProductById(int productId);
```

```cpp
// ProductPane.cpp
bool ProductPane::selectProductById(int productId)
{
    // find in active model first, then archived model, set current index, emit productSelected
}
```

- [ ] **Step 4: Ensure model supports post-restore row lookup**

Use existing `rowForTaskId` in `TaskListModel` to reselect when restoring from search; if missing behavior is needed, add helper usage instead of duplicating lookup.

- [ ] **Step 5: Build and manual mode-switch validation**

Run:
```bash
cmake --build build-plan --config Release
```

Manual verification:
1. After entering search mode, add button is disabled.
2. Clearing search restores product highlight + active/archived/deleted mode.
3. Prior selected task is reselected if it still exists.

- [ ] **Step 6: Commit Task 3**

```bash
git add src/ui/TaskPane.h src/ui/TaskPane.cpp src/ui/ProductPane.h src/ui/ProductPane.cpp src/models/TaskListModel.h src/models/TaskListModel.cpp
git commit -m "feat: add search-mode restore APIs for panes"
```

---

### Task 4: Wire global search to real results and restore previous view on clear

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Test: Manual global search scenarios

- [ ] **Step 1: Add search snapshot state to MainWindow**

```cpp
SearchViewSnapshot m_searchSnapshot;
bool m_searchActive = false;
```

- [ ] **Step 2: Replace placeholder search handler with real list loading**

```cpp
void MainWindow::onSearchRequested(const QString &query)
{
    if (query.trimmed().isEmpty()) {
        onSearchCleared();
        return;
    }

    if (!m_searchActive) {
        m_searchSnapshot.productId = m_productPane->selectedProductId();
        m_searchSnapshot.selectedTaskId = m_taskPane->selectedTaskId();
        m_searchSnapshot.taskPaneMode = static_cast<int>(m_taskPane->viewMode());
        m_searchSnapshot.valid = true;
        m_searchActive = true;
    }

    const QList<Task> results = DatabaseManager::instance().searchTasks(query);
    m_taskPane->loadSearchResults(results);
    statusBar()->showMessage(QString("Found %1 result(s)").arg(results.size()), 3000);
}
```

- [ ] **Step 3: Restore pre-search context on clear**

```cpp
void MainWindow::onSearchCleared()
{
    if (!m_searchActive || !m_searchSnapshot.valid) {
        return;
    }

    m_productPane->loadProducts();
    if (m_searchSnapshot.productId > 0) {
        m_productPane->selectProductById(m_searchSnapshot.productId);
    }
    m_taskPane->restoreView(
        m_searchSnapshot.productId,
        static_cast<TaskPane::ViewMode>(m_searchSnapshot.taskPaneMode),
        m_searchSnapshot.selectedTaskId);

    m_searchActive = false;
    m_searchSnapshot.valid = false;
}
```

- [ ] **Step 4: Manual end-to-end search validation**

Manual test matrix:
1. Search text that matches Active tasks from current/other products.
2. Search text that matches Archived tasks.
3. Search text that matches Deleted tasks.
4. Click a result and confirm editor opens correct task.
5. Clear search and confirm previous product/mode/task restoration.

- [ ] **Step 5: Commit Task 4**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "fix: render global search results and restore prior context"
```

---

### Task 5: Final regression pass and documentation update

**Files:**
- Modify: `README.md`
- Test: Build + full manual regression checklist

- [ ] **Step 1: Update README feature bullets**

Add concise bullets:
1. Preview selected tasks as Markdown (modeless dialog, copy markdown/html, save as md).
2. Search now renders global results and restores prior view on clear.

- [ ] **Step 2: Run full build validation**

Run:
```bash
cmake --build build-plan --config Release
```

Expected: successful release build.

- [ ] **Step 3: Run full manual regression checklist**

Checklist:
1. Export file flow still works (same content shape and dialogs).
2. Preview flow works and is non-modal.
3. Search works globally for Active/Archived/Deleted.
4. Clear search restores previous browsing context.
5. No crash when no results / empty query / save-cancel.

- [ ] **Step 4: Commit Task 5**

```bash
git add README.md
git commit -m "docs: describe markdown preview and search behavior"
```

---

## Plan Self-Review

### 1) Spec coverage

- Preview menu + shared flow: Task 1 + Task 2.
- Non-modal preview with copy markdown/html + save: Task 2.
- Search global render and clear restore: Task 3 + Task 4.
- Export regression protection: Task 1 + Task 5.

No uncovered spec requirements remain.

### 2) Placeholder scan

- No TODO/TBD placeholders left.
- Each task has concrete files, commands, and expected checks.

### 3) Type consistency

- `PreparedMarkdownPayload` naming is used consistently.
- `TaskPane::ViewMode` naming is used consistently in restore flow.
- Preview dialog naming is consistently `MarkdownPreviewDialog`.

