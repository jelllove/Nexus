#include "MainWindow.h"
#include "ui/SettingsDialog.h"
#include "services/AIService.h"
#include "services/UpdateService.h"
#include "services/TaskExportService.h"
#include "platform/GlobalHotkey.h"
#include "db/DatabaseManager.h"
#include "ui/ExportProgressDialog.h"
#include "ui/MarkdownPreviewDialog.h"
#include <QCloseEvent>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QStatusBar>
#include <QApplication>
#include <QDesktopServices>
#include <QProcess>
#include <QDialog>
#include <QDialogButtonBox>
#include <QRadioButton>
#include <QComboBox>
#include <QCheckBox>
#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QFile>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QEventLoop>
#include <QTimer>
#include <QPointer>
#include <QSignalBlocker>
#include <QBrush>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifndef APP_GIT_COMMIT
#define APP_GIT_COMMIT "unknown"
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    setupMenuBar();
    setupTrayIcon();
    setupGlobalHotkey();

    // Load initial data
    m_productPane->loadProducts();

    // Connect AI service
    connect(&AIService::instance(), &AIService::titleGenerated,
            this, &MainWindow::onTitleGenerated);
    connect(&AIService::instance(), &AIService::summaryGenerated,
            this, &MainWindow::onSummaryGenerated);
    connect(&AIService::instance(), &AIService::error,
            this, &MainWindow::onAIError);

    const QString buildCommit = QString::fromUtf8(APP_GIT_COMMIT);
    setWindowTitle(
        QString("Nexus - Task Manager (v%1 · %2)")
            .arg(qApp->applicationVersion(),
                 buildCommit.isEmpty() ? QStringLiteral("unknown") : buildCommit));
    resize(1400, 800);

    statusBar()->showMessage(
        QString("Ready · v%1 · %2")
            .arg(qApp->applicationVersion(),
                 buildCommit.isEmpty() ? QStringLiteral("unknown") : buildCommit));

    // Check for updates
    checkForUpdates();
}

MainWindow::~MainWindow()
{
    GlobalHotkey::instance().unregisterHotkey();
}

void MainWindow::setupUi()
{
    // Central widget
    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Search bar at top
    m_searchBar = new SearchBar(this);
    m_searchBar->setStyleSheet("background-color: #ecf0f1;");
    mainLayout->addWidget(m_searchBar);

    // 3-pane splitter
    m_splitter = new QSplitter(Qt::Horizontal, this);

    m_productPane = new ProductPane(this);
    m_taskPane = new TaskPane(this);
    m_editorPane = new EditorPane(this);

    m_splitter->addWidget(m_productPane);
    m_splitter->addWidget(m_taskPane);
    m_splitter->addWidget(m_editorPane);

    // Set initial sizes: Products(200) | Tasks(320) | Editor(rest)
    m_splitter->setSizes({200, 320, 880});
    m_splitter->setStretchFactor(0, 0);  // Products: fixed
    m_splitter->setStretchFactor(1, 0);  // Tasks: fixed
    m_splitter->setStretchFactor(2, 1);  // Editor: stretch

    mainLayout->addWidget(m_splitter, 1);

    setCentralWidget(centralWidget);

    // Connect signals
    connect(m_productPane, &ProductPane::productSelected,
            this, &MainWindow::onProductSelected);
    connect(m_taskPane, &TaskPane::taskSelected,
            this, &MainWindow::onTaskSelected);
    connect(m_taskPane, &TaskPane::subTaskSelected,
            this, &MainWindow::onSubTaskSelected);
    connect(m_searchBar, &SearchBar::searchRequested,
            this, &MainWindow::onSearchRequested);
    connect(m_searchBar, &SearchBar::searchCleared,
            this, &MainWindow::onSearchCleared);
    connect(m_editorPane, &EditorPane::generateTitleRequested,
            this, &MainWindow::onGenerateTitleRequested);
    connect(m_editorPane, &EditorPane::summarizeRequested,
            this, &MainWindow::onSummarizeRequested);
    connect(m_editorPane, &EditorPane::titleChanged,
            this, &MainWindow::onTaskTitleChanged);
    connect(m_editorPane, &EditorPane::autoGenerateTitleRequested,
            this, &MainWindow::onGenerateTitleRequested);
}

