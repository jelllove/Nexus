# Active Task Markdown Export Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a two-step export flow that exports user-selected Active main tasks (with their sub tasks) into an emoji-rich Markdown file, with optional AI one-line descriptions and a fallback mode when AI is not configured.

**Architecture:** Keep UI orchestration in `MainWindow`, keep markdown rendering/fallback text extraction in a dedicated export service, and add an export-specific AI API in `AIService` to avoid interfering with existing editor summary behavior. Data is read via existing `DatabaseManager` APIs and exported from an in-memory selection model.

**Tech Stack:** C++17, Qt6 Widgets, Qt Network, Qt SQL, existing `AIService` + `DatabaseManager`.

---

## Scope Check

This is a single subsystem (task export) and is scoped for one implementation plan.

## File Structure (planned changes)

- **Modify:** `CMakeLists.txt`
  - Register new export service source/header.
- **Create:** `src/services/TaskExportService.h`
  - Export DTOs and service method declarations (markdown generation + plain text fallback helpers).
- **Create:** `src/services/TaskExportService.cpp`
  - Markdown renderer and text normalization/fallback implementation.
- **Modify:** `src/services/AIService.h`
  - Add export-only one-sentence summary API + AI-config check API.
- **Modify:** `src/services/AIService.cpp`
  - Implement export-only API via existing request pipeline without reusing editor summary signal.
- **Modify:** `src/app/MainWindow.h`
  - Add export action slot and private helper methods/structs for step dialogs + orchestration.
- **Modify:** `src/app/MainWindow.cpp`
  - Add File menu export action, step-1/step-2 dialogs, data collection, optional AI summary pass, markdown write.
- **Modify:** `README.md`
  - Add feature bullet for Markdown export.

---

### Task 1: Add export service scaffolding and build wiring

**Files:**
- Create: `src/services/TaskExportService.h`
- Create: `src/services/TaskExportService.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add `TaskExportService.h` with export data contracts**

```cpp
#pragma once

#include <QString>
#include <QDateTime>
#include <QList>

struct ExportSubTaskItem {
    QString title;
    bool completed = false;
};

struct ExportTaskItem {
    QString productName;
    QString statusText;
    QString title;
    QString contentHtml;
    QDateTime updatedAt;
    QList<ExportSubTaskItem> subtasks;
    QString simpleDescription;
};

class TaskExportService
{
public:
    static QString extractPlainText(const QString &html);
    static QString fallbackSimpleDescription(const QString &html, int maxChars = 120);
    static QString buildMarkdown(const QList<ExportTaskItem> &tasks, const QDateTime &exportedAt);
};
```

- [ ] **Step 2: Add `TaskExportService.cpp` with compilable stubs**

```cpp
#include "TaskExportService.h"
#include <QRegularExpression>

QString TaskExportService::extractPlainText(const QString &html) { return html; }
QString TaskExportService::fallbackSimpleDescription(const QString &html, int maxChars) { return html.left(maxChars); }
QString TaskExportService::buildMarkdown(const QList<ExportTaskItem> &tasks, const QDateTime &exportedAt) { return QString(); }
```

- [ ] **Step 3: Wire new files in `CMakeLists.txt`**

```cmake
set(SOURCES
    src/services/UpdateService.cpp
    src/services/TaskExportService.cpp
    src/platform/GlobalHotkey.cpp
)

