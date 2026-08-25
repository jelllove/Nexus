#include <QApplication>
#include <QIcon>
#include <QSharedMemory>
#include <QSettings>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QtWebEngineWidgets/QWebEngineView>
#include "app/MainWindow.h"
#include "db/DatabaseManager.h"

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

    // Don't quit when last window is closed (we minimize to tray)
    app.setQuitOnLastWindowClosed(false);

    // Initialize database (use custom path from QSettings if set)
    QSettings settings;
    QString dbPath = settings.value("db_path").toString();
    bool dbReady = DatabaseManager::instance().initialize(dbPath.isEmpty() ? QString() : dbPath);
    if (!dbReady && !dbPath.isEmpty()) {
        // Recover from stale custom path on another machine by retrying default location.
        dbReady = DatabaseManager::instance().initialize();
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

    // Create and show main window
    MainWindow mainWindow;
    mainWindow.show();
    mainWindow.raise();
    mainWindow.activateWindow();

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