void MainWindow::setupMenuBar()
{
    QMenuBar *menuBar = this->menuBar();

    // File menu
    QMenu *fileMenu = menuBar->addMenu("&File");

    QAction *exportAction = fileMenu->addAction("Export &Tasks to Markdown...");
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportTasksToMarkdown);

    QAction *previewAction = fileMenu->addAction("Preview Selected Tasks as &Markdown...");
    connect(previewAction, &QAction::triggered, this, &MainWindow::previewTasksAsMarkdown);

    fileMenu->addSeparator();

    QAction *settingsAction = fileMenu->addAction("&Settings...");
    settingsAction->setShortcut(QKeySequence("Ctrl+,"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);

    fileMenu->addSeparator();

    QAction *quitAction = fileMenu->addAction("&Quit");
    quitAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    // View menu
    QMenu *viewMenu = menuBar->addMenu("&View");

    QAction *focusSearchAction = viewMenu->addAction("&Search");
    focusSearchAction->setShortcut(QKeySequence("Ctrl+F"));
    connect(focusSearchAction, &QAction::triggered, this, [this]() {
        m_searchBar->setFocus();
    });

    // Help menu
    QMenu *helpMenu = menuBar->addMenu("&Help");

    QAction *aboutAction = helpMenu->addAction("&About Nexus...");
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox aboutBox(this);
        aboutBox.setWindowTitle("About Nexus");
        aboutBox.setIconPixmap(QPixmap(":/icons/app-icon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        aboutBox.setTextFormat(Qt::RichText);
        const QString buildCommit = QString::fromUtf8(APP_GIT_COMMIT).trimmed().isEmpty()
            ? QStringLiteral("unknown")
            : QString::fromUtf8(APP_GIT_COMMIT).trimmed();
        aboutBox.setText(
            QString("<h2>Nexus v%1</h2>").arg(qApp->applicationVersion()) +
            "<p>Personal Task Management</p>" +
            QString("<p><b>Build Commit:</b> <code>%1</code></p>").arg(buildCommit.toHtmlEscaped()) +
            "<p>"
            "<b>GitHub:</b> <a href=\"https://github.com/jelllove/Nexus\">github.com/jelllove/Nexus</a><br>"
            "<b>Website:</b> <a href=\"https://www.jelllove.com\">www.jelllove.com</a><br>"
            "<b>Email:</b> <a href=\"mailto:jelllove@gmail.com\">jelllove@gmail.com</a>"
            "</p>"
        );
        aboutBox.exec();
    });

    QAction *checkUpdateAction = helpMenu->addAction("Check for &Updates...");
    connect(checkUpdateAction, &QAction::triggered, this, [this]() {
        statusBar()->showMessage("Checking for updates...");
        checkForUpdates();
    });
}

void MainWindow::setupTrayIcon()
{
    // Set app icon for window and tray
    QIcon appIcon(":/icons/app-icon.png");
    setWindowIcon(appIcon);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setToolTip("Nexus - Task Manager");
    m_trayIcon->setIcon(appIcon);

    // Tray context menu
    QMenu *trayMenu = new QMenu(this);

    QAction *showAction = trayMenu->addAction("Show/Hide");
    connect(showAction, &QAction::triggered, this, &MainWindow::toggleVisibility);

    trayMenu->addSeparator();

    QAction *quitAction = trayMenu->addAction("Quit");
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    m_trayIcon->setContextMenu(trayMenu);
    m_trayIcon->show();

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayActivated);
}

void MainWindow::setupGlobalHotkey()
{
#ifdef Q_OS_WIN
    // Default: Ctrl+Shift+N
    bool ok = GlobalHotkey::instance().registerHotkey(
        MOD_CONTROL | MOD_SHIFT, 'N');

    if (ok) {
        connect(&GlobalHotkey::instance(), &GlobalHotkey::hotkeyPressed,
                this, &MainWindow::onHotkeyPressed);
        statusBar()->showMessage("Global hotkey registered: Ctrl+Shift+N", 3000);
    } else {
        statusBar()->showMessage("Failed to register global hotkey", 3000);
    }
#endif
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Minimize to tray instead of closing
    if (m_trayIcon->isVisible()) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

void MainWindow::onProductSelected(int productId)
{
    m_searchActive = false;
    m_searchSnapshot = {};

    const QSignalBlocker blocker(m_searchBar);
    m_searchBar->clear();
    m_taskPane->loadTasks(productId);
    m_editorPane->clear();
}

void MainWindow::onTaskSelected(int taskId)
{
    m_editorPane->loadTask(taskId);
}

void MainWindow::onSubTaskSelected(int subTaskId)
{
    m_editorPane->loadSubTask(subTaskId);
}

void MainWindow::onSearchRequested(const QString &query)
{
    const QString trimmedQuery = query.trimmed();
    if (trimmedQuery.isEmpty()) {
        onSearchCleared();
        return;
    }

    if (!m_searchActive) {
        m_searchSnapshot.productId = m_productPane->selectedProductId();
        m_searchSnapshot.selectedTaskId = m_taskPane->selectedTaskId();
        m_searchSnapshot.mode = m_taskPane->viewMode();
        m_searchSnapshot.valid = true;
    }

    const auto results = DatabaseManager::instance().searchTasks(trimmedQuery);
    m_taskPane->loadSearchResults(results, trimmedQuery);
    m_searchActive = true;

    if (results.isEmpty()) {
        statusBar()->showMessage(QString("No results found for '%1'").arg(trimmedQuery), 3000);
    } else {
        statusBar()->showMessage(QString("Found %1 result(s)").arg(results.size()), 3000);
    }
}

void MainWindow::onSearchCleared()
{
    if (!m_searchActive || !m_searchSnapshot.valid) {
        const int productId = m_productPane->selectedProductId();
        if (productId > 0) {
            m_taskPane->loadTasks(productId);
        } else {
            m_taskPane->restoreView(-1, TaskPane::ViewMode::Active, -1);
        }
        return;
    }

    if (m_searchSnapshot.productId > 0) {
        m_productPane->selectProductById(m_searchSnapshot.productId, false);
    }

    m_taskPane->restoreView(
        m_searchSnapshot.productId,
        m_searchSnapshot.mode,
        m_searchSnapshot.selectedTaskId);

    m_searchActive = false;
    m_searchSnapshot = {};
    statusBar()->showMessage("Search cleared", 2000);
}

void MainWindow::onHotkeyPressed()
{
    toggleVisibility();
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger ||
        reason == QSystemTrayIcon::DoubleClick) {
        toggleVisibility();
    }
}