set(HEADERS
    src/services/UpdateService.h
    src/services/TaskExportService.h
    src/platform/GlobalHotkey.h
)
```

- [ ] **Step 4: Build-check compile integration**

Run: `cmake --build build --config Release --target Nexus`  
Expected: Build succeeds, no missing symbol/header errors for `TaskExportService`.

- [ ] **Step 5: Commit scaffolding**

```bash
git add CMakeLists.txt src/services/TaskExportService.h src/services/TaskExportService.cpp
git commit -m "feat: scaffold task markdown export service"
```

---

### Task 2: Implement markdown rendering and fallback text extraction

**Files:**
- Modify: `src/services/TaskExportService.cpp`
- Test: Manual preview by exporting sample string during runtime validation

- [ ] **Step 1: Implement HTML-to-plain-text normalization**

```cpp
QString TaskExportService::extractPlainText(const QString &html)
{
    QString text = html;
    text.remove(QRegularExpression("<[^>]*>"));
    text.replace("&nbsp;", " ");
    text.replace("&amp;", "&");
    text.replace("&lt;", "<");
    text.replace("&gt;", ">");
    return text.simplified();
}
```

- [ ] **Step 2: Implement fallback simple description (120-char behavior)**

```cpp
QString TaskExportService::fallbackSimpleDescription(const QString &html, int maxChars)
{
    const QString plain = extractPlainText(html);
    if (plain.isEmpty()) return QString::fromUtf8("📝 (empty)");
    return plain.length() <= maxChars ? plain : plain.left(maxChars) + "...";
}
```

- [ ] **Step 3: Implement emoji-rich markdown template**

```cpp
QMap<QString, QList<ExportTaskItem>> grouped;
int subTaskCount = 0;
for (const ExportTaskItem &item : tasks) {
    grouped[item.productName].append(item);
    subTaskCount += item.subtasks.size();
}

QString markdown;
markdown += "# 📦 Task Export\n\n";
markdown += QString("- 🕒 Exported at: %1\n").arg(exportedAt.toString("yyyy-MM-dd HH:mm:ss"));
markdown += QString("- 📊 Main tasks: %1\n").arg(tasks.size());
markdown += QString("- 🧩 Sub tasks: %1\n\n").arg(subTaskCount);

const QStringList productNames = grouped.keys();
for (const QString &productName : productNames) {
    markdown += QString("## 📚 %1\n\n").arg(productName);
    for (const ExportTaskItem &item : grouped[productName]) {
        markdown += QString("### 🟢 %1\n\n").arg(item.title.isEmpty() ? QString::fromUtf8("📝 Untitled Task") : item.title);
        markdown += "| Field | Value |\n|---|---|\n";
        markdown += QString("| Status | %1 |\n").arg(item.statusText);
        markdown += QString("| Title | %1 |\n").arg(item.title);
        markdown += QString("| Updated | %1 |\n\n").arg(item.updatedAt.isValid() ? item.updatedAt.toString("yyyy-MM-dd HH:mm") : "N/A");

        if (!item.simpleDescription.trimmed().isEmpty()) {
            markdown += QString("> 🧠 %1\n\n").arg(item.simpleDescription);
        }

        if (!item.subtasks.isEmpty()) {
            markdown += "#### 🪜 Sub Tasks\n";
            for (const ExportSubTaskItem &st : item.subtasks) {
                markdown += QString("- [%1] %2 %3\n")
                    .arg(st.completed ? "x" : " ")
                    .arg(st.completed ? "✅" : "⬜")
                    .arg(st.title);
            }
            markdown += "\n";
        } else {
            markdown += "- 💤 No sub tasks\n\n";
        }
        markdown += "---\n\n";
    }
}
return markdown;
```

- [ ] **Step 4: Add deterministic ordering in renderer**

```cpp
const QStringList productNames = grouped.keys();
for (const QString &productName : productNames) {
    const QList<ExportTaskItem> productTasks = grouped.value(productName);
    // iterate productTasks in insertion order to match step-2 user selection order
}
```

- [ ] **Step 5: Rebuild to validate formatter compilation**

Run: `cmake --build build --config Release --target Nexus`  
Expected: Build succeeds; no regex/QString formatting errors.

- [ ] **Step 6: Commit markdown renderer**

```bash
git add src/services/TaskExportService.cpp
git commit -m "feat: implement markdown export renderer with emoji formatting"
```

---

### Task 3: Extend AIService with export-safe one-line summarization

**Files:**
- Modify: `src/services/AIService.h`
- Modify: `src/services/AIService.cpp`

- [ ] **Step 1: Extend API declarations in `AIService.h`**

```cpp
void sendChatRequest(const QString &systemPrompt, const QString &userPrompt,
                     int maxTokens, double temperature,
                     std::function<void(const QString &)> onSuccess,
                     std::function<void(const QString &)> onFailure = {});

