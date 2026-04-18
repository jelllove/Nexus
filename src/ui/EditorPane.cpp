#include "EditorPane.h"
#include "db/DatabaseManager.h"
#include "services/ImageManager.h"
#include <QFileDialog>
#include <QUrl>
#include <QDesktopServices>
#include <QWebEnginePage>

// Custom page that intercepts link clicks and opens them externally
class EditorWebPage : public QWebEnginePage
{
public:
    using QWebEnginePage::QWebEnginePage;

    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool /*isMainFrame*/) override
    {
        if (type == NavigationTypeLinkClicked) {
            QDesktopServices::openUrl(url);
            return false;
        }
        return QWebEnginePage::acceptNavigationRequest(url, type, true);
    }
};

EditorPane::EditorPane(QWidget *parent)
    : QWidget(parent)
{
    setupUi();

    // Auto-save timer (3 seconds after last change)
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(true);
    m_autoSaveTimer->setInterval(3000);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &EditorPane::onAutoSave);
}

void EditorPane::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // WebEngine view — the ribbon toolbar is now inside the HTML
    m_webView = new QWebEngineView(this);

    // Use custom page to intercept link clicks
    auto *page = new EditorWebPage(m_webView);
    m_webView->setPage(page);

    m_channel = new QWebChannel(this);
    m_bridge = new EditorBridge(this);

    m_channel->registerObject("bridge", m_bridge);
    m_webView->page()->setWebChannel(m_channel);

    // Load editor HTML from resources
    m_webView->setUrl(QUrl("qrc:/editor/index.html"));

    layout->addWidget(m_webView, 1);

    // Connect bridge signals
    connect(m_bridge, &EditorBridge::contentChanged,
            this, &EditorPane::onEditorContentChanged);
    connect(m_bridge, &EditorBridge::editorReady,
            this, &EditorPane::onEditorReady);
    connect(m_bridge, &EditorBridge::imageInsertRequested,
            this, &EditorPane::onImageInsertRequested);
}

void EditorPane::loadTask(int taskId)
{
    // Save current task before switching
    if (m_currentTaskId > 0 && m_autoSaveTimer->isActive()) {
        onAutoSave();
    }

    m_currentTaskId = taskId;

    if (taskId <= 0) {
        clear();
        return;
    }

    Task task = DatabaseManager::instance().getTask(taskId);

    QString title = task.title;
    QString content = task.content;
    QString timestamp;
    if (task.updatedAt.isValid()) {
        timestamp = task.updatedAt.toString("dddd, MMMM d, yyyy    h:mm AP");
    } else if (task.createdAt.isValid()) {
        timestamp = task.createdAt.toString("dddd, MMMM d, yyyy    h:mm AP");
    }

    if (m_editorReady) {
        // Set title and timestamp via JS
        QString escapedTitle = title;
        escapedTitle.replace("'", "\\'");
        m_webView->page()->runJavaScript(
            QString("window.setPageTitle('%1')").arg(escapedTitle));
        m_webView->page()->runJavaScript(
            QString("window.setPageTimestamp('%1')").arg(timestamp));
        m_bridge->loadContent(content);
    } else {
        m_pendingContent = content;
        m_pendingTitle = title;
        m_pendingTimestamp = timestamp;
    }
}

void EditorPane::clear()
{
    m_currentTaskId = -1;
    if (m_editorReady) {
        m_webView->page()->runJavaScript("window.setPageTitle('')");
        m_webView->page()->runJavaScript("window.setPageTimestamp('')");
        m_bridge->loadContent("");
    }
}

void EditorPane::onEditorContentChanged(const QString &content)
{
    if (m_currentTaskId <= 0) return;

    // Check if this is a title change from the page title field
    if (content.startsWith("__TITLE__:")) {
        QString newTitle = content.mid(10);
        if (!newTitle.isEmpty()) {
            DatabaseManager::instance().updateTaskTitle(m_currentTaskId, newTitle);
            emit titleChanged(m_currentTaskId, newTitle);
        }
        return;
    }

    // Restart auto-save timer
    m_autoSaveTimer->start();
}

void EditorPane::onEditorReady()
{
    m_editorReady = true;
    if (!m_pendingTitle.isEmpty() || !m_pendingContent.isEmpty()) {
        QString escapedTitle = m_pendingTitle;
        escapedTitle.replace("'", "\\'");
        m_webView->page()->runJavaScript(
            QString("window.setPageTitle('%1')").arg(escapedTitle));
        m_webView->page()->runJavaScript(
            QString("window.setPageTimestamp('%1')").arg(m_pendingTimestamp));
        m_bridge->loadContent(m_pendingContent);
        m_pendingContent.clear();
        m_pendingTitle.clear();
        m_pendingTimestamp.clear();
    }
}

void EditorPane::onAutoSave()
{
    if (m_currentTaskId <= 0) return;

    QString content = m_bridge->content();

    // Save to database
    DatabaseManager::instance().updateTaskContent(m_currentTaskId, content);

    // Save snapshot for undo history
    DatabaseManager::instance().saveContentSnapshot(m_currentTaskId, content);

    emit contentChanged(m_currentTaskId, content);
}

void EditorPane::onImageInsertRequested()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Insert Image",
        QString(), "Images (*.png *.jpg *.jpeg *.gif *.bmp *.svg)");

    if (filePath.isEmpty()) return;

    QString imageUrl = ImageManager::instance().storeImage(filePath);
    if (!imageUrl.isEmpty()) {
        QString js = QString("window.insertImage('%1')").arg(imageUrl);
        m_webView->page()->runJavaScript(js);
    }
}