void MainWindow::toggleVisibility()
{
    if (isVisible() && !isMinimized()) {
        hide();
    } else {
        show();
        raise();
        activateWindow();
        if (isMinimized()) {
            showNormal();
        }
    }
}

void MainWindow::onTaskTitleChanged(int taskId, const QString &title)
{
    Q_UNUSED(taskId);
    Q_UNUSED(title);

    if (m_searchActive) {
        const QString query = m_searchBar->searchText();
        if (!query.isEmpty()) {
            onSearchRequested(query);
        }
        return;
    }

    // Refresh task list to show updated title
    int productId = m_productPane->selectedProductId();
    if (productId > 0) {
        m_taskPane->loadTasks(productId);
    }
}

void MainWindow::onGenerateTitleRequested(int taskId, const QString &content)
{
    m_currentTaskIdForTitle = taskId;
    statusBar()->showMessage("Generating title with AI...");
    AIService::instance().generateTitle(content);
}

void MainWindow::onTitleGenerated(const QString &title)
{
    if (m_currentTaskIdForTitle > 0) {
        DatabaseManager::instance().updateTaskTitle(m_currentTaskIdForTitle, title);
        statusBar()->showMessage(QString("Title updated: %1").arg(title), 5000);
        // Refresh task list
        if (m_searchActive) {
            const QString query = m_searchBar->searchText();
            if (!query.isEmpty()) {
                onSearchRequested(query);
            }
        } else {
            int productId = m_productPane->selectedProductId();
            if (productId > 0) {
                m_taskPane->loadTasks(productId);
            }
        }
        // Reload editor to show new title
        m_editorPane->loadTask(m_currentTaskIdForTitle);
    }
    m_editorPane->resetTitleGenerationPending();
    m_currentTaskIdForTitle = -1;
}

void MainWindow::onAIError(const QString &message)
{
    statusBar()->showMessage("AI Error: " + message, 5000);
    QMessageBox::warning(this, "AI Error", message);
    m_editorPane->resetTitleGenerationPending();
    m_currentTaskIdForTitle = -1;
    m_currentTaskIdForSummary = -1;
}

void MainWindow::onSummarizeRequested(int taskId, const QString &content)
{
    m_currentTaskIdForSummary = taskId;
    statusBar()->showMessage("Summarizing content with AI...");
    AIService::instance().summarizeContent(content);
}

void MainWindow::onSummaryGenerated(const QString &summary)
{
    if (m_currentTaskIdForSummary > 0) {
        // Append summary to existing content as a styled block
        Task task = DatabaseManager::instance().getTask(m_currentTaskIdForSummary);
        QString summaryHtml = QString(
            "<hr><blockquote><p><strong>AI Summary</strong></p><p>%1</p></blockquote>"
        ).arg(summary.toHtmlEscaped().replace("\n", "</p><p>"));

        QString newContent = task.content + summaryHtml;
        DatabaseManager::instance().updateTaskContent(m_currentTaskIdForSummary, newContent);
        m_editorPane->loadTask(m_currentTaskIdForSummary);
        statusBar()->showMessage("Summary added.", 5000);
    }
    m_currentTaskIdForSummary = -1;
}

void MainWindow::showSettings()
{
    SettingsDialog dialog(this);
    dialog.exec();
}