bool isConfigured() const;
void summarizeOneLineForExport(
    const QString &content,
    std::function<void(const QString &summary)> onSuccess,
    std::function<void(const QString &errorMessage)> onFailure);
```

- [ ] **Step 2: Update `sendChatRequest` implementation to support explicit failure callback**

```cpp
void AIService::sendChatRequest(const QString &systemPrompt, const QString &userPrompt,
                                int maxTokens, double temperature,
                                std::function<void(const QString &)> onSuccess,
                                std::function<void(const QString &)> onFailure)
{
    auto fail = [this, &onFailure](const QString &msg) {
        if (onFailure) onFailure(msg);
        else emit error(msg);
    };

    auto &db = DatabaseManager::instance();
    QString endpoint = db.getSetting("ai_endpoint");
    QString apiKey = db.getSetting("ai_api_key");
    QString model = db.getSetting("ai_model", "gpt-4o-mini");
    if (endpoint.isEmpty() || apiKey.isEmpty()) {
        fail("AI endpoint or API key not configured. Please check Settings.");
        return;
    }

    QJsonArray messages;
    if (!systemPrompt.isEmpty()) {
        QJsonObject sysMsg;
        sysMsg["role"] = "system";
        sysMsg["content"] = systemPrompt;
        messages.append(sysMsg);
    }
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userPrompt;
    messages.append(userMsg);

    QJsonObject requestBody;
    requestBody["model"] = model;
    requestBody["messages"] = messages;
    requestBody["max_tokens"] = maxTokens;
    requestBody["temperature"] = temperature;

    QNetworkRequest request = buildRequest(endpoint, apiKey);
    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(requestBody).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, onSuccess, fail]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            fail(QString("AI request failed: %1").arg(reply->errorString()));
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray choices = doc.object()["choices"].toArray();
        if (!choices.isEmpty()) {
            const QString result = choices[0].toObject()["message"].toObject()["content"].toString().trimmed();
            if (!result.isEmpty()) {
                onSuccess(result);
                return;
            }
        }
        fail("Failed to parse AI response.");
    });
}
```

- [ ] **Step 3: Implement `isConfigured()` using existing DB settings**

```cpp
bool AIService::isConfigured() const
{
    auto &db = DatabaseManager::instance();
    return !db.getSetting("ai_endpoint").trimmed().isEmpty()
        && !db.getSetting("ai_api_key").trimmed().isEmpty();
}
```

- [ ] **Step 4: Implement export-specific summarize method without emitting `summaryGenerated`**

```cpp
void AIService::summarizeOneLineForExport(
    const QString &content,
    std::function<void(const QString &summary)> onSuccess,
    std::function<void(const QString &errorMessage)> onFailure)
{
    QString plain = content;
    plain.remove(QRegularExpression("<[^>]*>"));
    plain = plain.trimmed();
    if (plain.isEmpty()) {
        onSuccess(QString::fromUtf8("📝 (empty)"));
        return;
    }

    if (plain.length() > 4000) {
        plain = plain.left(4000) + "...";
    }

    sendChatRequest(
        "Summarize the user task in exactly one concise sentence in the same language. "
        "Return one sentence only, plain text.",
        plain,
        120,
        0.2,
        [onSuccess](const QString &result) {
            onSuccess(result.simplified());
        },
        [onFailure](const QString &msg) {
            onFailure(msg);
        }
    );
}
```

- [ ] **Step 5: Ensure existing title/summary flows still use default error signal path**

```cpp
// generateTitle and summarizeContent continue to call sendChatRequest with only onSuccess,
// so UI-visible AI errors still flow through emit error(...) exactly as before.
```

- [ ] **Step 6: Build-check AI API changes**

Run: `cmake --build build --config Release --target Nexus`  
Expected: Build succeeds; `AIService` signatures compile in all call sites.

- [ ] **Step 7: Commit AI extension**

```bash
git add src/services/AIService.h src/services/AIService.cpp
git commit -m "feat: add export-specific one-line summary API in AI service"
```

---

### Task 4: Add export UI flow in MainWindow (menu + step dialogs + selection)

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Add new slot + helper structs in `MainWindow.h`**

```cpp
private slots:
    void exportTasksToMarkdown();

