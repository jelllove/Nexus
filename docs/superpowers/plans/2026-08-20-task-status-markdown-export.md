# Task Status Markdown Export Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a File-menu export flow that lets users choose active tasks from the current product and save a Markdown summary file containing each selected task’s work status, title, and short note excerpt.

**Architecture:** Keep UI and formatting concerns separated: `MainWindow` coordinates export flow, `ExportTaskSummaryDialog` owns user selection, and a pure `TaskSummaryMarkdownExporter` helper handles validation, text cleanup, and Markdown generation. This keeps business logic testable without QWebEngine or full app boot.

**Tech Stack:** C++17, Qt6 Widgets/Core/Test/Sql, existing Task/Product models, CMake/CTest.

---

## File Structure

- Create `src/export/TaskSummaryMarkdownExporter.h` — validation/result enums and Markdown builder API.
- Create `src/export/TaskSummaryMarkdownExporter.cpp` — HTML stripping, whitespace normalization, truncation, escaping, header/table rendering.
- Create `src/ui/ExportTaskSummaryDialog.h` — dialog API for choosing task IDs.
- Create `src/ui/ExportTaskSummaryDialog.cpp` — checkbox list UI and selection extraction.
- Create `tests/TaskSummaryMarkdownExporterTests.cpp` — TDD coverage for validation + markdown formatting + dialog selection behavior.
- Modify `src/app/MainWindow.h` — add export slot declaration.
- Modify `src/app/MainWindow.cpp` — add File menu action, dialog launch, prechecks, Save As writing, status/error messages.
- Modify `CMakeLists.txt` — compile new files and register a focused export test target.
- Modify `README.md` — document Markdown export capability.

### Task 1: Create failing export tests and wire test target

**Files:**
- Create: `tests/TaskSummaryMarkdownExporterTests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing tests for export validation and markdown rendering**

```cpp
#include <QtTest>
#include <QDateTime>
#include "export/TaskSummaryMarkdownExporter.h"
#include "models/Task.h"

class TaskSummaryMarkdownExporterTests : public QObject
{
    Q_OBJECT

private:
    static Task makeTask(int id, const QString &title, TaskWorkStatus status, const QString &content)
    {
        Task task;
        task.id = id;
        task.title = title;
        task.workStatus = status;
        task.content = content;
        return task;
    }

private slots:
    void validationRejectsMissingProduct()
    {
        const QList<Task> tasks{makeTask(1, "Task A", TaskWorkStatus::NotStarted, "<p>note</p>")};
        const QList<int> selectedIds{1};
        QCOMPARE(
            validateTaskSummaryExportInputs(-1, tasks, selectedIds),
            TaskSummaryExportIssue::NoProductSelected);
    }

    void validationRejectsNoActiveTasks()
    {
        const QList<Task> noTasks;
        const QList<int> selectedIds;
        QCOMPARE(
            validateTaskSummaryExportInputs(10, noTasks, selectedIds),
            TaskSummaryExportIssue::NoActiveTasks);
    }

    void summaryStripsHtmlNormalizesWhitespaceAndTruncates()
    {
        const QString html =
            "<h1>Hello</h1><p>line&nbsp;one</p><p>line two with more words for truncation</p>";
        const QString summary = summarizeTaskContentForMarkdown(html, 20);
        QCOMPARE(summary, QString("Hello line one line…"));
    }

    void markdownIncludesHeaderAndRows()
    {
        const QDateTime ts(QDate(2026, 8, 20), QTime(13, 45, 0), Qt::UTC);
        const QList<Task> tasks{
            makeTask(1, "Task A", TaskWorkStatus::Ongoing, "<p>Build export feature</p>")
        };
        const QString markdown = buildTaskSummaryMarkdown("Nexus Product", ts, tasks, 120);

        QVERIFY(markdown.contains("# Task Summary Export"));
        QVERIFY(markdown.contains("**Product:** Nexus Product"));
        QVERIFY(markdown.contains("| Status | Title | Summary |"));
        QVERIFY(markdown.contains("| Ongoing | Task A | Build export feature |"));
    }

    void markdownEscapesPipeCharactersInCells()
    {
        const QDateTime ts(QDate(2026, 8, 20), QTime(13, 45, 0), Qt::UTC);
        const QList<Task> tasks{
            makeTask(2, "Task | B", TaskWorkStatus::Paused, "<p>a | b</p>")
        };
        const QString markdown = buildTaskSummaryMarkdown("Product", ts, tasks, 120);
        QVERIFY(markdown.contains("| Paused | Task \\| B | a \\| b |"));
    }
};