void MainWindow::exportTasksToMarkdown()
{
    PreparedMarkdownPayload payload;
    if (!prepareMarkdownPayload(payload, "Export Tasks")) {
        return;
    }

    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (defaultDir.isEmpty()) {
        defaultDir = QDir::homePath();
    }
    QString defaultPath = defaultDir + "/nexus_tasks_" +
                          QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".md";

    QString path = QFileDialog::getSaveFileName(
        this,
        "Export Tasks to Markdown",
        defaultPath,
        "Markdown Files (*.md)");
    if (path.isEmpty()) {
        statusBar()->showMessage("Export cancelled", 3000);
        return;
    }
    if (!path.endsWith(".md", Qt::CaseInsensitive)) {
        path += ".md";
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(
            this, "Export Failed",
            "Failed to write markdown file:\n" + file.errorString());
        statusBar()->showMessage("Export failed", 5000);
        return;
    }
    file.write(payload.markdown.toUtf8());
    file.close();

    QString message = QString("Exported %1 task(s) to:\n%2")
                          .arg(payload.items.size())
                          .arg(path);
    if (payload.includeDescription && payload.fallbackCount > 0) {
        message += QString(
            "\n\n%1 task description(s) used fallback text because AI was unavailable or failed.")
                       .arg(payload.fallbackCount);
    }

    QMessageBox::information(this, "Export Complete", message);
    statusBar()->showMessage(
        QString("Export complete: %1 task(s)").arg(payload.items.size()), 6000);
}

void MainWindow::previewTasksAsMarkdown()
{
    PreparedMarkdownPayload payload;
    if (!prepareMarkdownPayload(payload, "Preview Tasks")) {
        return;
    }

    if (!m_markdownPreviewDialog) {
        m_markdownPreviewDialog = new MarkdownPreviewDialog(this);
    }

    m_markdownPreviewDialog->setMarkdownContent(payload.markdown);
    m_markdownPreviewDialog->show();
    m_markdownPreviewDialog->raise();
    m_markdownPreviewDialog->activateWindow();

    QString status = QString("Preview ready: %1 task(s)").arg(payload.items.size());
    if (payload.includeDescription && payload.fallbackCount > 0) {
        status += QString(" (%1 fallback summary)").arg(payload.fallbackCount);
    }
    statusBar()->showMessage(status, 6000);
}

bool MainWindow::prepareMarkdownPayload(PreparedMarkdownPayload &payload, const QString &actionName)
{
    bool exportAllProducts = true;
    int selectedProductId = -1;
    bool includeDescription = false;

    if (!promptExportScopeDialog(exportAllProducts, selectedProductId, includeDescription)) {
        return false;
    }

    QHash<int, QString> productNames;
    QMap<int, QList<Task>> tasksByProduct = collectActiveTasksForExport(
        exportAllProducts, selectedProductId, productNames);

    if (tasksByProduct.isEmpty()) {
        QMessageBox::information(this, actionName,
                                 "No active tasks found in the selected scope.");
        return false;
    }

    QList<ExportTaskItem> selectedExportItems;
    if (!promptTaskSelectionDialog(tasksByProduct, productNames, selectedExportItems)) {
        return false;
    }

    if (selectedExportItems.isEmpty()) {
        QMessageBox::information(this, actionName,
                                 "Please select at least one task.");
        return false;
    }

    if (includeDescription) {
        const auto confirm = QMessageBox::question(
            this,
            "Confirm AI Summarization",
            "AI summarization is enabled. Nexus will call your configured AI endpoint "
            "to generate one-line summaries for selected tasks.\n\nContinue?",
            QMessageBox::Yes | QMessageBox::No);
        if (confirm != QMessageBox::Yes) {
            return false;
        }
    }

    const int summaryUnits = includeDescription ? selectedExportItems.size() : 0;
    const int totalUnits = qMax(2, summaryUnits + 2);
    int completedUnits = 0;
    const QString cancelStatusMessage = actionName.startsWith("Preview")
        ? "Preview cancelled"
        : "Export cancelled";

    ExportProgressDialog progressDialog(this);
    if (actionName.startsWith("Preview")) {
        progressDialog.setContextText("Preview Progress", "Preparing markdown preview. Please wait...");
        progressDialog.appendLog("Preview preparation started.");
    } else {
        progressDialog.setContextText("Export Progress", "Exporting tasks. Please wait...");
        progressDialog.appendLog("Export started.");
    }
    progressDialog.setProgress(0, totalUnits);
    progressDialog.show();

    auto setProgress = [&](int completed, const QString &message) {
        completedUnits = qBound(0, completed, totalUnits);
        progressDialog.setProgress(completedUnits, totalUnits);
        if (!message.trimmed().isEmpty()) {
            progressDialog.appendLog(message);
        }
    };

    if (progressDialog.isCancelled()) {
        statusBar()->showMessage(cancelStatusMessage, 5000);
        return false;
    }

    statusBar()->showMessage(actionName.startsWith("Preview")
                                 ? "Preparing markdown preview..."
                                 : "Exporting tasks...");
    int fallbackCount = 0;
    const bool summariesDone = resolveSimpleDescriptions(
        selectedExportItems, includeDescription, fallbackCount,
        [&](int completed, int total, const QString &message) {
            Q_UNUSED(total);
            setProgress(completed, message);
        },
        [&]() {
            return progressDialog.isCancelled();
        });

    if (!summariesDone) {
        progressDialog.appendLog("Cancelled before markdown generation.");
        statusBar()->showMessage(cancelStatusMessage, 5000);
        return false;
    }

    if (!includeDescription) {
        setProgress(0, "AI summarization disabled. Skipping summary stage.");
    }

    if (progressDialog.isCancelled()) {
        progressDialog.appendLog("Cancelled before markdown build.");
        statusBar()->showMessage(cancelStatusMessage, 5000);
        return false;
    }

    setProgress(summaryUnits + 1, "Building markdown content.");
    const QString markdown = TaskExportService::buildMarkdown(
        selectedExportItems, QDateTime::currentDateTime());

    if (progressDialog.isCancelled()) {
        progressDialog.appendLog("Cancelled after markdown build.");
        statusBar()->showMessage(cancelStatusMessage, 5000);
        return false;
    }

    setProgress(summaryUnits + 2, "Markdown content ready.");
    progressDialog.appendLog("Preparation completed successfully.");
    progressDialog.close();

    payload.items = selectedExportItems;
    payload.markdown = markdown;
    payload.includeDescription = includeDescription;
    payload.fallbackCount = fallbackCount;
    return true;
}

