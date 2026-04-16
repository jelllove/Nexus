#include <QApplication>
#include <QIcon>
#include <QtWebEngineWidgets/QWebEngineView>
#include "app/MainWindow.h"
#include "db/DatabaseManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Nexus");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Nexus");


    // Don't quit when last window is closed (we minimize to tray)
    app.setQuitOnLastWindowClosed(false);

    // Initialize database
    DatabaseManager::instance().initialize();

    // Create and show main window
    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