QTEST_MAIN(TaskSummaryMarkdownExporterTests)
#include "TaskSummaryMarkdownExporterTests.moc"
```

- [ ] **Step 2: Register focused export tests in CMake**

```cmake
add_executable(NexusTaskSummaryExportTests
    tests/TaskSummaryMarkdownExporterTests.cpp
    src/export/TaskSummaryMarkdownExporter.cpp
)
target_include_directories(NexusTaskSummaryExportTests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(NexusTaskSummaryExportTests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)
add_test(NAME NexusTaskSummaryExportTests COMMAND NexusTaskSummaryExportTests)
set_tests_properties(NexusTaskSummaryExportTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_PREFIX_PATH}/bin"
)
```

- [ ] **Step 3: Run tests to confirm RED**

Run:

```powershell
cmake -S . -B build-tests -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:\Qt\6.8.3\msvc2022_64" -DBUILD_TESTING=ON -DNEXUS_BUILD_APP=OFF
cmake --build build-tests --config Release --target NexusTaskSummaryExportTests
ctest --test-dir build-tests -C Release --output-on-failure -R NexusTaskSummaryExportTests
```

Expected: test target fails (missing exporter/dialog implementation or failing assertions), proving tests are guarding the new behavior.

- [ ] **Step 4: Commit failing-test baseline**

```bash
git add CMakeLists.txt tests/TaskSummaryMarkdownExporterTests.cpp
git commit -m "test: add failing markdown export coverage"
```

### Task 2: Implement exporter helper until tests pass

**Files:**
- Create: `src/export/TaskSummaryMarkdownExporter.h`
- Create: `src/export/TaskSummaryMarkdownExporter.cpp`
- Modify: `tests/TaskSummaryMarkdownExporterTests.cpp` (only if expected text needs exact-match correction)

- [ ] **Step 1: Add exporter API**

```cpp
// src/export/TaskSummaryMarkdownExporter.h
#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include "models/Task.h"

enum class TaskSummaryExportIssue {
    None,
    NoProductSelected,
    NoActiveTasks,
    NoTaskSelected
};

TaskSummaryExportIssue validateTaskSummaryExportInputs(
    int productId,
    const QList<Task> &availableTasks,
    const QList<int> &selectedTaskIds);

QString summarizeTaskContentForMarkdown(const QString &htmlContent, int summaryMaxChars = 120);

QString buildTaskSummaryMarkdown(
    const QString &productName,
    const QDateTime &exportedAt,
    const QList<Task> &selectedTasks,
    int summaryMaxChars = 120);
```

- [ ] **Step 2: Implement minimal logic for validation + markdown builder**

```cpp
// src/export/TaskSummaryMarkdownExporter.cpp (core logic)
TaskSummaryExportIssue validateTaskSummaryExportInputs(
    int productId,
    const QList<Task> &availableTasks,
    const QList<int> &selectedTaskIds)
{
    if (productId <= 0) return TaskSummaryExportIssue::NoProductSelected;
    if (availableTasks.isEmpty()) return TaskSummaryExportIssue::NoActiveTasks;
    if (selectedTaskIds.isEmpty()) return TaskSummaryExportIssue::NoTaskSelected;
    return TaskSummaryExportIssue::None;
}

QString summarizeTaskContentForMarkdown(const QString &htmlContent, int summaryMaxChars)
{
    QString plain = htmlContent;
    plain.replace(QRegularExpression("<[^>]*>"), " ");
    plain.replace("&nbsp;", " ");
    plain.replace(QRegularExpression("\\s+"), " ");
    plain = plain.trimmed();
    if (summaryMaxChars > 0 && plain.size() > summaryMaxChars) {
        plain = plain.left(summaryMaxChars - 1).trimmed() + QString::fromUtf8("…");
    }
    return plain;
}
```

- [ ] **Step 3: Complete row escaping and markdown output format**

```cpp
// src/export/TaskSummaryMarkdownExporter.cpp (formatting pieces)
namespace {
QString escapeMarkdownCell(QString value)
{
    value.replace("\\", "\\\\");
    value.replace("|", "\\|");
    value.replace("\n", " ");
    return value.trimmed();
}
}