bool MainWindow::promptExportScopeDialog(bool &exportAllProducts,
                                         int &selectedProductId,
                                         bool &includeDescription)
{
    QDialog dialog(this);
    dialog.setWindowTitle("Export Tasks - Step 1/2");
    dialog.setMinimumWidth(520);

    auto *layout = new QVBoxLayout(&dialog);

    auto *hint = new QLabel(
        "Status is fixed to Active tasks.\n"
        "Choose your export scope and description options.",
        &dialog);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *allProductsRadio = new QRadioButton("All products (Active tasks only)", &dialog);
    auto *singleProductRadio = new QRadioButton("Single product (Active tasks only)", &dialog);
    allProductsRadio->setChecked(true);
    layout->addWidget(allProductsRadio);
    layout->addWidget(singleProductRadio);

    auto *productCombo = new QComboBox(&dialog);
    const auto products = DatabaseManager::instance().getAllProducts();
    for (const Product &product : products) {
        productCombo->addItem(product.name, product.id);
    }
    productCombo->setEnabled(false);
    connect(singleProductRadio, &QRadioButton::toggled, productCombo, &QWidget::setEnabled);
    layout->addWidget(productCombo);

    auto *includeDescriptionCheck =
        new QCheckBox("Include simple description (AI one-line summary)", &dialog);
    includeDescriptionCheck->setChecked(false);
    layout->addWidget(includeDescriptionCheck);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    exportAllProducts = allProductsRadio->isChecked();
    selectedProductId = productCombo->currentData().toInt();
    includeDescription = includeDescriptionCheck->isChecked();

    if (!exportAllProducts && selectedProductId <= 0) {
        QMessageBox::warning(this, "Export Tasks",
                             "Please choose a product for export.");
        return false;
    }

    return true;
}

QMap<int, QList<Task>> MainWindow::collectActiveTasksForExport(
    bool exportAllProducts, int selectedProductId, QHash<int, QString> &productNames) const
{
    QMap<int, QList<Task>> tasksByProduct;
    const auto products = DatabaseManager::instance().getAllProducts();

    for (const Product &product : products) {
        if (!exportAllProducts && product.id != selectedProductId) {
            continue;
        }

        const QList<Task> activeTasks = DatabaseManager::instance().getTasksForProduct(
            product.id, TaskStatus::Active);
        if (activeTasks.isEmpty()) {
            continue;
        }

        productNames.insert(product.id, product.name);
        tasksByProduct.insert(product.id, activeTasks);
    }

    return tasksByProduct;
}

