#include <QApplication>
#include <QIcon>
#include <QSharedMemory>
#include <QSettings>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QtWebEngineWidgets/QWebEngineView>
#include "app/MainWindow.h"
#include "db/DatabaseManager.h"
#include "platform/InstallerSmoke.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

static const char *SOCKET_NAME = "NexusTaskManagerIPC";

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Nexus");
    app.setApplicationVersion(APP_VERSION);
    app.setOrganizationName("Nexus");

    const QStringList arguments = app.arguments();
    const int smokeArgument = arguments.indexOf("--installer-smoke");
    if (smokeArgument >= 0) {
        if (arguments.size() != 3 || smokeArgument != 1 || arguments.at(2).isEmpty()) {
            qCritical("Usage: Nexus --installer-smoke <new-output-directory>");
            return 1;
        }
        InstallerSmoke smoke(arguments.at(2));
        if (!smoke.prepare()) return 1;
        app.setQuitOnLastWindowClosed(false);
        MainWindow mainWindow;
        mainWindow.show();
        mainWindow.raise();
        mainWindow.activateWindow();
        smoke.start(&mainWindow);
        return app.exec();
    }

    // Single-instance guard: prevent multiple Nexus processes
    QSharedMemory singleInstanceGuard("NexusTaskManagerSingleInstance");
    if (!singleInstanceGuard.create(1)) {
        // Another instance is already running — ask it to show itself
        QLocalSocket socket;
        socket.connectToServer(SOCKET_NAME);
        if (socket.waitForConnected(1000)) {
            socket.write("show");
            socket.waitForBytesWritten(1000);
            socket.disconnectFromServer();
        }
        return 0;
    }

    app.setQuitOnLastWindowClosed(!QSystemTrayIcon::isSystemTrayAvailable());

    // Initialize database (use custom path from QSettings if set)
    QSettings settings;
    QString dbPath = settings.value("db_path").toString();
    bool dbReady = DatabaseManager::instance().initialize(dbPath.isEmpty() ? QString() : dbPath);
    if (!dbReady && !dbPath.isEmpty()) {
        const auto fallbackChoice = QMessageBox::warning(
            nullptr,
            "Nexus",
            QString("Failed to open configured database path:\n%1\n\nUse default local database path instead?")
                .arg(dbPath),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (fallbackChoice == QMessageBox::Yes) {
            dbReady = DatabaseManager::instance().initialize();
        }
    }
    if (!dbReady) {
        QMessageBox::critical(nullptr, "Nexus",
                              "Failed to initialize database. Please check database path/settings.");
        return 1;
    }

    // Daily automatic backup
    DatabaseManager::instance().backupDatabase();

    // Purge tasks deleted more than 30 days ago
    DatabaseManager::instance().purgeOldDeletedTasks(30);

    // Weekly compaction in delayed background run
    DatabaseManager::instance().scheduleWeeklyCompaction(7, 60 * 1000);

    // Create and show main window
    MainWindow mainWindow;
    mainWindow.show();
    mainWindow.raise();
    mainWindow.activateWindow();

#ifdef Q_OS_MACOS
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &mainWindow,
                     [&mainWindow](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive && !mainWindow.isVisible()) {
            mainWindow.show();
            mainWindow.raise();
            mainWindow.activateWindow();
        }
    });
#endif

    // IPC server: listen for "show" messages from other instances
    QLocalServer::removeServer(SOCKET_NAME);
    QLocalServer ipcServer;
    ipcServer.listen(SOCKET_NAME);
    QObject::connect(&ipcServer, &QLocalServer::newConnection, [&]() {
        QLocalSocket *client = ipcServer.nextPendingConnection();
        QObject::connect(client, &QLocalSocket::readyRead, [&mainWindow, client]() {
            client->readAll();
            mainWindow.show();
            mainWindow.raise();
            mainWindow.activateWindow();
#ifdef Q_OS_WIN
            // Force bring to front on Windows
            SetForegroundWindow(reinterpret_cast<HWND>(mainWindow.winId()));
#endif
            client->deleteLater();
        });
    });

    return app.exec();
}
