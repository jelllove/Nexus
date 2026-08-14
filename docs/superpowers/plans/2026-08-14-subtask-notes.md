# Subtask Notes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give each subtask an independently stored, searchable rich-text note that opens in the existing editor and supports the same autosave and AI actions as a main task.

**Architecture:** Introduce a small `EditorTarget` value type so task IDs and subtask IDs never share an implicit namespace. Extend SQLite schema version 6 to version 7 for subtask content, timestamps, history, and FTS5; route the existing editor and AI callbacks through the typed target; and teach the task list to display and select layer-aware search results.

**Tech Stack:** C++17, Qt 6 Widgets/WebEngine/WebChannel/Sql/Network/Test, SQLite/FTS5, CMake/CTest, TipTap.

---

## File Structure

- Create `src/models/EditorTarget.h` — typed identity for no selection, main task, or subtask.
- Create `src/models/SearchResult.h` — search result containing a parent task plus an optional matched subtask.
- Create `tests/SubtaskFeatureTests.cpp` — migration, persistence, search, model, and click-routing tests.
- Create `tests/FreshDatabaseTests.cpp` — isolated-process verification of the final schema for a new database.
- Modify `CMakeLists.txt` — register the new headers and the Qt Test/CTest target.
- Modify `src/models/Task.h` — add subtask content and timestamps.
- Modify `src/db/DatabaseManager.h` and `src/db/DatabaseManager.cpp` — schema v7, subtask CRUD/history, search index, and checked initialization.
- Modify `src/models/TaskListModel.h` and `src/models/TaskListModel.cpp` — typed selection and grouped search rows.
- Modify `src/ui/TaskPane.h` and `src/ui/TaskPane.cpp` — layer-aware selection, deterministic checkbox hit testing, and search display.
- Modify `src/ui/TaskCardDelegate.cpp` — increase subtask title text from 10 pt to 11 pt.
- Modify `src/ui/EditorPane.h` and `src/ui/EditorPane.cpp` — load, rename, autosave, and issue AI requests for either layer.
- Modify `src/app/MainWindow.h` and `src/app/MainWindow.cpp` — typed AI response routing and real search result display.
- Modify `src/main.cpp` — stop startup and show an error when database initialization or migration fails.
- Modify `README.md` — document subtask notes and subtask-aware search.

### Task 1: Establish the typed identity and test harness

**Files:**
- Create: `src/models/EditorTarget.h`
- Create: `src/models/SearchResult.h`
- Create: `tests/SubtaskFeatureTests.cpp`
- Modify: `CMakeLists.txt:10-59,75-89`

- [ ] **Step 1: Verify the current baseline**

Run:

```powershell
npm --prefix editor-bundle run build
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:\Qt\6.8.3\msvc2022_64" -DBUILD_TESTING=ON
cmake --build build --config Release
```

Expected: the editor bundle reports a successful build, CMake configures, and `build\Release\Nexus.exe` is produced.

- [ ] **Step 2: Write the failing identity test**

Create `tests/SubtaskFeatureTests.cpp` with the initial test:

```cpp
#include <QtTest>
#include "models/EditorTarget.h"

class SubtaskFeatureTests : public QObject
{
    Q_OBJECT

private slots:
    void editorTargetsKeepLayersDistinct()
    {
        const EditorTarget task = EditorTarget::task(7);
        const EditorTarget subtask = EditorTarget::subtask(7);

        QVERIFY(task.isValid());
        QVERIFY(subtask.isValid());
        QVERIFY(task != subtask);
        QCOMPARE(task.kind, EditorTargetKind::Task);
        QCOMPARE(subtask.kind, EditorTargetKind::SubTask);
        QCOMPARE(task.id, 7);
        QCOMPARE(subtask.id, 7);
        QVERIFY(!EditorTarget().isValid());
    }
};

QTEST_MAIN(SubtaskFeatureTests)
#include "SubtaskFeatureTests.moc"
```

- [ ] **Step 3: Add the CTest target and verify the test fails**

Append this test setup to `CMakeLists.txt`:

```cmake
include(CTest)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(NexusTests
        tests/SubtaskFeatureTests.cpp
        src/db/DatabaseManager.cpp
        src/models/TaskListModel.cpp
        src/ui/TaskPane.cpp
        src/ui/TaskCardDelegate.cpp
    )
    target_include_directories(NexusTests PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(NexusTests PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Sql
        Qt6::Test
    )

    add_test(NAME NexusTests COMMAND NexusTests)
    set_tests_properties(NexusTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
```

Run:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:\Qt\6.8.3\msvc2022_64" -DBUILD_TESTING=ON
cmake --build build --config Release --target NexusTests
```

Expected: compilation fails because `models/EditorTarget.h` does not exist.

- [ ] **Step 4: Implement the identity and search-result types**

Create `src/models/EditorTarget.h`:

```cpp
#pragma once

#include <QMetaType>

enum class EditorTargetKind {
    None,
    Task,
    SubTask
};

