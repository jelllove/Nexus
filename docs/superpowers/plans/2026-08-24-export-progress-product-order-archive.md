# Export Progress, Product Ordering, and Product Archive Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver one release that upgrades export UX (AI confirmation + progress/logs + tree-form markdown) and Product management (drag reorder + archived product section) while preserving current Task behavior.

**Architecture:** Keep existing app structure and apply focused upgrades in `MainWindow` (export flow), `TaskExportService` (markdown rendering), `ProductPane` + `ProductListModel` (active/archived product UX + drag reorder), and `DatabaseManager` (product archive schema and APIs). Add a small dedicated `ExportProgressDialog` UI class so progress/log logic does not bloat `MainWindow`. Preserve backward compatibility by defaulting existing products to `active` in migration.

**Tech Stack:** C++17, Qt6 Widgets/Model-View (`QListView`, `QAbstractListModel`, `QDialog`, `QProgressBar`, `QPlainTextEdit`), SQLite via `QSqlQuery`, existing CMake build.

---

## Scope Check

This spec contains two independent subsystems:

1. Export subsystem (`MainWindow`, `TaskExportService`, new progress dialog).
2. Product management subsystem (`DatabaseManager`, `ProductListModel`, `ProductPane`).

Recommendation: two separate plans can reduce risk.  
User decision is single-phase delivery, so this plan keeps one document but isolates work by subsystem and commit boundaries.

## File Structure

- **Create:** `src/ui/ExportProgressDialog.h`
  - Export progress dialog interface: progress value, log append, cancel state.
- **Create:** `src/ui/ExportProgressDialog.cpp`
  - Export progress dialog implementation.
- **Modify:** `src/models/Product.h`
  - Add product archive status fields for model consistency.
- **Modify:** `src/db/DatabaseManager.h`
  - Add product archive and status-scoped reorder APIs.
- **Modify:** `src/db/DatabaseManager.cpp`
  - Add migration columns (`products.status`, `products.archived_at`) and new product queries/actions.
- **Modify:** `src/models/ProductListModel.h`
  - Add status-scoped loading and drag/drop reorder support.
- **Modify:** `src/models/ProductListModel.cpp`
  - Implement status-aware loading and `moveRows`.
- **Modify:** `src/ui/ProductPane.h`
  - Add active/archived list widgets and archive toggle action slots.
- **Modify:** `src/ui/ProductPane.cpp`
  - Render active + archived sections; context actions archive/reactivate; wire drag reorder.
- **Modify:** `src/app/MainWindow.h`
  - Add progress-aware export helper signatures.
- **Modify:** `src/app/MainWindow.cpp`
  - AI second-confirm, progress dialog, cancellable flow, export selection cascade verification.
- **Modify:** `src/services/TaskExportService.cpp`
  - Convert markdown to collapsible tree and remove all badges.
- **Modify:** `src/services/TaskExportService.h`
  - Keep DTO/output contract aligned if helper signatures change.
- **Modify:** `CMakeLists.txt`
  - Register new `ExportProgressDialog` source/header.
- **Modify:** `README.md`
  - Update feature description for new export/product behavior.

---

### Task 1: Add Product archive schema and DatabaseManager APIs

**Files:**
- Modify: `src/models/Product.h`
- Modify: `src/db/DatabaseManager.h`
- Modify: `src/db/DatabaseManager.cpp`
- Test: manual DB/API behavior check (no automated test suite exists in repository)

- [ ] **Step 1: Reproduce current schema gap**

Run:
```bash
git --no-pager grep -n "products.*archived_at" -- src/db/DatabaseManager.cpp src/models/Product.h
```

Expected: no `products.archived_at` model/API coverage.

- [ ] **Step 2: Add Product status fields in model**

```cpp
enum class ProductStatus {
    Active,
    Archived
};

struct Product {
    int id = 0;
    QString name;
    int sortOrder = 0;
    ProductStatus status = ProductStatus::Active;
    QDateTime archivedAt;
    QDateTime createdAt;
    QDateTime updatedAt;
};
```

- [ ] **Step 3: Add archive/query/reorder APIs in DatabaseManager**

```cpp
QList<Product> getProductsByStatus(ProductStatus status);
bool archiveProduct(int id);
bool reactivateProduct(int id);
bool reorderProductsByStatus(const QList<int> &productIds, ProductStatus status);
```

