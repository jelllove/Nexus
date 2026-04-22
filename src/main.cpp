#include <QApplication>
#include <QIcon>
#include <QSharedMemory>
#include <QSettings>
#include <QtWebEngineWidgets/QWebEngineView>
#include "app/MainWindow.h"
#include "db/DatabaseManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Nexus");
    app.setApplicationVersion(APP_VERSION);
    app.setOrganizationName("Nexus");

    // Single-instance guard: prevent multiple Nexus processes
    QSharedMemory singleInstanceGuard("NexusTaskManagerSingleInstance");
    if (!singleInstanceGuard.create(1)) {
        // Another instance is already running
        return 0;
    }

    // Don't quit when last window is closed (we minimize to tray)
    app.setQuitOnLastWindowClosed(false);

    // Initialize database (use custom path from QSettings if set)
    QSettings settings;
    QString dbPath = settings.value("db_path").toString();
    DatabaseManager::instance().initialize(dbPath.isEmpty() ? QString() : dbPath);

    // Daily automatic backup
    DatabaseManager::instance().backupDatabase();

    // Create and show main window
    MainWindow mainWindow;
    mainWindow.show();
    mainWindow.raise();
    mainWindow.activateWindow();

    return app.exec();
}