bool MainWindow::promptTaskSelectionDialog(
    const QMap<int, QList<Task>> &tasksByProduct,
    const QHash<int, QString> &productNames,
    QList<ExportTaskItem> &selectedExportItems)
{
    enum NodeType {
        ProductNode = 1,
        MainTaskNode = 2,
        SubTaskNode = 3
    };

    constexpr int RoleNodeType = Qt::UserRole + 20;
    constexpr int RoleId = Qt::UserRole;
    constexpr int RoleProductId = Qt::UserRole + 1;
    constexpr int RoleTitle = Qt::UserRole + 2;
    constexpr int RoleSubtaskWorkStatus = Qt::UserRole + 3;
    constexpr int RoleWorkStatusText = Qt::UserRole + 4;
    constexpr int RoleWorkStatusIcon = Qt::UserRole + 5;
    constexpr int RoleSubtaskWorkStatusIcon = Qt::UserRole + 6;

    const auto priorityCodeFor = [](TaskPriority priority) -> QString {
        const QString priorityText = Task::priorityToString(priority);
        const int splitIndex = priorityText.indexOf(" - ");
        return splitIndex > 0 ? priorityText.left(splitIndex) : priorityText;
    };
    QDialog dialog(this);
    dialog.setWindowTitle("Export Tasks - Step 2/2");
    dialog.setMinimumSize(640, 500);

    auto *layout = new QVBoxLayout(&dialog);
    auto *hint = new QLabel(
        "Select what to export in a Product → Main Task → Sub Task tree.",
        &dialog);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *tree = new QTreeWidget(&dialog);
    tree->setHeaderLabels({"Tasks"});
    tree->setRootIsDecorated(true);
    tree->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(tree, 1);

    for (auto it = tasksByProduct.cbegin(); it != tasksByProduct.cend(); ++it) {
        const int productId = it.key();
        const QString productName = productNames.value(
            productId, QString("Product %1").arg(productId));
        auto *productItem = new QTreeWidgetItem(
            tree, {QString::fromUtf8("📚 ") + productName});
        productItem->setFlags((productItem->flags() | Qt::ItemIsUserCheckable) &
                              ~Qt::ItemIsSelectable);
        productItem->setCheckState(0, Qt::Unchecked);
        productItem->setData(0, RoleNodeType, ProductNode);
        productItem->setData(0, RoleProductId, productId);

        for (const Task &task : it.value()) {
            const QString title = task.title.trimmed().isEmpty() ? "Untitled Task" : task.title;
            const QString priorityCode = priorityCodeFor(task.priority);
            const QString priorityBadge = QString("[%1]").arg(priorityCode);
            auto *taskItem = new QTreeWidgetItem(
                productItem, {QString("%1 %2 %3")
                                  .arg(Task::workStatusIcon(task.workStatus), title, priorityBadge)});
            taskItem->setData(0, RoleNodeType, MainTaskNode);
            taskItem->setData(0, RoleId, task.id);
            taskItem->setData(0, RoleProductId, productId);
            taskItem->setData(0, RoleTitle, task.title);
            taskItem->setData(0, RoleWorkStatusText, Task::workStatusToString(task.workStatus));
            taskItem->setData(0, RoleWorkStatusIcon, Task::workStatusIcon(task.workStatus));
            taskItem->setToolTip(0, Task::priorityToString(task.priority));
            taskItem->setBackground(0, QBrush(Task::priorityBackgroundColor(task.priority)));
            taskItem->setCheckState(0, Qt::Unchecked);
            taskItem->setFlags((taskItem->flags() | Qt::ItemIsUserCheckable) &
                               ~Qt::ItemIsSelectable);

            const QList<SubTask> subtasks = DatabaseManager::instance().getSubtasks(task.id);
            for (const SubTask &subtask : subtasks) {
                const QString subTitle = subtask.title.trimmed().isEmpty()
                    ? QString("Untitled sub task")
                    : subtask.title;
                auto *subtaskItem = new QTreeWidgetItem(
                    taskItem,
                    {QString("%1 %2").arg(Task::workStatusIcon(subtask.workStatus), subTitle)});
                subtaskItem->setData(0, RoleNodeType, SubTaskNode);
                subtaskItem->setData(0, RoleId, subtask.id);
                subtaskItem->setData(0, RoleTitle, subtask.title);
                subtaskItem->setData(
                    0, RoleSubtaskWorkStatus, static_cast<int>(subtask.workStatus));
                subtaskItem->setData(
                    0, RoleSubtaskWorkStatusIcon, Task::workStatusIcon(subtask.workStatus));
                subtaskItem->setCheckState(0, Qt::Unchecked);
                subtaskItem->setFlags((subtaskItem->flags() | Qt::ItemIsUserCheckable) &
                                      ~Qt::ItemIsSelectable);
            }
        }
    }
    tree->setUniformRowHeights(false);
    tree->expandAll();
    QTimer::singleShot(0, tree, [tree]() {
        tree->expandAll();
        tree->viewport()->update();
    });

    const auto setChildrenCheckState = [](QTreeWidgetItem *node, Qt::CheckState state, const auto &self) -> void {
        for (int i = 0; i < node->childCount(); ++i) {
            QTreeWidgetItem *child = node->child(i);
            if (child->isDisabled()) {
                continue;
            }
            child->setCheckState(0, state);
            self(child, state, self);
        }
    };

    bool syncingCheckState = false;
    connect(tree, &QTreeWidget::itemChanged, tree,
            [&, tree](QTreeWidgetItem *item, int column) {
                Q_UNUSED(column);
                if (syncingCheckState) {
                    return;
                }

                syncingCheckState = true;
                const Qt::CheckState state = item->checkState(0);
                if (state != Qt::PartiallyChecked) {
                    setChildrenCheckState(item, state, setChildrenCheckState);
                }

                QTreeWidgetItem *parent = item->parent();
                while (parent) {
                    int checkedChildren = 0;
                    int partialChildren = 0;
                    int effectiveChildCount = 0;
                    for (int i = 0; i < parent->childCount(); ++i) {
                        QTreeWidgetItem *child = parent->child(i);
                        if (child->isDisabled()) {
                            continue;
                        }
                        ++effectiveChildCount;
                        const Qt::CheckState childState = child->checkState(0);
                        if (childState == Qt::Checked) {
                            checkedChildren++;
                        } else if (childState == Qt::PartiallyChecked) {
                            partialChildren++;
                        }
                    }

                    if (effectiveChildCount == 0) {
                        parent->setCheckState(0, Qt::Unchecked);
                    } else if (checkedChildren == effectiveChildCount) {
                        parent->setCheckState(0, Qt::Checked);
                    } else if (checkedChildren == 0 && partialChildren == 0) {
                        parent->setCheckState(0, Qt::Unchecked);
                    } else {
                        parent->setCheckState(0, Qt::PartiallyChecked);
                    }
                    parent = parent->parent();
                }
                syncingCheckState = false;
            });

    auto *actionRow = new QHBoxLayout();
    auto *selectAllBtn = new QPushButton("Select All", &dialog);
    auto *clearAllBtn = new QPushButton("Clear All", &dialog);
    actionRow->addWidget(selectAllBtn);
    actionRow->addWidget(clearAllBtn);
    actionRow->addStretch();
    layout->addLayout(actionRow);

    connect(selectAllBtn, &QPushButton::clicked, tree, [tree]() {
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            tree->topLevelItem(i)->setCheckState(0, Qt::Checked);
        }
        tree->viewport()->update();
    });
    connect(clearAllBtn, &QPushButton::clicked, tree, [tree]() {
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            tree->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
        }
        tree->viewport()->update();
    });

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    selectedExportItems.clear();
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        auto *productItem = tree->topLevelItem(i);
        const int productId = productItem->data(0, RoleProductId).toInt();
        const QString productName = productNames.value(productId);
        for (int j = 0; j < productItem->childCount(); ++j) {
            auto *taskItem = productItem->child(j);
            if (taskItem->data(0, RoleNodeType).toInt() != MainTaskNode) {
                continue;
            }

            const int taskId = taskItem->data(0, RoleId).toInt();
            const Qt::CheckState taskState = taskItem->checkState(0);
            QList<ExportSubTaskItem> selectedSubtasks;
            for (int k = 0; k < taskItem->childCount(); ++k) {
                auto *subtaskItem = taskItem->child(k);
                if (subtaskItem->data(0, RoleNodeType).toInt() != SubTaskNode) {
                    continue;
                }
                if (subtaskItem->checkState(0) != Qt::Checked) {
                    continue;
                }
                ExportSubTaskItem selectedSubtask;
                selectedSubtask.title = subtaskItem->data(0, RoleTitle).toString();
                selectedSubtask.workStatus = static_cast<TaskWorkStatus>(
                    subtaskItem->data(0, RoleSubtaskWorkStatus).toInt());
                selectedSubtask.workStatusIcon =
                    subtaskItem->data(0, RoleSubtaskWorkStatusIcon).toString().trimmed();
                if (selectedSubtask.workStatusIcon.isEmpty()) {
                    selectedSubtask.workStatusIcon = Task::workStatusIcon(selectedSubtask.workStatus);
                }
                selectedSubtasks.append(selectedSubtask);
            }

            const bool shouldExportTask = (taskState == Qt::Checked ||
                                           taskState == Qt::PartiallyChecked ||
                                           !selectedSubtasks.isEmpty());
            if (!shouldExportTask) {
                continue;
            }

            const QList<Task> productTasks = tasksByProduct.value(productId);
            for (const Task &task : productTasks) {
                if (task.id != taskId) {
                    continue;
                }

                ExportTaskItem exportItem;
                exportItem.productName = productNames.value(productId);
                exportItem.statusText = "Active";
                exportItem.title = task.title;
                exportItem.priority = task.priority;
                exportItem.contentHtml = task.content;
                exportItem.updatedAt = task.updatedAt;
                exportItem.workStatusText = taskItem->data(0, RoleWorkStatusText).toString();
                exportItem.workStatusIcon = taskItem->data(0, RoleWorkStatusIcon).toString();
                exportItem.subtasks = selectedSubtasks;

                selectedExportItems.append(exportItem);
                break;
            }
        }
    }

    return true;
}