struct EditorTarget {
    EditorTargetKind kind = EditorTargetKind::None;
    int id = -1;

    static EditorTarget task(int taskId) { return {EditorTargetKind::Task, taskId}; }
    static EditorTarget subtask(int subtaskId) { return {EditorTargetKind::SubTask, subtaskId}; }

    bool isValid() const { return kind != EditorTargetKind::None && id > 0; }

    friend bool operator==(const EditorTarget &left, const EditorTarget &right)
    {
        return left.kind == right.kind && left.id == right.id;
    }

    friend bool operator!=(const EditorTarget &left, const EditorTarget &right)
    {
        return !(left == right);
    }
};

Q_DECLARE_METATYPE(EditorTarget)
```

Create `src/models/SearchResult.h`:

```cpp
#pragma once

#include "models/EditorTarget.h"
#include "models/Task.h"

struct SearchResult {
    Task parentTask;
    SubTask matchedSubtask;
    bool isSubtaskMatch = false;

    EditorTarget target() const
    {
        return isSubtaskMatch
            ? EditorTarget::subtask(matchedSubtask.id)
            : EditorTarget::task(parentTask.id);
    }
};
```

Add both headers to the `HEADERS` list in `CMakeLists.txt`.

- [ ] **Step 5: Run the identity test**

Run:

```powershell
cmake --build build --config Release --target NexusTests
ctest --test-dir build -C Release --output-on-failure
```

Expected: `NexusTests` passes.

- [ ] **Step 6: Commit**

```powershell
git add CMakeLists.txt src\models\EditorTarget.h src\models\SearchResult.h tests\SubtaskFeatureTests.cpp
git commit -m "test: add typed editor target harness" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

### Task 2: Migrate and persist subtask notes

**Files:**
- Create: `tests/FreshDatabaseTests.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/models/Task.h:28-34`
- Modify: `src/db/DatabaseManager.h:16-65,87-95`
- Modify: `src/db/DatabaseManager.cpp:18-48,51-110,145-266,637-750`
- Modify: `tests/SubtaskFeatureTests.cpp`

- [ ] **Step 1: Add failing migration and persistence tests**

Extend the test class with a `QTemporaryDir`, a legacy-v6 database seeder, and these test slots:

```cpp
private:
    QTemporaryDir m_tempDir;
    QString m_databasePath;

    void createLegacyV6Database()
    {
        m_databasePath = m_tempDir.filePath("nexus-v6.db");
        {
            QSqlDatabase legacy = QSqlDatabase::addDatabase("QSQLITE", "legacy-v6");
            legacy.setDatabaseName(m_databasePath);
            QVERIFY(legacy.open());
            QSqlQuery query(legacy);
            QVERIFY(query.exec("PRAGMA foreign_keys=ON"));
            QVERIFY(query.exec(
                "CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT NOT NULL, "
                "sort_order INTEGER DEFAULT 0, created_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
                "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP)"));
            QVERIFY(query.exec(
                "CREATE TABLE tasks (id INTEGER PRIMARY KEY, product_id INTEGER NOT NULL, "
                "title TEXT NOT NULL DEFAULT '', content TEXT DEFAULT '', priority INTEGER DEFAULT 2, "
                "status TEXT DEFAULT 'active', sort_order INTEGER DEFAULT 0, "
                "created_at DATETIME DEFAULT CURRENT_TIMESTAMP, updated_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
                "archived_at DATETIME, due_date DATETIME, completed INTEGER DEFAULT 0, "
                "completed_at DATETIME, work_status INTEGER DEFAULT 0, deleted_at DATETIME, "
                "FOREIGN KEY(product_id) REFERENCES products(id) ON DELETE CASCADE)"));
            QVERIFY(query.exec(
                "CREATE TABLE subtasks (id INTEGER PRIMARY KEY, task_id INTEGER NOT NULL, "
                "title TEXT NOT NULL DEFAULT '', completed INTEGER DEFAULT 0, sort_order INTEGER DEFAULT 0, "
                "created_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
                "FOREIGN KEY(task_id) REFERENCES tasks(id) ON DELETE CASCADE)"));
            QVERIFY(query.exec("CREATE TABLE settings (key TEXT PRIMARY KEY, value TEXT)"));
            QVERIFY(query.exec("INSERT INTO settings VALUES ('db_version', '6')"));
            QVERIFY(query.exec("INSERT INTO products(id, name) VALUES (1, 'Product')"));
            QVERIFY(query.exec("INSERT INTO tasks(id, product_id, title, content) "
                               "VALUES (1, 1, 'Parent', '<p>parent-before</p>')"));
            QVERIFY(query.exec("INSERT INTO subtasks(id, task_id, title) VALUES (1, 1, 'Child')"));
            legacy.close();
        }
        QSqlDatabase::removeDatabase("legacy-v6");
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        qRegisterMetaType<EditorTarget>();
        createLegacyV6Database();
        QVERIFY(DatabaseManager::instance().initialize(m_databasePath));
    }

    void migrationAddsSubtaskNoteStorage()
    {
        const SubTask subtask = DatabaseManager::instance().getSubtask(1);
        QCOMPARE(subtask.id, 1);
        QCOMPARE(subtask.content, QString());
        QVERIFY(subtask.createdAt.isValid());
        QVERIFY(subtask.updatedAt.isValid());
        QCOMPARE(DatabaseManager::instance().getSetting("db_version"), QString("7"));
    }

    void parentAndSubtaskWritesStayIndependent()
    {
        auto &database = DatabaseManager::instance();
        QVERIFY(database.updateTaskContent(1, "<p>parent-after</p>"));
        QVERIFY(database.updateSubtaskContent(1, "<p>child-after</p>"));
        QVERIFY(database.updateSubtaskTitle(1, "Child renamed"));

        QCOMPARE(database.getTask(1).content, QString("<p>parent-after</p>"));
        QCOMPARE(database.getSubtask(1).content, QString("<p>child-after</p>"));
        QCOMPARE(database.getTask(1).title, QString("Parent"));
        QCOMPARE(database.getSubtask(1).title, QString("Child renamed"));
    }

    void subtaskHistoryIsIndependent()
    {
        auto &database = DatabaseManager::instance();
        QVERIFY(database.saveContentSnapshot(1, "<p>parent snapshot</p>"));
        QVERIFY(database.saveSubtaskContentSnapshot(1, "<p>child snapshot</p>"));

        QCOMPARE(database.getContentHistory(1), QList<QString>{"<p>parent snapshot</p>"});
        QCOMPARE(database.getSubtaskContentHistory(1), QList<QString>{"<p>child snapshot</p>"});
    }
```

