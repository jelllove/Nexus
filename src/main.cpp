#include <QApplication>
#include <QIcon>
#include <QSharedMemory>
#include <QtWebEngineWidgets/QWebEngineView>
#include "app/MainWindow.h"
#include "db/DatabaseManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Nexus");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Nexus");

    // Single-instance guard: prevent multiple Nexus processes
    QSharedMemory singleInstanceGuard("NexusTaskManagerSingleInstance");
    if (!singleInstanceGuard.create(1)) {
        // Another instance is already running
        return 0;
    }

    // Don't quit when last window is closed (we minimize to tray)
    app.setQuitOnLastWindowClosed(false);

    // Initialize database
    DatabaseManager::instance().initialize();

    // Create and show main window
    MainWindow mainWindow;
    mainWindow.show();
    mainWindow.raise();
    mainWindow.activateWindow();

    return app.exec();
}