bool MainWindow::resolveSimpleDescriptions(
    QList<ExportTaskItem> &items,
    bool includeDescription,
    int &fallbackCount,
    const std::function<void(int completed, int total, const QString &message)> &progressCallback,
    const std::function<bool()> &isCancelled) const
{
    fallbackCount = 0;
    const int total = items.size();

    if (!includeDescription) {
        for (ExportTaskItem &item : items) {
            item.simpleDescription.clear();
        }
        return true;
    }

    if (!AIService::instance().isConfigured()) {
        for (int i = 0; i < items.size(); ++i) {
            if (isCancelled && isCancelled()) {
                return false;
            }
            ExportTaskItem &item = items[i];
            item.simpleDescription = TaskExportService::fallbackSimpleDescription(
                item.contentHtml, 120);
            ++fallbackCount;
            if (progressCallback) {
                progressCallback(i + 1, total,
                                 QString("AI unavailable, used fallback summary (%1/%2).")
                                     .arg(i + 1)
                                     .arg(total));
            }
        }
        return true;
    }

    for (int i = 0; i < items.size(); ++i) {
        if (isCancelled && isCancelled()) {
            return false;
        }

        ExportTaskItem &item = items[i];
        struct SummaryState {
            bool done = false;
            bool success = false;
            QString summary;
        };

        auto state = std::make_shared<SummaryState>();
        QEventLoop loop;
        QPointer<QEventLoop> loopPtr(&loop);
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeoutTimer.start(30000);

        AIService::instance().summarizeOneLineForExport(
            item.contentHtml,
            [state, loopPtr](const QString &summaryText) {
                if (state->done) {
                    return;
                }
                state->done = true;
                state->summary = summaryText.trimmed();
                state->success = !state->summary.isEmpty();
                if (loopPtr) {
                    loopPtr->quit();
                }
            },
            [state, loopPtr](const QString &) {
                if (state->done) {
                    return;
                }
                state->done = true;
                state->success = false;
                if (loopPtr) {
                    loopPtr->quit();
                }
            });

        loop.exec();

        if (state->done && state->success) {
            item.simpleDescription = state->summary;
            if (progressCallback) {
                progressCallback(i + 1, total,
                                 QString("Generated AI summary (%1/%2).")
                                     .arg(i + 1)
                                     .arg(total));
            }
        } else {
            item.simpleDescription = TaskExportService::fallbackSimpleDescription(
                item.contentHtml, 120);
            ++fallbackCount;
            if (progressCallback) {
                progressCallback(i + 1, total,
                                 QString("AI summary failed, used fallback (%1/%2).")
                                     .arg(i + 1)
                                     .arg(total));
            }
        }

        if (isCancelled && isCancelled()) {
            return false;
        }
    }

    return true;
}

