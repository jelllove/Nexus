#include "MainWindow.h"
#include "ui/SettingsDialog.h"
#include "ui/ExportTaskSummaryDialog.h"
#include "services/AIService.h"
#include "services/UpdateService.h"
#include "platform/GlobalHotkey.h"
#include "db/DatabaseManager.h"
#include "export/TaskSummaryMarkdownExporter.h"
#include <QCloseEvent>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QFile>
#include <QSet>
#include <QStringConverter>
#include <QTextStream>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QStatusBar>
#include <QApplication>
#include <QDesktopServices>
#include <QProcess>

#ifdef Q_OS_WIN
#include <windows.h>
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

    setWindowTitle("Nexus - Task Manager");
    resize(1400, 800);

    statusBar()->showMessage("Ready");

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
    connect(m_taskPane, &TaskPane::itemSelected,
            this, &MainWindow::onItemSelected);
    connect(m_searchBar, &SearchBar::searchRequested,
            this, &MainWindow::onSearchRequested);
    connect(m_searchBar, &SearchBar::searchCleared,
            this, &MainWindow::onSearchCleared);
    connect(m_editorPane, &EditorPane::generateTitleRequested,
            this, &MainWindow::onGenerateTitleRequested);
    connect(m_editorPane, &EditorPane::summarizeRequested,
            this, &MainWindow::onSummarizeRequested);
    connect(m_editorPane, &EditorPane::titleChanged,
            this, &MainWindow::onItemTitleChanged);
    connect(m_editorPane, &EditorPane::autoGenerateTitleRequested,
            this, &MainWindow::onGenerateTitleRequested);
    connect(m_editorPane, &EditorPane::saveFailed, this, [this](const QString &message) {
        statusBar()->showMessage(message, 5000);
        QMessageBox::warning(this, "Nexus", message);
    });
}