private:
    struct ExportScopeConfig {
        bool allProducts = true;
        int productId = -1;
        bool includeDescription = false;
    };

    struct SelectedMainTask {
        QString productName;
        Task task;
    };

    bool promptExportScope(ExportScopeConfig &config);
    bool promptTaskSelection(const QMap<int, QList<Task>> &tasksByProduct,
                             const QHash<int, QString> &productNames,
                             QList<SelectedMainTask> &selectedTasks);
```

- [ ] **Step 2: Add File menu action in `setupMenuBar()`**

```cpp
QAction *exportAction = fileMenu->addAction("Export &Tasks to Markdown...");
connect(exportAction, &QAction::triggered, this, &MainWindow::exportTasksToMarkdown);
fileMenu->addSeparator();
```

- [ ] **Step 3: Implement Step 1 dialog (scope + include-description toggle)**

```cpp
bool MainWindow::promptExportScope(ExportScopeConfig &config)
{
    QDialog dialog(this);
    dialog.setWindowTitle("Export Tasks - Step 1/2");
    auto *layout = new QVBoxLayout(&dialog);
    auto *allProducts = new QRadioButton("All products (Active only)", &dialog);
    auto *singleProduct = new QRadioButton("Single product (Active only)", &dialog);
    auto *productCombo = new QComboBox(&dialog);
    auto *includeDescription = new QCheckBox("Include simple description", &dialog);
    allProducts->setChecked(true);

    const auto products = DatabaseManager::instance().getAllProducts();
    for (const Product &p : products) {
        productCombo->addItem(p.name, p.id);
    }
    productCombo->setEnabled(false);
    connect(singleProduct, &QRadioButton::toggled, productCombo, &QWidget::setEnabled);

    layout->addWidget(allProducts);
    layout->addWidget(singleProduct);
    layout->addWidget(productCombo);
    layout->addWidget(includeDescription);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) return false;
    config.allProducts = allProducts->isChecked();
    config.productId = productCombo->currentData().toInt();
    config.includeDescription = includeDescription->isChecked();
    return true;
}
```

- [ ] **Step 4: Implement Step 2 dialog (task checklist by product)**

```cpp
bool MainWindow::promptTaskSelection(const QMap<int, QList<Task>> &tasksByProduct,
                                     const QHash<int, QString> &productNames,
                                     QList<SelectedMainTask> &selectedTasks)
{
    QDialog dialog(this);
    dialog.setWindowTitle("Export Tasks - Step 2/2");
    auto *layout = new QVBoxLayout(&dialog);
    auto *tree = new QTreeWidget(&dialog);
    tree->setHeaderLabels({"Task"});
    tree->setSelectionMode(QAbstractItemView::NoSelection);

    for (auto it = tasksByProduct.cbegin(); it != tasksByProduct.cend(); ++it) {
        auto *productItem = new QTreeWidgetItem(tree, {QString::fromUtf8("📚 ") + productNames.value(it.key())});
        productItem->setFlags(productItem->flags() & ~Qt::ItemIsSelectable);
        for (const Task &task : it.value()) {
            auto *taskItem = new QTreeWidgetItem(productItem, {task.title});
            taskItem->setData(0, Qt::UserRole, task.id);
            taskItem->setData(0, Qt::UserRole + 1, it.key());
            taskItem->setCheckState(0, Qt::Unchecked);
            taskItem->setFlags(taskItem->flags() | Qt::ItemIsUserCheckable);
        }
    }
    tree->expandAll();
    layout->addWidget(tree);

    auto *actions = new QHBoxLayout();
    auto *selectAll = new QPushButton("Select All", &dialog);
    auto *clearAll = new QPushButton("Clear All", &dialog);
    actions->addWidget(selectAll);
    actions->addWidget(clearAll);
    layout->addLayout(actions);

    connect(selectAll, &QPushButton::clicked, tree, [tree]() {
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            auto *productItem = tree->topLevelItem(i);
            for (int j = 0; j < productItem->childCount(); ++j) {
                productItem->child(j)->setCheckState(0, Qt::Checked);
            }
        }
    });
    connect(clearAll, &QPushButton::clicked, tree, [tree]() {
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            auto *productItem = tree->topLevelItem(i);
            for (int j = 0; j < productItem->childCount(); ++j) {
                productItem->child(j)->setCheckState(0, Qt::Unchecked);
            }
        }
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return false;

    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        auto *productItem = tree->topLevelItem(i);
        for (int j = 0; j < productItem->childCount(); ++j) {
            auto *taskItem = productItem->child(j);
            if (taskItem->checkState(0) != Qt::Checked) continue;
            const int taskId = taskItem->data(0, Qt::UserRole).toInt();
            const int productId = taskItem->data(0, Qt::UserRole + 1).toInt();
            const auto tasks = tasksByProduct.value(productId);
            for (const Task &task : tasks) {
                if (task.id == taskId) {
                    selectedTasks.append({productNames.value(productId), task});
                    break;
                }
            }
        }
    }
    return true;
}
```

- [ ] **Step 5: Validate UI build**

Run: `cmake --build build --config Release --target Nexus`  
Expected: Build succeeds and app launches with new File menu action.

- [ ] **Step 6: Commit UI wiring**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "feat: add two-step task export UI flow"
```

