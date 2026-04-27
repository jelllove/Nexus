#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QSplitter>
#include "ui/ProductPane.h"
#include "ui/TaskPane.h"
#include "ui/EditorPane.h"
#include "ui/SearchBar.h"

class UpdateService;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onProductSelected(int productId);
    void onTaskSelected(int taskId);
    void onTaskTitleChanged(int taskId, const QString &title);
    void onSearchRequested(const QString &query);
    void onSearchCleared();
    void onHotkeyPressed();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onGenerateTitleRequested(int taskId, const QString &content);
    void onTitleGenerated(const QString &title);
    void onSummarizeRequested(int taskId, const QString &content);
    void onSummaryGenerated(const QString &summary);
    void onAIError(const QString &message);
    void showSettings();
    void onUpdateAvailable(const QString &latestVersion, const QString &downloadUrl, const QString &releaseNotes);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished(const QString &installerPath);

private:
    void setupUi();
    void setupTrayIcon();
    void setupGlobalHotkey();
    void setupMenuBar();
    void toggleVisibility();
    void checkForUpdates();

    // UI components
    QSplitter *m_splitter;
    ProductPane *m_productPane;
    TaskPane *m_taskPane;
    EditorPane *m_editorPane;
    SearchBar *m_searchBar;

    // System tray
    QSystemTrayIcon *m_trayIcon;

    // State
    int m_currentTaskIdForTitle = -1;
    int m_currentTaskIdForSummary = -1;
};