Add the required includes:

```cpp
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include "db/DatabaseManager.h"
```

Create `tests/FreshDatabaseTests.cpp` so a separate test process can initialize the singleton against an empty path:

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include "db/DatabaseManager.h"

class FreshDatabaseTests : public QObject
{
    Q_OBJECT

private slots:
    void freshDatabaseUsesVersionSevenSchema()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        auto &database = DatabaseManager::instance();
        QVERIFY(database.initialize(directory.filePath("fresh.db")));
        QCOMPARE(database.getSetting("db_version"), QString("7"));

        const int productId = database.addProduct("Product");
        const int taskId = database.addTask(productId, "Parent");
        const int subtaskId = database.addSubtask(taskId, "Child");
        QVERIFY(subtaskId > 0);
        QVERIFY(database.updateSubtaskContent(subtaskId, "<p>fresh note</p>"));
        QCOMPARE(database.getSubtask(subtaskId).content, QString("<p>fresh note</p>"));
    }
};

QTEST_GUILESS_MAIN(FreshDatabaseTests)
#include "FreshDatabaseTests.moc"
```

Add a second CTest executable:

```cmake
add_executable(NexusFreshDatabaseTests
    tests/FreshDatabaseTests.cpp
    src/db/DatabaseManager.cpp
)
target_include_directories(NexusFreshDatabaseTests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(NexusFreshDatabaseTests PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)
add_test(NAME NexusFreshDatabaseTests COMMAND NexusFreshDatabaseTests)
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
cmake --build build --config Release --target NexusTests NexusFreshDatabaseTests
```

Expected: compilation fails because the subtask content fields and database methods are absent.

- [ ] **Step 3: Extend the model and database API**

Add to `SubTask` in `src/models/Task.h`:

```cpp
QString content;
QDateTime createdAt;
QDateTime updatedAt;
```

Add these declarations to `DatabaseManager.h`:

```cpp
SubTask getSubtask(int subtaskId);
bool updateSubtaskTitle(int subtaskId, const QString &title);
bool updateSubtaskContent(int subtaskId, const QString &content);
bool saveSubtaskContentSnapshot(int subtaskId, const QString &content);
QList<QString> getSubtaskContentHistory(int subtaskId);
void cleanupOldSubtaskHistory(int subtaskId, int maxAgeMinutes = 60);
```

Change the main-task snapshot declaration from `void` to:

```cpp
bool saveContentSnapshot(int taskId, const QString &content);
```

- [ ] **Step 4: Implement checked schema version 7**

In `createTables()`, create the final subtask tables:

```sql
CREATE TABLE IF NOT EXISTS subtasks (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  task_id INTEGER NOT NULL,
  title TEXT NOT NULL DEFAULT '',
  content TEXT NOT NULL DEFAULT '',
  completed INTEGER DEFAULT 0,
  sort_order INTEGER DEFAULT 0,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (task_id) REFERENCES tasks(id) ON DELETE CASCADE
)
```

```sql
CREATE TABLE IF NOT EXISTS subtask_content_history (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  subtask_id INTEGER NOT NULL,
  content TEXT NOT NULL,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (subtask_id) REFERENCES subtasks(id) ON DELETE CASCADE
)
```

In `migrateDatabase()`, add a transaction-backed v6-to-v7 migration. For a v6 table, create `subtasks_v7` with the final schema, copy all existing IDs and fields while setting `content` to `''` and `updated_at` to `created_at`, drop the old table, rename the new table, create `subtask_content_history`, update `settings.db_version` to `7`, and commit. Check every query and roll back on the first failure.

Use this exact data copy:

```sql
INSERT INTO subtasks_v7
    (id, task_id, title, content, completed, sort_order, created_at, updated_at)
SELECT id, task_id, title, '', completed, sort_order, created_at, created_at
FROM subtasks
```

Change initialization order and propagate migration failure:

```cpp
if (!createTables())
    return false;
if (!migrateDatabase())
    return false;
if (!createFtsTables())
    return false;
return true;
```

- [ ] **Step 5: Implement subtask CRUD and history**

Read subtasks with:

```sql
SELECT id, task_id, title, content, completed, sort_order, created_at, updated_at
FROM subtasks
WHERE task_id = ?
ORDER BY sort_order ASC, id ASC
```

Implement `getSubtask()` with the same columns and `WHERE id = ?`. Update title and content with `updated_at = CURRENT_TIMESTAMP`, and return `false` when SQL execution fails.

Implement the history insert and query against `subtask_content_history`. Change `saveContentSnapshot()` to return `query.exec()` instead of discarding failure.

- [ ] **Step 6: Run migration and persistence tests**

Run:

```powershell
cmake --build build --config Release --target NexusTests
ctest --test-dir build -C Release --output-on-failure
```

Expected: all identity, migration, CRUD, and history tests pass.

- [ ] **Step 7: Commit**

```powershell
git add CMakeLists.txt src\models\Task.h src\db\DatabaseManager.h src\db\DatabaseManager.cpp tests\SubtaskFeatureTests.cpp tests\FreshDatabaseTests.cpp
git commit -m "feat: persist notes for subtasks" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

### Task 3: Add subtask full-text search and grouped results

**Files:**
- Modify: `src/db/DatabaseManager.h:64-65`
- Modify: `src/db/DatabaseManager.cpp:113-143,752-822`
- Modify: `src/models/TaskListModel.h:8-81`
- Modify: `src/models/TaskListModel.cpp:15-155`
- Modify: `tests/SubtaskFeatureTests.cpp`

- [ ] **Step 1: Write failing search and model tests**

Add:

```cpp
void searchReturnsTypedParentAndSubtaskMatches()
{
    auto &database = DatabaseManager::instance();
    QVERIFY(database.updateTaskContent(1, "<p>parent-only-key</p>"));
    QVERIFY(database.updateSubtaskContent(1, "<p>child-only-key</p>"));

    const QList<SearchResult> parentResults = database.searchItems("parent-only-key");
    QCOMPARE(parentResults.size(), 1);
    QCOMPARE(parentResults.first().target(), EditorTarget::task(1));

    const QList<SearchResult> childResults = database.searchItems("child-only-key");
    QCOMPARE(childResults.size(), 1);
    QCOMPARE(childResults.first().target(), EditorTarget::subtask(1));
    QCOMPARE(childResults.first().parentTask.id, 1);
}

void searchModelPreservesParentContextAndTypedRows()
{
    const QList<SearchResult> results = DatabaseManager::instance().searchItems("child-only-key");
    TaskListModel model;
    model.loadSearchResults(results);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.targetAt(0), EditorTarget::task(1));
    QCOMPARE(model.targetAt(1), EditorTarget::subtask(1));
    QVERIFY(model.index(1).data(TaskListModel::IsSubTaskRole).toBool());
    QCOMPARE(model.index(1).data(TaskListModel::ContentRole).toString(),
             QString("<p>child-only-key</p>"));
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
cmake --build build --config Release --target NexusTests
```

Expected: compilation fails because `searchItems()`, typed search loading, and `targetAt()` are absent.

- [ ] **Step 3: Add the subtask FTS5 index**

Extend `createFtsTables()` with:

```sql
CREATE VIRTUAL TABLE IF NOT EXISTS subtasks_fts USING fts5(
  title, content, content=subtasks, content_rowid=id
)
```

Before creating the virtual table, query `sqlite_master` to record whether `subtasks_fts` already exists. Add insert, delete, and update triggers mirroring the existing task triggers, but targeting `subtasks` and `subtasks_fts`. When the virtual table is newly created, build its index from the external content table:

```sql
INSERT INTO subtasks_fts(subtasks_fts) VALUES ('rebuild')
```

Treat absence of FTS5 as non-fatal, preserving the `LIKE` fallback.

- [ ] **Step 4: Implement typed search**

Replace the public `searchTasks()` entry point with:

```cpp
QList<SearchResult> searchItems(const QString &query, int productId = -1);
```

Keep the existing main-task FTS query and add this subtask query:

```sql
SELECT
  t.id, t.product_id, t.title, t.content, t.priority, t.status,
  t.sort_order, t.created_at, t.updated_at, t.archived_at, t.due_date, t.work_status,
  s.id, s.task_id, s.title, s.content, s.completed, s.sort_order, s.created_at, s.updated_at
FROM subtasks s
INNER JOIN subtasks_fts f ON s.id = f.rowid
INNER JOIN tasks t ON t.id = s.task_id
WHERE subtasks_fts MATCH ?
ORDER BY rank
```

When `productId > 0`, add `AND t.product_id = ?`. On FTS failure, run the equivalent `s.title LIKE ? OR s.content LIKE ?` query. Map main matches to `SearchResult{task, SubTask(), false}` and child matches to `SearchResult{parentTask, subtask, true}`.

- [ ] **Step 5: Teach the list model to group search rows**

Add `ContentRole`, `CreatedAtRole`, and `UpdatedAtRole` handling for subtask rows. Replace `m_activeTaskId` with `EditorTarget m_activeTarget`, add:

```cpp
void loadSearchResults(const QList<SearchResult> &results);
EditorTarget targetAt(int row) const;
int rowForTarget(const EditorTarget &target) const;
void setActiveTarget(const EditorTarget &target);
EditorTarget activeTarget() const;
```

`loadSearchResults()` must append each parent row once and append each matched subtask beneath it. Use a `QSet<int>` of appended parent IDs. Do not query unrelated siblings while displaying search results.

For `IsActiveTaskRole`, compare both target kind and ID:

```cpp
if (row.type == DisplayRow::SubTaskRow)
    return m_activeTarget == EditorTarget::subtask(row.subtask.id);
return m_activeTarget == EditorTarget::task(row.task.id);
```

- [ ] **Step 6: Run search tests**

Run:

```powershell
cmake --build build --config Release --target NexusTests
ctest --test-dir build -C Release --output-on-failure
```

Expected: typed parent/subtask search and grouped model tests pass.

- [ ] **Step 7: Commit**

```powershell
git add src\db\DatabaseManager.h src\db\DatabaseManager.cpp src\models\TaskListModel.h src\models\TaskListModel.cpp tests\SubtaskFeatureTests.cpp
git commit -m "feat: search subtask titles and notes" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

### Task 4: Make subtask rows selectable without breaking checkboxes

**Files:**
- Modify: `src/ui/TaskPane.h:15-55`
- Modify: `src/ui/TaskPane.cpp:66-126,329-414`
- Modify: `src/ui/TaskCardDelegate.cpp:96-136,452-460`
- Modify: `tests/SubtaskFeatureTests.cpp`

- [ ] **Step 1: Write the failing click-routing test**

Add a test that loads product 1, expands task 1, clicks the subtask body, and then clicks its checkbox:

```cpp
void subtaskBodySelectsButCheckboxOnlyToggles()
{
    TaskPane pane;
    pane.resize(420, 500);
    pane.loadTasks(1);

    auto *model = pane.findChild<TaskListModel *>();
    auto *view = pane.findChild<QListView *>();
    QVERIFY(model);
    QVERIFY(view);
    model->toggleExpand(1);
    pane.show();
    QTest::qWait(50);

    const QModelIndex subtaskIndex = model->index(1);
    QSignalSpy selectionSpy(&pane, &TaskPane::itemSelected);

    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      view->visualRect(subtaskIndex).center());
    QCOMPARE(selectionSpy.count(), 1);
    QCOMPARE(qvariant_cast<EditorTarget>(selectionSpy.takeFirst().at(0)),
             EditorTarget::subtask(1));

    QStyleOptionViewItem option;
    option.rect = view->visualRect(subtaskIndex);
    option.font = view->font();
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      TaskCardDelegate::subtaskCheckboxRect(option).center());
    QCOMPARE(selectionSpy.count(), 0);
    QVERIFY(DatabaseManager::instance().getSubtask(1).completed);
}
```

Include `ui/TaskPane.h`, `ui/TaskCardDelegate.h`, and `QSignalSpy`.

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build --config Release --target NexusTests
ctest --test-dir build -C Release --output-on-failure
```