QString buildTaskSummaryMarkdown(
    const QString &productName,
    const QDateTime &exportedAt,
    const QList<Task> &selectedTasks,
    int summaryMaxChars)
{
    QString out;
    out += "# Task Summary Export\n\n";
    out += QString("**Product:** %1  \n").arg(escapeMarkdownCell(productName));
    out += QString("**Exported At:** %1\n\n")
               .arg(exportedAt.toLocalTime().toString("yyyy-MM-dd HH:mm:ss"));
    out += "| Status | Title | Summary |\n";
    out += "| --- | --- | --- |\n";

    for (const Task &task : selectedTasks) {
        out += QString("| %1 | %2 | %3 |\n")
                   .arg(escapeMarkdownCell(Task::workStatusToString(task.workStatus)))
                   .arg(escapeMarkdownCell(task.title))
                   .arg(escapeMarkdownCell(summarizeTaskContentForMarkdown(task.content, summaryMaxChars)));
    }
    return out;
}
```

- [ ] **Step 4: Run export tests to confirm GREEN**

Run:

```powershell
cmake --build build-tests --config Release --target NexusTaskSummaryExportTests
ctest --test-dir build-tests -C Release --output-on-failure -R NexusTaskSummaryExportTests
```

Expected: `NexusTaskSummaryExportTests` passes.

- [ ] **Step 5: Commit helper implementation**

```bash
git add src/export/TaskSummaryMarkdownExporter.h src/export/TaskSummaryMarkdownExporter.cpp
git commit -m "feat: add markdown export builder and validation helpers"
```

### Task 3: Add failing dialog selection tests

**Files:**
- Modify: `tests/TaskSummaryMarkdownExporterTests.cpp`
- Create: `src/ui/ExportTaskSummaryDialog.h`
- Create: `src/ui/ExportTaskSummaryDialog.cpp`
- Modify: `CMakeLists.txt` (add dialog source to export test target)

- [ ] **Step 1: Add tests for dialog defaults and selected IDs**

```cpp
// tests/TaskSummaryMarkdownExporterTests.cpp
#include <QListWidget>
#include "ui/ExportTaskSummaryDialog.h"

void dialogPreselectsAllTasksByDefault()
{
    QList<Task> tasks{
        makeTask(11, "T1", TaskWorkStatus::NotStarted, "<p>a</p>"),
        makeTask(12, "T2", TaskWorkStatus::Completed, "<p>b</p>")
    };
    ExportTaskSummaryDialog dialog(tasks);
    QCOMPARE(dialog.selectedTaskIds(), QList<int>({11, 12}));
}

void dialogReturnsOnlyCheckedTaskIds()
{
    QList<Task> tasks{
        makeTask(11, "T1", TaskWorkStatus::NotStarted, "<p>a</p>"),
        makeTask(12, "T2", TaskWorkStatus::Completed, "<p>b</p>")
    };
    ExportTaskSummaryDialog dialog(tasks);
    auto *list = dialog.findChild<QListWidget*>("exportTaskList");
    QVERIFY(list);
    list->item(1)->setCheckState(Qt::Unchecked);
    QCOMPARE(dialog.selectedTaskIds(), QList<int>({11}));
}
```

- [ ] **Step 2: Add minimal dialog stub so tests compile but fail at runtime**

```cpp
// src/ui/ExportTaskSummaryDialog.h (stub)
#pragma once

#include <QDialog>
#include <QList>
#include "models/Task.h"

class ExportTaskSummaryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExportTaskSummaryDialog(const QList<Task> &, QWidget *parent = nullptr);
    QList<int> selectedTaskIds() const;
};
```

```cpp
// src/ui/ExportTaskSummaryDialog.cpp (stub)
#include "ExportTaskSummaryDialog.h"

ExportTaskSummaryDialog::ExportTaskSummaryDialog(const QList<Task> &, QWidget *parent)
    : QDialog(parent)
{
}