void MainWindow::setupMenuBar()
{
    QMenuBar *menuBar = this->menuBar();

    // File menu
    QMenu *fileMenu = menuBar->addMenu("&File");

    QAction *settingsAction = fileMenu->addAction("&Settings...");
    settingsAction->setShortcut(QKeySequence("Ctrl+,"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);

    QAction *exportTaskSummaryAction = fileMenu->addAction("Export Task Summary (.md)...");
    exportTaskSummaryAction->setShortcut(QKeySequence("Ctrl+Shift+E"));
    connect(exportTaskSummaryAction, &QAction::triggered,
            this, &MainWindow::onExportTaskSummaryRequested);

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
        aboutBox.setText(
            QString("<h2>Nexus v%1</h2>").arg(qApp->applicationVersion()) +
            "<p>Personal Task Management</p>"
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
    const int previousProductId = m_taskPane->currentProductId();
    if (!m_editorPane->loadItem(EditorTarget())) {
        m_productPane->setSelectedProduct(previousProductId);
        return;
    }

    m_searchBar->clear();
    m_taskPane->loadTasks(productId);
    m_taskPane->setActiveTarget(EditorTarget());
}

void MainWindow::onItemSelected(const EditorTarget &target)
{
    const EditorTarget previousTarget = m_editorPane->currentTarget();
    if (m_editorPane->loadItem(target)) {
        m_taskPane->setActiveTarget(target);
    } else {
        m_taskPane->setActiveTarget(previousTarget);
    }
}

void MainWindow::onSearchRequested(const QString &query)
{
    const QString previousQuery = m_taskPane->currentSearchQuery();
    const QList<SearchResult> results = DatabaseManager::instance().searchItems(query);
    const EditorTarget currentTarget = m_editorPane->currentTarget();

    bool currentTargetVisible = false;
    for (const SearchResult &result : results) {
        if (result.target() == currentTarget) {
            currentTargetVisible = true;
            break;
        }
        if (currentTarget.kind == EditorTargetKind::Task
            && result.parentTask.id == currentTarget.id) {
            currentTargetVisible = true;
            break;
        }
    }

    if (!currentTargetVisible && currentTarget.isValid()
        && !m_editorPane->loadItem(EditorTarget())) {
        m_searchBar->setSearchText(previousQuery);
        return;
    }

    m_taskPane->showSearchResults(query, results);
    if (results.isEmpty()) {
        statusBar()->showMessage(QString("No results found for '%1'").arg(query), 3000);
    } else {
        statusBar()->showMessage(QString("Found %1 result(s)").arg(results.size()), 3000);
    }

    if (currentTargetVisible && m_taskPane->containsTarget(currentTarget)) {
        m_taskPane->setActiveTarget(currentTarget);
    } else {
        m_taskPane->setActiveTarget(EditorTarget());
    }
}

void MainWindow::onSearchCleared()
{
    const EditorTarget currentTarget = m_editorPane->currentTarget();
    const QString previousQuery = m_taskPane->currentSearchQuery();
    const QList<SearchResult> previousResults = previousQuery.isEmpty()
        ? QList<SearchResult>()
        : DatabaseManager::instance().searchItems(previousQuery);

    m_taskPane->clearSearchResults();

    if (m_taskPane->containsTarget(currentTarget)) {
        m_taskPane->setActiveTarget(currentTarget);
    } else {
        if (currentTarget.isValid()) {
            if (m_editorPane->loadItem(EditorTarget())) {
                m_taskPane->setActiveTarget(EditorTarget());
            } else {
                if (!previousQuery.isEmpty()) {
                    m_taskPane->showSearchResults(previousQuery, previousResults);
                }
                m_searchBar->setSearchText(previousQuery);
                m_taskPane->setActiveTarget(currentTarget);
            }
        } else {
            m_taskPane->setActiveTarget(EditorTarget());
        }
    }
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

void MainWindow::onItemTitleChanged(const EditorTarget &target, const QString &title)
{
    Q_UNUSED(title);
    refreshTaskPaneForCurrentContext(target);
}

void MainWindow::onGenerateTitleRequested(const EditorTarget &target, const QString &content)
{
    if (!target.isValid()) {
        return;
    }

    m_titleTarget = target;
    statusBar()->showMessage("Generating title with AI...");
    AIService::instance().generateTitle(content);
}

void MainWindow::onTitleGenerated(const QString &title)
{
    if (!m_titleTarget.isValid()) {
        m_editorPane->resetTitleGenerationPending();
        return;
    }

    bool saved = false;
    if (m_titleTarget.kind == EditorTargetKind::Task) {
        saved = DatabaseManager::instance().updateTaskTitle(m_titleTarget.id, title);
    } else if (m_titleTarget.kind == EditorTargetKind::SubTask) {
        saved = DatabaseManager::instance().updateSubtaskTitle(m_titleTarget.id, title);
    }

    if (!saved) {
        onAIError("The generated title could not be saved.");
        return;
    }

    statusBar()->showMessage(QString("Title updated: %1").arg(title), 5000);
    refreshTaskPaneForCurrentContext(m_titleTarget);
    if (m_editorPane->currentTarget() == m_titleTarget) {
        m_editorPane->loadItem(m_titleTarget);
    }

    m_editorPane->resetTitleGenerationPending();
    m_titleTarget = EditorTarget();
}

void MainWindow::onAIError(const QString &message)
{
    statusBar()->showMessage("AI Error: " + message, 5000);
    QMessageBox::warning(this, "AI Error", message);
    m_editorPane->resetTitleGenerationPending();
    m_titleTarget = EditorTarget();
    m_summaryTarget = EditorTarget();
}

void MainWindow::onSummarizeRequested(const EditorTarget &target, const QString &content)
{
    if (!target.isValid()) {
        return;
    }

    m_summaryTarget = target;
    statusBar()->showMessage("Summarizing content with AI...");
    AIService::instance().summarizeContent(content);
}

void MainWindow::onSummaryGenerated(const QString &summary)
{
    if (!m_summaryTarget.isValid()) {
        return;
    }

    QString existingContent;
    if (m_summaryTarget.kind == EditorTargetKind::Task) {
        const Task task = DatabaseManager::instance().getTask(m_summaryTarget.id);
        if (task.id <= 0) {
            onAIError("The summarized item could not be reloaded.");
            return;
        }
        existingContent = task.content;
    } else if (m_summaryTarget.kind == EditorTargetKind::SubTask) {
        const SubTask subtask = DatabaseManager::instance().getSubtask(m_summaryTarget.id);
        if (subtask.id <= 0) {
            onAIError("The summarized item could not be reloaded.");
            return;
        }
        existingContent = subtask.content;
    }

    const QString summaryHtml = QString(
        "<hr><blockquote><p><strong>AI Summary</strong></p><p>%1</p></blockquote>"
    ).arg(summary.toHtmlEscaped().replace("\n", "</p><p>"));
    const QString newContent = existingContent + summaryHtml;

    bool saved = false;
    if (m_summaryTarget.kind == EditorTargetKind::Task) {
        saved = DatabaseManager::instance().updateTaskContent(m_summaryTarget.id, newContent);
    } else if (m_summaryTarget.kind == EditorTargetKind::SubTask) {
        saved = DatabaseManager::instance().updateSubtaskContent(m_summaryTarget.id, newContent);
    }

    if (!saved) {
        onAIError("The generated summary could not be saved.");
        return;
    }

    refreshTaskPaneForCurrentContext(m_summaryTarget);
    if (m_editorPane->currentTarget() == m_summaryTarget) {
        m_editorPane->loadItem(m_summaryTarget);
    }
    statusBar()->showMessage("Summary added.", 5000);
    m_summaryTarget = EditorTarget();
}

void MainWindow::refreshTaskPaneForCurrentContext(const EditorTarget &preferredTarget)
{
    const EditorTarget target = preferredTarget.isValid()
        ? preferredTarget
        : m_editorPane->currentTarget();
    const QString query = m_taskPane->isShowingSearchResults()
        ? m_taskPane->currentSearchQuery()
        : QString();

    if (!query.isEmpty()) {
        m_taskPane->showSearchResults(query, DatabaseManager::instance().searchItems(query));
    } else {
        m_taskPane->refreshCurrentView();
    }

    if (m_taskPane->containsTarget(target)) {
        m_taskPane->setActiveTarget(target);
        return;
    }

    if (target.isValid() && m_editorPane->currentTarget() == target) {
        if (m_editorPane->loadItem(EditorTarget())) {
            m_taskPane->setActiveTarget(EditorTarget());
        } else {
            m_taskPane->setActiveTarget(target);
        }
    } else {
        m_taskPane->setActiveTarget(EditorTarget());
    }
}

void MainWindow::showSettings()
{
    SettingsDialog dialog(this);
    dialog.exec();
}

void MainWindow::onExportTaskSummaryRequested()
{
    const int productId = m_productPane->selectedProductId();
    if (productId <= 0) {
        QMessageBox::information(this, "Export Task Summary",
                                 "Please select a product first.");
        return;
    }

    auto &database = DatabaseManager::instance();
    const QList<Task> activeTasks =
        database.getTasksForProduct(productId, TaskStatus::Active);
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

    const QList<int> selectedIds = dialog.selectedTaskIds();
    if (validateTaskSummaryExportInputs(productId, activeTasks, selectedIds)
        == TaskSummaryExportIssue::NoTaskSelected) {
        QMessageBox::information(this, "Export Task Summary",
                                 "Please select at least one task to export.");
        return;
    }

    const QSet<int> selectedSet(selectedIds.begin(), selectedIds.end());
    QList<Task> selectedTasks;
    selectedTasks.reserve(selectedSet.size());
    for (const Task &task : activeTasks) {
        if (selectedSet.contains(task.id)) {
            selectedTasks.append(task);
        }
    }

    if (selectedTasks.isEmpty()) {
        QMessageBox::information(this, "Export Task Summary",
                                 "The selected tasks are no longer available.");
        return;
    }

    const Product product = database.getProduct(productId);
    const QString productName =
        product.id > 0 ? product.name : QStringLiteral("Unknown Product");
    const QDateTime now = QDateTime::currentDateTime();
    const QString markdown =
        buildTaskSummaryMarkdown(productName, now, selectedTasks, 120);

    const QString defaultFileName =
        QString("task-summary-%1.md").arg(now.toString("yyyyMMdd-HHmm"));
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        "Export Task Summary",
        defaultFileName,
        "Markdown Files (*.md)");
    if (filePath.isEmpty()) {
        statusBar()->showMessage("Task summary export canceled.", 3000);
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this,
                             "Export Task Summary",
                             QString("Failed to write file:\n%1\n\n%2")
                                 .arg(filePath, file.errorString()));
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << markdown;
    file.close();

    statusBar()->showMessage(
        QString("Exported %1 task(s) to %2")
            .arg(static_cast<int>(selectedTasks.size()))
            .arg(filePath),
        5000);
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