Expected: the test fails because subtask body clicks currently return without emitting a selection.

- [ ] **Step 3: Implement deterministic click routing**

Replace `taskSelected(int)` with:

```cpp
void itemSelected(const EditorTarget &target);
```

Add:

```cpp
void showSearchResults(const QList<SearchResult> &results);
void refreshCurrentView();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void handleItemClick(const QModelIndex &index, const QPoint &viewportPosition);
```

Install the event filter on `m_listView->viewport()` and route mouse-release positions to `handleItemClick()`. Remove the `QListView::clicked` connection so one click cannot be handled twice.

For a subtask checkbox hit, toggle completion and return without changing the active target or emitting. For the rest of a subtask row:

```cpp
const EditorTarget target = EditorTarget::subtask(
    index.data(TaskListModel::SubTaskIdRole).toInt());
m_model->setActiveTarget(target);
m_listView->setCurrentIndex(index);
m_listView->viewport()->update();
emit itemSelected(target);
```

Use the equivalent `EditorTarget::task(taskId)` flow for main rows.

- [ ] **Step 4: Add search display and context-menu consistency**

`showSearchResults()` calls `m_model->loadSearchResults(results)` and sets the title label to `"Search Results"`. `refreshCurrentView()` reloads the current normal status; search refreshes are initiated by `MainWindow` with the current query.