QList<int> ExportTaskSummaryDialog::selectedTaskIds() const
{
    return {};
}
```

- [ ] **Step 3: Add dialog stub to export test target**

```cmake
add_executable(NexusTaskSummaryExportTests
    tests/TaskSummaryMarkdownExporterTests.cpp
    src/export/TaskSummaryMarkdownExporter.cpp
    src/ui/ExportTaskSummaryDialog.cpp
)
```

- [ ] **Step 4: Run tests to confirm RED for dialog behavior**

Run:

```powershell
cmake --build build-tests --config Release --target NexusTaskSummaryExportTests
ctest --test-dir build-tests -C Release --output-on-failure -R NexusTaskSummaryExportTests
```

Expected: dialog tests fail because the stub returns no selected IDs.

- [ ] **Step 5: Commit failing dialog tests**

```bash
git add tests/TaskSummaryMarkdownExporterTests.cpp src/ui/ExportTaskSummaryDialog.h src/ui/ExportTaskSummaryDialog.cpp CMakeLists.txt
git commit -m "test: add failing export dialog selection tests"
```

### Task 4: Implement export dialog and make tests green

**Files:**
- Modify: `src/ui/ExportTaskSummaryDialog.h`
- Modify: `src/ui/ExportTaskSummaryDialog.cpp`
- Modify: `CMakeLists.txt` (add dialog sources/headers into app lists)

- [ ] **Step 1: Add dialog header**

```cpp
// src/ui/ExportTaskSummaryDialog.h
#pragma once

#include <QDialog>
#include <QList>
#include "models/Task.h"

class QListWidget;
class QPushButton;

class ExportTaskSummaryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExportTaskSummaryDialog(const QList<Task> &tasks, QWidget *parent = nullptr);
    QList<int> selectedTaskIds() const;

private:
    void updateExportButtonState();
    QListWidget *m_taskList = nullptr;
    QPushButton *m_exportButton = nullptr;
};
```

- [ ] **Step 2: Implement checkable list UI with all tasks checked**

```cpp
// src/ui/ExportTaskSummaryDialog.cpp (core setup)
m_taskList = new QListWidget(this);
m_taskList->setObjectName("exportTaskList");

for (const Task &task : tasks) {
    auto *item = new QListWidgetItem(
        QString("[%1] %2")
            .arg(Task::workStatusToString(task.workStatus), task.title),
        m_taskList);
    item->setData(Qt::UserRole, task.id);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Checked);
}
```

- [ ] **Step 3: Implement selected-id extraction and button enablement**

```cpp
QList<int> ExportTaskSummaryDialog::selectedTaskIds() const
{
    QList<int> ids;
    for (int i = 0; i < m_taskList->count(); ++i) {
        QListWidgetItem *item = m_taskList->item(i);
        if (item->checkState() == Qt::Checked) {
            ids.append(item->data(Qt::UserRole).toInt());
        }
    }
    return ids;
}
```

- [ ] **Step 4: Add dialog to app build lists**

```cmake
# CMakeLists.txt
set(SOURCES
    ...
    src/ui/ExportTaskSummaryDialog.cpp
)

set(HEADERS
    ...
    src/ui/ExportTaskSummaryDialog.h
)
```

- [ ] **Step 5: Run tests to confirm GREEN**

Run:

```powershell
cmake --build build-tests --config Release --target NexusTaskSummaryExportTests
ctest --test-dir build-tests -C Release --output-on-failure -R NexusTaskSummaryExportTests
```

Expected: all export tests pass.

- [ ] **Step 6: Commit dialog implementation**

```bash
git add src/ui/ExportTaskSummaryDialog.h src/ui/ExportTaskSummaryDialog.cpp CMakeLists.txt
git commit -m "feat: add export task selection dialog"
```

### Task 5: Wire File menu export flow in MainWindow

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Add slot declaration**

```cpp
// src/app/MainWindow.h
private slots:
    ...
    void onExportTaskSummaryRequested();
```

- [ ] **Step 2: Add File-menu action**

```cpp
// src/app/MainWindow.cpp inside setupMenuBar()
QAction *exportTaskSummaryAction = fileMenu->addAction("Export Task Summary (.md)...");
exportTaskSummaryAction->setShortcut(QKeySequence("Ctrl+Shift+E"));
connect(exportTaskSummaryAction, &QAction::triggered,
        this, &MainWindow::onExportTaskSummaryRequested);