---

### Task 5: Implement export orchestration (data loading, optional AI summaries, markdown file write)

**Files:**
- Modify: `src/app/MainWindow.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/services/TaskExportService.cpp`

- [ ] **Step 1: Collect active tasks by scope using existing DatabaseManager APIs**

```cpp
QMap<int, QList<Task>> tasksByProduct;
QHash<int, QString> productNames;
const auto products = DatabaseManager::instance().getAllProducts();
for (const Product &p : products) {
    if (!scope.allProducts && p.id != scope.productId) continue;
    const auto tasks = DatabaseManager::instance().getTasksForProduct(p.id, TaskStatus::Active);
    if (!tasks.isEmpty()) {
        tasksByProduct.insert(p.id, tasks);
        productNames.insert(p.id, p.name);
    }
}
if (tasksByProduct.isEmpty()) {
    QMessageBox::information(this, "Export Tasks", "No active tasks found in the selected scope.");
    return;
}
```

- [ ] **Step 2: Build selected export DTO list including sub tasks**

```cpp
QList<ExportTaskItem> exportItems;
for (const SelectedMainTask &selected : selectedTasks) {
    ExportTaskItem item;
    item.productName = selected.productName;
    item.statusText = "Active";
    item.title = selected.task.title;
    item.contentHtml = selected.task.content;
    item.updatedAt = selected.task.updatedAt;
    for (const SubTask &st : DatabaseManager::instance().getSubtasks(selected.task.id)) {
        item.subtasks.append({st.title, st.completed});
    }
    exportItems.append(item);
}
```

- [ ] **Step 3: Resolve simple descriptions (AI first, fallback second)**