When an active subtask is renamed from the context menu, emit `itemSelected(activeTarget)` after refreshing so the editor title reloads. When the active subtask is deleted, emit an invalid `EditorTarget()` to clear the editor.

- [ ] **Step 5: Increase the subtask font**

In `TaskCardDelegate::paint()`, change:

```cpp
titleFont.setPointSize(11);
```

Keep regular weight. Increase the compact row height from 30 to 32 pixels so the larger text is not clipped:

```cpp
return QSize(280, 32);
```

- [ ] **Step 6: Run the click-routing test**

Run:

```powershell
cmake --build build --config Release --target NexusTests
ctest --test-dir build -C Release --output-on-failure
```

Expected: body click emits exactly one subtask target; checkbox click changes completion and emits no selection.

- [ ] **Step 7: Commit**

```powershell
git add src\ui\TaskPane.h src\ui\TaskPane.cpp src\ui\TaskCardDelegate.cpp tests\SubtaskFeatureTests.cpp
git commit -m "feat: open selectable subtask rows" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

### Task 5: Route the rich editor and AI through `EditorTarget`

**Files:**
- Modify: `src/ui/EditorPane.h:12-50`
- Modify: `src/ui/EditorPane.cpp:26-208`
- Modify: `src/app/MainWindow.h:24-63`
- Modify: `src/app/MainWindow.cpp:89-106,218-345`
- Modify: `tests/SubtaskFeatureTests.cpp`

- [ ] **Step 1: Add the target-routing persistence regression test**

Add a table-driven test proving that identical numeric IDs route to different storage:

```cpp
void typedContentWritesRouteToTheirOwnLayer()
{
    auto &database = DatabaseManager::instance();
    const auto write = [&database](const EditorTarget &target, const QString &content) {
        if (target.kind == EditorTargetKind::Task)
            return database.updateTaskContent(target.id, content);
        if (target.kind == EditorTargetKind::SubTask)
            return database.updateSubtaskContent(target.id, content);
        return false;
    };

    QVERIFY(write(EditorTarget::task(1), "<p>task routed</p>"));
    QVERIFY(write(EditorTarget::subtask(1), "<p>subtask routed</p>"));
    QCOMPARE(database.getTask(1).content, QString("<p>task routed</p>"));
    QCOMPARE(database.getSubtask(1).content, QString("<p>subtask routed</p>"));
    QVERIFY(!write(EditorTarget(), "<p>invalid</p>"));
}
```

- [ ] **Step 2: Run the test and confirm the persistence contract**

Run:

```powershell
cmake --build build --config Release --target NexusTests
ctest --test-dir build -C Release --output-on-failure
```

Expected: the new contract test passes, proving the storage layer can safely route overlapping numeric IDs before the editor API changes.

- [ ] **Step 3: Change the editor API and signals**

Replace `loadTask(int)` and `m_currentTaskId` with:

```cpp
void loadItem(const EditorTarget &target);
EditorTarget currentTarget() const { return m_currentTarget; }

