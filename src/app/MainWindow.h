#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QSplitter>
#include <QMap>
#include <QHash>
#include <QPointer>
#include <functional>
#include "ui/ProductPane.h"
#include "ui/TaskPane.h"
#include "ui/EditorPane.h"
#include "ui/SearchBar.h"
#include "models/Task.h"
#include "services/TaskExportService.h"

class UpdateService;
class MarkdownPreviewDialog;

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
    void onSubTaskSelected(int subTaskId);
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
    void exportTasksToMarkdown();
    void previewTasksAsMarkdown();
    void showSettings();
    void onUpdateAvailable(const QString &latestVersion, const QString &downloadUrl, const QString &releaseNotes);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished(const QString &installerPath);

private:
    struct PreparedMarkdownPayload {
        QList<ExportTaskItem> items;
        QString markdown;
        bool includeDescription = false;
        int fallbackCount = 0;
    };

    struct SearchViewSnapshot {
        int productId = -1;
        int selectedTaskId = -1;
        TaskPane::ViewMode mode = TaskPane::ViewMode::Active;
        bool valid = false;
    };

    void setupUi();
    void setupTrayIcon();
    void setupGlobalHotkey();
    void setupMenuBar();
    void toggleVisibility();
    void checkForUpdates();
    bool prepareMarkdownPayload(PreparedMarkdownPayload &payload, const QString &actionName);
    bool promptExportScopeDialog(bool &exportAllProducts, int &selectedProductId, bool &includeDescription);
    bool promptTaskSelectionDialog(
        const QMap<int, QList<Task>> &tasksByProduct,
        const QHash<int, QString> &productNames,
        QList<ExportTaskItem> &selectedExportItems);
    QMap<int, QList<Task>> collectActiveTasksForExport(bool exportAllProducts, int selectedProductId,
                                                       QHash<int, QString> &productNames) const;
    bool resolveSimpleDescriptions(
        QList<ExportTaskItem> &items,
        bool includeDescription,
        int &fallbackCount,
        const std::function<void(int completed, int total, const QString &message)> &progressCallback,
        const std::function<bool()> &isCancelled) const;

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
    bool m_searchActive = false;
    SearchViewSnapshot m_searchSnapshot;
    QPointer<MarkdownPreviewDialog> m_markdownPreviewDialog;
};