fileMenu->addSeparator();
```

- [ ] **Step 3: Implement export slot prechecks and dialog flow**

```cpp
void MainWindow::onExportTaskSummaryRequested()
{
    const int productId = m_productPane->selectedProductId();
    auto &database = DatabaseManager::instance();

    const QList<Task> activeTasks = productId > 0
        ? database.getTasksForProduct(productId, TaskStatus::Active)
        : QList<Task>();

    if (productId <= 0) {
        QMessageBox::information(this, "Export Task Summary",
                                 "Please select a product first.");
        return;
    }
    if (activeTasks.isEmpty()) {
        QMessageBox::information(this, "Export Task Summary",
                                 "No active tasks are available to export.");
        return;
    }

    ExportTaskSummaryDialog dialog(activeTasks, this);
    if (dialog.exec() != QDialog::Accepted) {
        statusBar()->showMessage("Task summary export canceled.", 3000);
        return;
    }
```

- [ ] **Step 4: Implement save path + markdown write**

```cpp
    const QList<int> selectedIds = dialog.selectedTaskIds();
    const TaskSummaryExportIssue selectionCheck =
        validateTaskSummaryExportInputs(productId, activeTasks, selectedIds);
    if (selectionCheck == TaskSummaryExportIssue::NoTaskSelected) {
        QMessageBox::information(this, "Export Task Summary",
                                 "Please select at least one task to export.");
        return;
    }

    QList<Task> selectedTasks;
    QSet<int> selectedSet(selectedIds.begin(), selectedIds.end());
    for (const Task &task : activeTasks) {
        if (selectedSet.contains(task.id)) selectedTasks.append(task);
    }

    const Product product = database.getProduct(productId);
    const QDateTime now = QDateTime::currentDateTime();
    const QString markdown = buildTaskSummaryMarkdown(product.name, now, selectedTasks, 120);

    const QString defaultName =
        QString("task-summary-%1.md").arg(now.toString("yyyyMMdd-HHmm"));
    const QString filePath = QFileDialog::getSaveFileName(
        this, "Export Task Summary", defaultName, "Markdown Files (*.md)");
    if (filePath.isEmpty()) {
        statusBar()->showMessage("Task summary export canceled.", 3000);
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export Task Summary",
            QString("Failed to write file:\n%1\n\n%2")
                .arg(filePath, file.errorString()));
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << markdown;
    file.close();

    statusBar()->showMessage(
        QString("Exported %1 task(s) to %2").arg(selectedTasks.size(), filePath), 5000);
}
```

- [ ] **Step 5: Add required includes**

```cpp
// src/app/MainWindow.cpp
#include "ui/ExportTaskSummaryDialog.h"
#include "export/TaskSummaryMarkdownExporter.h"
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QSet>
```

- [ ] **Step 6: Build app and run export tests**

Run:

```powershell
cmake --build build-tests --config Release --target NexusTaskSummaryExportTests
ctest --test-dir build-tests -C Release --output-on-failure -R NexusTaskSummaryExportTests
cmake --build build --config Release --target Nexus
```

Expected: export tests pass and app target builds.

- [ ] **Step 7: Commit MainWindow wiring**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "feat: add file-menu markdown task summary export flow"
```

### Task 6: Update README and run final verification

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Document the new feature**

Add one bullet in `## Features`:

```md
- **Markdown Task Export** — Export selected active tasks from the current product to a Markdown file with work status, title, and short note summary.
```

- [ ] **Step 2: Run final targeted validation**

Run:

```powershell
ctest --test-dir build-tests -C Release --output-on-failure -R NexusTaskSummaryExportTests
ctest --test-dir build-tests -C Release --output-on-failure -R NexusTests
ctest --test-dir build-tests -C Release --output-on-failure -R NexusFreshDatabaseTests
```

Expected: all three targeted suites pass.

- [ ] **Step 3: Commit docs and final cleanups**

```bash
git add README.md
git commit -m "docs: describe markdown task export feature"
```

## Self-Review Checklist

- Spec coverage:
  - File menu entry point: covered in Task 5.
  - Current-product, active-task selection: covered in Tasks 4 and 5.
  - Markdown header + table (Status/Title/Summary): covered in Task 2 and tests in Tasks 1/2.
  - Local 120-char summary extraction: covered in Task 2 tests/implementation.
  - Save As path handling and write errors: covered in Task 5.
  - No schema changes: maintained by structure and task scope.
- Placeholder scan: no TODO/TBD placeholders remain.
- Type consistency: same helper names and enum names are used across tests, exporter, dialog, and MainWindow tasks.