signals:
    void contentChanged(const EditorTarget &target, const QString &content);
    void titleChanged(const EditorTarget &target, const QString &title);
    void generateTitleRequested(const EditorTarget &target, const QString &content);
    void summarizeRequested(const EditorTarget &target, const QString &content);
    void autoGenerateTitleRequested(const EditorTarget &target, const QString &content);
    void saveFailed(const QString &message);

private:
    EditorTarget m_currentTarget;
```

- [ ] **Step 4: Implement layer-aware loading and title editing**

In `loadItem()`, flush an active autosave before switching. An invalid target calls `clear()`. For a task, load `Task`; for a subtask, load `SubTask`; map either to local `title`, `content`, `createdAt`, and `updatedAt` values before using the existing JavaScript and bridge calls.

In the `__TITLE__:` branch:

```cpp
bool saved = false;
if (m_currentTarget.kind == EditorTargetKind::Task)
    saved = DatabaseManager::instance().updateTaskTitle(m_currentTarget.id, newTitle);
else if (m_currentTarget.kind == EditorTargetKind::SubTask)
    saved = DatabaseManager::instance().updateSubtaskTitle(m_currentTarget.id, newTitle);

if (!saved) {
    emit saveFailed("Failed to save the item title.");
    return;
}
emit titleChanged(m_currentTarget, newTitle);
```

- [ ] **Step 5: Implement layer-aware autosave, history, and AI requests**

Route content and snapshot writes by target kind. Check both return values:

```cpp
const bool contentSaved = m_currentTarget.kind == EditorTargetKind::Task
    ? database.updateTaskContent(m_currentTarget.id, content)
    : database.updateSubtaskContent(m_currentTarget.id, content);
const bool historySaved = m_currentTarget.kind == EditorTargetKind::Task
    ? database.saveContentSnapshot(m_currentTarget.id, content)
    : database.saveSubtaskContentSnapshot(m_currentTarget.id, content);