void MainWindow::checkForUpdates()
{
    QString checkUpdates = DatabaseManager::instance().getSetting("check_updates", "true");
    if (checkUpdates != "true") return;

    auto &updater = UpdateService::instance();

    // Disconnect previous connections to avoid duplicates
    disconnect(&updater, &UpdateService::updateAvailable, this, nullptr);
    disconnect(&updater, &UpdateService::downloadProgress, this, nullptr);
    disconnect(&updater, &UpdateService::downloadFinished, this, nullptr);
    disconnect(&updater, &UpdateService::error, this, nullptr);

    connect(&updater, &UpdateService::updateAvailable,
            this, &MainWindow::onUpdateAvailable);
    connect(&updater, &UpdateService::downloadProgress,
            this, &MainWindow::onDownloadProgress);
    connect(&updater, &UpdateService::downloadFinished,
            this, &MainWindow::onDownloadFinished);
    connect(&updater, &UpdateService::error,
            this, [this](const QString &msg) {
                statusBar()->showMessage("Update: " + msg, 5000);
            });

    updater.checkForUpdate();
}

void MainWindow::onUpdateAvailable(const QString &latestVersion,
                                    const QString &downloadUrl,
                                    const QString &releaseNotes)
{
    // Truncate release notes for display
    QString notes = releaseNotes.left(500);
    if (releaseNotes.length() > 500) notes += "...";

    int ret = QMessageBox::question(this, "Update Available",
        QString("A new version of Nexus is available!\n\n"
                "Current version: v%1\n"
                "Latest version: %2\n\n"
                "%3\n\n"
                "Would you like to download and install the update?")
            .arg(qApp->applicationVersion(), latestVersion, notes),
        QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        statusBar()->showMessage("Downloading update...");
        UpdateService::instance().downloadAndInstall(downloadUrl);
    }
}

void MainWindow::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        int percent = static_cast<int>(bytesReceived * 100 / bytesTotal);
        statusBar()->showMessage(QString("Downloading update... %1%").arg(percent));
    }
}

void MainWindow::onDownloadFinished(const QString &installerPath)
{
    statusBar()->showMessage("Download complete. Launching installer...");

    // Launch the installer and quit
    bool started = QProcess::startDetached(installerPath, QStringList());
    if (started) {
        qApp->quit();
    } else {
        QMessageBox::warning(this, "Update Error",
            "Failed to launch the installer.\n"
            "The installer was saved to:\n" + installerPath);
    }
}