```cpp
if (dbVersion < 7) {
    query.exec("PRAGMA table_info(products)");
    bool hasStatus = false;
    bool hasArchivedAt = false;
    while (query.next()) {
        const QString col = query.value(1).toString();
        if (col == "status") hasStatus = true;
        if (col == "archived_at") hasArchivedAt = true;
    }
    if (!hasStatus) query.exec("ALTER TABLE products ADD COLUMN status TEXT DEFAULT 'active'");
    if (!hasArchivedAt) query.exec("ALTER TABLE products ADD COLUMN archived_at DATETIME");
    setSetting("db_version", "7");
    dbVersion = 7;
}
```

- [ ] **Step 4: Build to catch compile/migration signature errors**

Run:
```bash
"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus
```

Expected: build succeeds.

- [ ] **Step 5: Commit database/model changes**

```bash
git add src/models/Product.h src/db/DatabaseManager.h src/db/DatabaseManager.cpp
git commit -m "feat: add product archive schema and database APIs"
```

---

### Task 2: Extend ProductListModel for status-scoped loading and drag reorder

**Files:**
- Modify: `src/models/ProductListModel.h`
- Modify: `src/models/ProductListModel.cpp`
- Test: manual model behavior check through UI integration in next task

- [ ] **Step 1: Add model API for status filtering**

```cpp
void loadProducts(ProductStatus status = ProductStatus::Active);
void setStatus(ProductStatus status);
ProductStatus status() const { return m_status; }
```

```cpp
private:
    QList<Product> m_products;
    ProductStatus m_status = ProductStatus::Active;
```

- [ ] **Step 2: Add drag/drop model capabilities**

```cpp
Qt::DropActions supportedDropActions() const override;
bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
              const QModelIndex &destinationParent, int destinationRow) override;
```

```cpp
Qt::ItemFlags ProductListModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags base = QAbstractListModel::flags(index);
    if (!index.isValid()) return base | Qt::ItemIsDropEnabled;
    return base | Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
}
```

- [ ] **Step 3: Persist reorder by status**

```cpp
bool ProductListModel::moveRows(const QModelIndex &, int sourceRow, int count,
                                const QModelIndex &, int destinationRow)
{
    if (count != 1 || sourceRow < 0 || sourceRow >= m_products.size()) return false;
    if (destinationRow == sourceRow || destinationRow == sourceRow + 1) return true;

    beginMoveRows(QModelIndex(), sourceRow, sourceRow, QModelIndex(), destinationRow);
    m_products.move(sourceRow, destinationRow > sourceRow ? destinationRow - 1 : destinationRow);
    endMoveRows();

    QList<int> ids;
    for (const Product &p : m_products) ids.append(p.id);
    return DatabaseManager::instance().reorderProductsByStatus(ids, m_status);
}
```

- [ ] **Step 4: Build to verify model interface consistency**

Run:
```bash
"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus
```

Expected: build succeeds.

- [ ] **Step 5: Commit ProductListModel changes**

```bash
git add src/models/ProductListModel.h src/models/ProductListModel.cpp
git commit -m "feat: add product model drag reorder and status-aware loading"
```

---

### Task 3: Rebuild ProductPane with Active + Archived sections and archive actions

**Files:**
- Modify: `src/ui/ProductPane.h`
- Modify: `src/ui/ProductPane.cpp`
- Test: manual Product pane interaction checks

- [ ] **Step 1: Add dual-list UI fields and archived toggle control**

```cpp
QListView *m_activeListView;
QListView *m_archivedListView;
ProductListModel *m_activeModel;
ProductListModel *m_archivedModel;
QPushButton *m_showArchivedButton;
QWidget *m_archivedSection;
```

```cpp
m_showArchivedButton = new QPushButton("Show Archived Products", this);
m_showArchivedButton->setCheckable(true);
connect(m_showArchivedButton, &QPushButton::toggled, m_archivedSection, &QWidget::setVisible);
```

- [ ] **Step 2: Enable drag/drop on both lists**

```cpp
for (QListView *view : {m_activeListView, m_archivedListView}) {
    view->setDragEnabled(true);
    view->setAcceptDrops(true);
    view->setDropIndicatorShown(true);
    view->setDragDropMode(QAbstractItemView::InternalMove);
    view->setDefaultDropAction(Qt::MoveAction);
}
```

- [ ] **Step 3: Add context actions for archive/reactivate**

```cpp
QAction *archiveAction = nullptr;
QAction *reactivateAction = nullptr;
const bool sourceIsActiveList = (sourceView == m_activeListView);
if (sourceIsActiveList) {
    archiveAction = menu.addAction("Archive");
} else {
    reactivateAction = menu.addAction("Reactivate");
}
```