if (!contentSaved || !historySaved) {
    emit saveFailed("Failed to save the item note.");
    return;
}
emit contentChanged(m_currentTarget, content);
```

Read the selected layer's title before auto-generating a blank title. Emit all manual and automatic AI signals with `m_currentTarget`. Image insertion remains shared and unchanged.

- [ ] **Step 6: Update `MainWindow` slots and pending AI state**

Use these slot signatures:

```cpp
void onItemSelected(const EditorTarget &target);
void onItemTitleChanged(const EditorTarget &target, const QString &title);
void onGenerateTitleRequested(const EditorTarget &target, const QString &content);
void onSummarizeRequested(const EditorTarget &target, const QString &content);
```

Replace both integer pending IDs with:

```cpp
EditorTarget m_titleTarget;
EditorTarget m_summaryTarget;
```

Connect `TaskPane::itemSelected` to `onItemSelected`. `onItemSelected()` calls `m_editorPane->loadItem(target)`. Selecting a product passes an invalid target to clear the editor. `onItemTitleChanged()` reloads the selected product's task list; Task 6 makes that refresh search-aware. Connect `EditorPane::saveFailed` to a warning message and status-bar update.

- [ ] **Step 7: Route generated titles and summaries to the originating layer**

Store the requested target before calling `AIService`. In `onTitleGenerated()`:

```cpp
bool saved = false;
if (m_titleTarget.kind == EditorTargetKind::Task)
    saved = DatabaseManager::instance().updateTaskTitle(m_titleTarget.id, title);
else if (m_titleTarget.kind == EditorTargetKind::SubTask)
    saved = DatabaseManager::instance().updateSubtaskTitle(m_titleTarget.id, title);

if (!saved) {
    onAIError("The generated title could not be saved.");
    return;
}

if (m_editorPane->currentTarget() == m_titleTarget)
    m_editorPane->loadItem(m_titleTarget);
m_titleTarget = EditorTarget();
```

For summaries, load content from the pending target's `Task` or `SubTask`, append the existing HTML-escaped AI summary block, and save through `updateTaskContent()` or `updateSubtaskContent()`. Reload only when the editor still shows the pending target. Reset the pending target after either success or error.

- [ ] **Step 8: Build and run tests**

Run:

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Expected: `Nexus.exe` and `NexusTests.exe` build and all tests pass.

- [ ] **Step 9: Commit the integrated editor and AI flow**

```powershell
git add src\ui\EditorPane.h src\ui\EditorPane.cpp src\app\MainWindow.h src\app\MainWindow.cpp tests\SubtaskFeatureTests.cpp
git commit -m "feat: edit subtask notes in the main editor" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

### Task 6: Integrate real search display and checked startup

**Files:**
- Modify: `src/app/MainWindow.h:24-63`
- Modify: `src/app/MainWindow.cpp:89-106,218-345`
- Modify: `src/main.cpp:41-48`

- [ ] **Step 1: Display both search layers**

Implement:

```cpp
void MainWindow::onSearchRequested(const QString &query)
{
    const QList<SearchResult> results =
        DatabaseManager::instance().searchItems(query);
    m_taskPane->showSearchResults(results);
    statusBar()->showMessage(
        results.isEmpty()
            ? QString("No results found for '%1'").arg(query)
            : QString("Found %1 result(s)").arg(results.size()),
        3000);
}
```

Clicking a search row already emits its typed target, so `onItemSelected()` calls `m_editorPane->loadItem(target)`. Clearing search reloads the selected product and clears an editor target that is no longer visible.

- [ ] **Step 2: Keep title and AI refreshes in the current view**

After a title edit or AI result, call `onSearchRequested(m_searchBar->searchText())` when search text is non-empty. Otherwise reload the selected product. This preserves search context while updating visible titles and note matches.

- [ ] **Step 3: Surface database initialization failure**

Change startup to:

```cpp
if (!DatabaseManager::instance().initialize(dbPath.isEmpty() ? QString() : dbPath)) {
    QMessageBox::critical(nullptr, "Nexus",
                          "Nexus could not initialize its database.");
    return EXIT_FAILURE;
}
```

Add `<QMessageBox>` and `<cstdlib>` includes. Do not run backup or cleanup after a failed initialization.

- [ ] **Step 4: Build the integrated application and run tests**

Run:

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Expected: `Nexus.exe` and `NexusTests.exe` build; all tests pass.

- [ ] **Step 5: Commit search and checked startup**

