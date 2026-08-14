#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QSplitter>
#include "models/EditorTarget.h"
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
    void onItemSelected(const EditorTarget &target);
    void onItemTitleChanged(const EditorTarget &target, const QString &title);
    void onSearchRequested(const QString &query);
    void onSearchCleared();
    void onHotkeyPressed();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onGenerateTitleRequested(const EditorTarget &target, const QString &content);
    void onTitleGenerated(const QString &title);
    void onSummarizeRequested(const EditorTarget &target, const QString &content);
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
    void refreshTaskPaneForCurrentContext(const EditorTarget &preferredTarget = EditorTarget());

    // UI components
    QSplitter *m_splitter;
    ProductPane *m_productPane;
    TaskPane *m_taskPane;
    EditorPane *m_editorPane;
    SearchBar *m_searchBar;

    // System tray
    QSystemTrayIcon *m_trayIcon;

    // State
    EditorTarget m_titleTarget;
    EditorTarget m_summaryTarget;
};