```cpp
if (selected == archiveAction) {
    DatabaseManager::instance().archiveProduct(productId);
    loadProducts();
} else if (selected == reactivateAction) {
    DatabaseManager::instance().reactivateProduct(productId);
    loadProducts();
}
```

- [ ] **Step 4: Build after ProductPane refactor**

Run:
```bash
"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus
```

Expected: build succeeds.

- [ ] **Step 5: Manual UI checks for Product pane**

Flow:
1. Launch app.
2. Confirm only Active products are shown initially.
3. Toggle `Show Archived Products`; confirm archived section appears below active list.
4. Archive one active product; confirm it moves to archived section and task statuses do not change.
5. Reactivate the same product; confirm it returns to active section and task statuses remain unchanged.
6. Drag within Active and within Archived; restart app and confirm both orders persist.

Expected: all checks pass.

- [ ] **Step 6: Commit ProductPane changes**

```bash
git add src/ui/ProductPane.h src/ui/ProductPane.cpp
git commit -m "feat: add active and archived product sections with drag reorder"
```

---

### Task 4: Add dedicated ExportProgressDialog component

**Files:**
- Create: `src/ui/ExportProgressDialog.h`
- Create: `src/ui/ExportProgressDialog.cpp`
- Modify: `CMakeLists.txt`
- Test: build-time integration validation

- [ ] **Step 1: Create dialog header**

```cpp
class ExportProgressDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExportProgressDialog(QWidget *parent = nullptr);
    void setProgress(int value, int maximum);
    void appendLog(const QString &line);
    bool isCancelled() const;

private:
    QProgressBar *m_progressBar;
    QPlainTextEdit *m_logView;
    QPushButton *m_cancelButton;
    bool m_cancelled = false;
};
```

- [ ] **Step 2: Implement dialog behavior**

```cpp
connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
    m_cancelled = true;
    m_cancelButton->setEnabled(false);
    appendLog("Cancellation requested by user.");
});
```

```cpp
void ExportProgressDialog::setProgress(int value, int maximum)
{
    m_progressBar->setMaximum(maximum);
    m_progressBar->setValue(value);
    qApp->processEvents();
}
```

- [ ] **Step 3: Register new files in CMake**

```cmake
list(APPEND SOURCES
    src/ui/ExportProgressDialog.cpp
)

list(APPEND HEADERS
    src/ui/ExportProgressDialog.h
)
```

- [ ] **Step 4: Build to verify new UI component linkage**

Run:
```bash
"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus
```

Expected: build succeeds.

- [ ] **Step 5: Commit progress dialog component**

```bash
git add src/ui/ExportProgressDialog.h src/ui/ExportProgressDialog.cpp CMakeLists.txt
git commit -m "feat: add reusable export progress dialog"
```

---

### Task 5: Upgrade export flow with AI second confirmation, progress logs, and cancel support

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Test: manual export flow validation

- [ ] **Step 1: Add AI second-confirm gate**

```cpp
if (includeDescription) {
    const auto aiConfirm = QMessageBox::question(
        this,
        "Confirm AI Summarization",
        "AI summarization is enabled for this export. Continue?",
        QMessageBox::Yes | QMessageBox::No
    );
    if (aiConfirm != QMessageBox::Yes) {
        return;
    }
}
```

- [ ] **Step 2: Wire ExportProgressDialog into export pipeline**

```cpp
ExportProgressDialog progress(this);
progress.appendLog("Collecting selected tasks");
progress.setProgress(1, 4);
progress.show();

if (progress.isCancelled()) {
    statusBar()->showMessage("Export cancelled", 5000);
    return;
}
```

- [ ] **Step 3: Add progress-aware summary resolver contract**

```cpp
void resolveSimpleDescriptions(
    QList<ExportTaskItem> &items,
    bool includeDescription,
    int &fallbackCount,
    const std::function<void(int, int, const QString&)> &progressCallback,
    const std::function<bool()> &isCancelled
) const;
```

```cpp
if (isCancelled && isCancelled()) {
    return;
}
if (progressCallback) {
    progressCallback(index + 1, items.size(), QString("Summarized %1/%2").arg(index + 1).arg(items.size()));
}
```

- [ ] **Step 4: Keep Product full-select cascade strict**

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

- [ ] **Step 5: Build after export flow changes**

Run:
```bash
"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus
```

Expected: build succeeds.

- [ ] **Step 6: Manual export flow checks**