```powershell
git add src\app\MainWindow.h src\app\MainWindow.cpp src\main.cpp
git commit -m "feat: display layer-aware search results" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

### Task 7: Verify cascade cleanup and fallback search

**Files:**
- Modify: `tests/SubtaskFeatureTests.cpp`
- Modify: `src/db/DatabaseManager.cpp`

- [ ] **Step 1: Write the failing cleanup test**

Create a second parent and child so the primary fixture remains available:

```cpp
void subtaskDataCascadesOnPermanentParentDeletion()
{
    auto &database = DatabaseManager::instance();
    const int taskId = database.addTask(1, "Disposable parent");
    const int subtaskId = database.addSubtask(taskId, "Disposable child");
    QVERIFY(taskId > 0);
    QVERIFY(subtaskId > 0);
    QVERIFY(database.updateSubtaskContent(subtaskId, "<p>disposable note</p>"));
    QVERIFY(database.saveSubtaskContentSnapshot(subtaskId, "<p>snapshot</p>"));

    QVERIFY(database.permanentlyDeleteTask(taskId));
    QCOMPARE(database.getSubtask(subtaskId).id, -1);
    QVERIFY(database.getSubtaskContentHistory(subtaskId).isEmpty());
    QVERIFY(database.searchItems("disposable note").isEmpty());
}
```

- [ ] **Step 2: Run the cleanup test**

Run:

```powershell
cmake --build build --config Release --target NexusTests
ctest --test-dir build -C Release --output-on-failure
```

Expected: pass if foreign keys and FTS delete triggers are correct; otherwise fail at the orphaned history or search assertion.

- [ ] **Step 3: Add the FTS5-unavailable fallback test**

Declare this slot after the cascade test so it can remove the test database's virtual table without affecting earlier cases:

```cpp
void zzSubtaskSearchFallsBackWhenFtsIsUnavailable()
{
    {
        QSqlDatabase connection =
            QSqlDatabase::addDatabase("QSQLITE", "disable-subtask-fts");
        connection.setDatabaseName(m_databasePath);
        QVERIFY(connection.open());
        QSqlQuery query(connection);
        QVERIFY(query.exec("DROP TABLE subtasks_fts"));
        connection.close();
    }
    QSqlDatabase::removeDatabase("disable-subtask-fts");

    const QList<SearchResult> results =
        DatabaseManager::instance().searchItems("child-only-key");
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().target(), EditorTarget::subtask(1));
}
```

- [ ] **Step 4: Run the fallback test**

Run:

```powershell
cmake --build build --config Release --target NexusTests
ctest --test-dir build -C Release --output-on-failure
```

Expected: the FTS query reports a warning and the `LIKE` fallback returns the matching subtask.

- [ ] **Step 5: Fix only an exposed cascade, trigger, or fallback defect**

If the test fails, ensure:

```sql
FOREIGN KEY (subtask_id) REFERENCES subtasks(id) ON DELETE CASCADE
```

and:

```sql
CREATE TRIGGER IF NOT EXISTS subtasks_ad AFTER DELETE ON subtasks BEGIN
  INSERT INTO subtasks_fts(subtasks_fts, rowid, title, content)
  VALUES ('delete', old.id, old.title, old.content);
END
```

Do not add manual child deletion when the foreign-key cascade is working.

- [ ] **Step 6: Run all tests**

Run:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 7: Commit**

```powershell
git add tests\SubtaskFeatureTests.cpp src\db\DatabaseManager.cpp
git commit -m "test: cover subtask cleanup and search fallback" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

### Task 8: Document and perform end-to-end verification

**Files:**
- Modify: `README.md:9-23,24-39`

- [ ] **Step 1: Update feature documentation**

Add these feature bullets:

```markdown
- **Subtask Notes** — Select a subtask to edit its own rich-text note in the main editor, with autosave, images, AI title generation, and AI summaries
- **Layer-Aware Search** — Search main-task and subtask titles and notes, then open the matching layer directly
```

Update the database architecture description to mention subtask content/history and the subtask FTS5 index.

- [ ] **Step 2: Run the complete automated verification**

Run:

```powershell
npm --prefix editor-bundle run build
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:\Qt\6.8.3\msvc2022_64" -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
git diff --check
```

Expected: editor bundle succeeds, `Nexus` and `NexusTests` build, all tests pass, and `git diff --check` prints nothing.

- [ ] **Step 3: Perform the UI smoke test**

Run:

```powershell
.\build\Release\Nexus.exe
```

Verify in order:

1. Create a main task and add a subtask.
2. Expand the parent and confirm the subtask title is slightly larger but still regular weight.
3. Click the subtask body and confirm its title and empty note open in the right editor.
4. Enter formatted text and insert an image; wait three seconds, switch to the parent, and switch back.
5. Confirm the parent and subtask notes remain independent.
6. Click the subtask checkbox and confirm it toggles completion without replacing the editor selection.
7. Generate a subtask title and summary with the configured AI endpoint.
8. Search for unique text in the subtask note and open the subtask from the result.
9. Clear search and confirm the selected product's normal task list returns.
10. Restart Nexus and confirm the subtask note persists.

- [ ] **Step 4: Commit documentation**

```powershell
git add README.md
git commit -m "docs: describe subtask notes and search" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

- [ ] **Step 5: Request code review**

Invoke the `requesting-code-review` skill against the complete branch diff. Address only high-confidence correctness, security, or regression findings, then rerun the complete automated verification.