```cpp
int fallbackCount = 0;
if (!scope.includeDescription) {
    for (auto &item : exportItems) {
        item.simpleDescription.clear();
    }
} else if (!AIService::instance().isConfigured()) {
    for (auto &item : exportItems) {
        item.simpleDescription = TaskExportService::fallbackSimpleDescription(item.contentHtml, 120);
        fallbackCount++;
    }
} else {
    for (auto &item : exportItems) {
        bool done = false;
        QString generatedSummary;
        QEventLoop loop;
        AIService::instance().summarizeOneLineForExport(
            item.contentHtml,
            [&](const QString &summaryText) {
                done = true;
                generatedSummary = summaryText.trimmed();
                loop.quit();
            },
            [&](const QString &) {
                done = false;
                loop.quit();
            }
        );
        loop.exec();
        if (done && !generatedSummary.isEmpty()) {
            item.simpleDescription = generatedSummary;
        } else {
            item.simpleDescription = TaskExportService::fallbackSimpleDescription(item.contentHtml, 120);
            fallbackCount++;
        }
    }
}
```

- [ ] **Step 4: Generate markdown and write selected path**

```cpp
const QString markdown = TaskExportService::buildMarkdown(exportItems, QDateTime::currentDateTime());
QFile file(path);
if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, "Export Failed", file.errorString());
    return;
}
file.write(markdown.toUtf8());
file.close();
```

- [ ] **Step 5: User feedback for success + partial AI fallback warnings**

```cpp
statusBar()->showMessage(QString("Exported %1 tasks to %2").arg(exportItems.size()).arg(path), 6000);
if (fallbackCount > 0) {
    QMessageBox::information(this, "Export Tasks",
        QString("Export finished. %1 task descriptions used fallback text because AI was unavailable or summary failed.")
            .arg(fallbackCount));
}
```

- [ ] **Step 6: Build-check full integration**

Run: `cmake --build build --config Release --target Nexus`  
Expected: Build succeeds; export action compiles with all helper/service calls.

- [ ] **Step 7: Commit export orchestration**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp src/services/TaskExportService.cpp
git commit -m "feat: export active tasks to markdown with optional AI descriptions"
```

---

### Task 6: Validate behavior end-to-end and update docs

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Update README feature list**

```md
- **Markdown Export** — Export selected active tasks (with subtasks) to an emoji-rich Markdown file, with optional AI-generated one-line descriptions.
```

- [ ] **Step 2: Build before manual verification**

Run: `cmake --build build --config Release --target Nexus`  
Expected: Build succeeds.

- [ ] **Step 3: Manual scenario 1 (all products, no description)**

Run app flow:
1. File -> Export Tasks to Markdown
2. Scope = all products
3. Include description = OFF
4. Select subset of main tasks
5. Save `.md`

Expected:
- file is created
- only Active selected tasks included
- subtasks included
- markdown contains emoji header/table/checklist/separators

- [ ] **Step 4: Manual scenario 2 (single product, AI not configured)**

Expected:
- description lines use fallback 120-char text
- export still succeeds

- [ ] **Step 5: Manual scenario 3 (single product, AI configured)**

Expected:
- description lines are one-sentence summaries
- no content mutation in editor task body

- [ ] **Step 6: Commit docs + polish**

```bash
git add README.md
git commit -m "docs: document markdown task export feature"
```

---

### Task 7: Final verification and squash-ready handoff commit

**Files:**
- Modify: none (verification only unless bugs found)

- [ ] **Step 1: Run final build verification**

Run: `cmake --build build --config Release --target Nexus`  
Expected: PASS

- [ ] **Step 2: Run git diff sanity review**

Run: `git --no-pager diff --stat`  
Expected: Changes limited to planned files only.

- [ ] **Step 3: Run user-facing smoke test**

Expected:
- New menu action exists
- Export wizard works in two steps
- Markdown output matches agreed style

- [ ] **Step 4: Create final integration commit (if not already grouped)**

```bash
git add CMakeLists.txt src/app/MainWindow.h src/app/MainWindow.cpp src/services/AIService.h src/services/AIService.cpp src/services/TaskExportService.h src/services/TaskExportService.cpp README.md
git commit -m "feat: add markdown export for active tasks with optional AI summaries"
```
