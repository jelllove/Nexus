#include "MainWindow.h"
#include "ui/SettingsDialog.h"
#include "services/AIService.h"
#include "services/UpdateService.h"
#include "platform/GlobalHotkey.h"
#include "db/DatabaseManager.h"
#include <QCloseEvent>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
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
    connect(m_taskPane, &TaskPane::taskSelected,
            this, &MainWindow::onTaskSelected);
    connect(m_searchBar, &SearchBar::searchRequested,
            this, &MainWindow::onSearchRequested);
    connect(m_searchBar, &SearchBar::searchCleared,
            this, &MainWindow::onSearchCleared);
    connect(m_editorPane, &EditorPane::generateTitleRequested,
            this, &MainWindow::onGenerateTitleRequested);
    connect(m_editorPane, &EditorPane::titleChanged,
            this, &MainWindow::onTaskTitleChanged);
}

void MainWindow::setupMenuBar()
{
    QMenuBar *menuBar = this->menuBar();

    // File menu
    QMenu *fileMenu = menuBar->addMenu("&File");

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
    m_searchBar->clear();
    m_taskPane->loadTasks(productId);
    m_editorPane->clear();
}

void MainWindow::onTaskSelected(int taskId)
{
    m_editorPane->loadTask(taskId);
}

void MainWindow::onSearchRequested(const QString &query)
{
    auto results = DatabaseManager::instance().searchTasks(query);
    if (results.isEmpty()) {
        statusBar()->showMessage(QString("No results found for '%1'").arg(query), 3000);
    } else {
        statusBar()->showMessage(QString("Found %1 result(s)").arg(results.size()), 3000);
    }
    // Show results in task pane
    // We need to access the model directly through the task pane
    // For now, show a message
}

void MainWindow::onSearchCleared()
{
    // Reload current product's tasks
    int productId = m_productPane->selectedProductId();
    if (productId > 0) {
        m_taskPane->loadTasks(productId);
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

void MainWindow::onTaskTitleChanged(int taskId, const QString &title)
{
    Q_UNUSED(taskId);
    Q_UNUSED(title);
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
        int productId = m_productPane->selectedProductId();
        if (productId > 0) {
            m_taskPane->loadTasks(productId);
        }
        // Reload editor to show new title
        m_editorPane->loadTask(m_currentTaskIdForTitle);
    }
    m_currentTaskIdForTitle = -1;
}

void MainWindow::onAIError(const QString &message)
{
    statusBar()->showMessage("AI Error: " + message, 5000);
    m_currentTaskIdForTitle = -1;
}

void MainWindow::showSettings()
{
    SettingsDialog dialog(this);
    dialog.exec();
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