Flow:
1. Open export Step-1 and confirm AI checkbox is unchecked by default.
2. Check AI option and click OK; verify second confirmation appears.
3. Start export and verify progress dialog updates log lines and progress bar.
4. Click Cancel before write phase; verify no markdown file is created.
5. Run export again without cancel; verify export completes.

Expected: all checks pass.

- [ ] **Step 7: Commit MainWindow export upgrades**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "feat: add ai confirm and progress logging to export flow"
```

---

### Task 6: Rewrite markdown output to collapsible tree and simplified metadata

**Files:**
- Modify: `src/services/TaskExportService.cpp`
- Modify: `src/services/TaskExportService.h` (only if signature/fields require alignment)
- Test: manual markdown output inspection

- [ ] **Step 1: Replace badge-based layout with nested details tree**

```cpp
stream << "# 📦 Task Export\n\n";
QMap<QString, QList<ExportTaskItem>> groupedProducts;
for (const ExportTaskItem &item : tasks) {
    groupedProducts[item.productName].append(item);
}
for (auto it = groupedProducts.cbegin(); it != groupedProducts.cend(); ++it) {
    const QString productName = it.key();
    stream << "<details open>\n";
    stream << "<summary>📚 " << productName << "</summary>\n\n";
    for (const ExportTaskItem &item : it.value()) {
        const QString taskTitle = normalizeHeading(item.title, "Untitled Task");
        stream << "<details>\n";
        stream << "<summary>" << item.workStatusIcon << " " << taskTitle << "</summary>\n\n";
        // task body
        stream << "</details>\n\n";
    }
    stream << "</details>\n\n";
}
```

- [ ] **Step 2: Keep only one metadata line (`Status`)**

```cpp
stream << "- **Status:** " << item.workStatusIcon << " " << workStatusText << "\n\n";
```

- [ ] **Step 3: Remove colored badge output**

```cpp
stream << "# 📦 Task Export\n\n";
stream << "> Exported at " << exportedAt.toString("yyyy-MM-dd HH:mm:ss") << "\n\n";
// No makeBadge output in header or task sections.
```

- [ ] **Step 4: Build after markdown renderer rewrite**

Run:
```bash
"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus
```

Expected: build succeeds.

- [ ] **Step 5: Manual markdown validation**

Flow:
1. Export a dataset with at least two Products and subtasks.
2. Open generated `.md`.
3. Confirm:
   - collapsible Product -> Main -> Sub structure exists,
   - no top badges and no task-level badges,
   - task metadata only contains `Status`,
   - `Updated`, old `Status`, and metadata `Title` lines are absent.

Expected: markdown matches all rules.

- [ ] **Step 6: Commit markdown format rewrite**

```bash
git add src/services/TaskExportService.cpp src/services/TaskExportService.h
git commit -m "feat: switch export markdown to collapsible tree without badges"
```

---

### Task 7: Documentation and final regression

**Files:**
- Modify: `README.md`
- Test: final build + full manual regression

- [ ] **Step 1: Update README feature bullet**

```md
- **Markdown Export** — Export selected Active tasks with optional AI summaries (double-confirmed), visible export progress logs, and a collapsible Product → Main Task → Sub Task markdown tree without color badges
```

- [ ] **Step 2: Run final release build**

Run:
```bash
"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\XQQ\Nexus.worktrees\export-functionality-check\build" --config Release --target Nexus
```

Expected: build succeeds.

- [ ] **Step 3: Execute full manual regression checklist**

Checklist:
1. Product archive/reactivate does not mutate task statuses.
2. Active/Archived Product sections are correct and Archived section is hidden by default.
3. Active and Archived Product lists each support drag reorder and persist order.
4. Export Step-1 AI option defaults off and uses second confirm when enabled.
5. Export progress dialog shows logs, supports cancel, and does not write partial files.
6. Export selection cascades from Product to all real Main/Sub descendants.
7. Export markdown is collapsible tree and badge-free with simplified metadata.

Expected: all checklist items pass.

- [ ] **Step 4: Create integration commit**

```bash
git add README.md src/app/MainWindow.h src/app/MainWindow.cpp src/services/TaskExportService.cpp src/services/TaskExportService.h src/ui/ProductPane.h src/ui/ProductPane.cpp src/models/ProductListModel.h src/models/ProductListModel.cpp src/models/Product.h src/db/DatabaseManager.h src/db/DatabaseManager.cpp src/ui/ExportProgressDialog.h src/ui/ExportProgressDialog.cpp CMakeLists.txt
git commit -m "feat: improve export workflow and add archived product management"
```

- [ ] **Step 5: Push branch**

```bash
git push origin agents/export-functionality-check
```
