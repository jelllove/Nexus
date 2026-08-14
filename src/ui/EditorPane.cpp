#include "EditorPane.h"

#include "db/DatabaseManager.h"
#include "services/ImageManager.h"

#include <QDesktopServices>
#include <QFileDialog>
#include <QRegularExpression>
#include <QUrl>
#include <QWebEnginePage>

namespace {
QString toJavaScriptStringLiteral(const QString &value)
{
    QString escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("'", "\\'");
    escaped.replace("\r", "\\r");
    escaped.replace("\n", "\\n");
    escaped.replace(QChar(0x2028), "\\u2028");
    escaped.replace(QChar(0x2029), "\\u2029");
    return QStringLiteral("'%1'").arg(escaped);
}

void setEditorMetadata(QWebEngineView *webView, const QString &functionName, const QString &value)
{
    webView->page()->runJavaScript(
        QStringLiteral("window.%1(%2)")
            .arg(functionName, toJavaScriptStringLiteral(value)));
}

QString displayTimestamp(const QDateTime &createdAt, const QDateTime &updatedAt)
{
    const QDateTime effectiveTime = updatedAt.isValid() ? updatedAt : createdAt;
    if (!effectiveTime.isValid()) {
        return QString();
    }

    return effectiveTime.toString(QStringLiteral("dddd, MMMM d, yyyy    h:mm AP"));
}
}

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

    connect(m_bridge, &EditorBridge::contentChanged,
            this, &EditorPane::onEditorContentChanged);
    connect(m_bridge, &EditorBridge::editorReady,
            this, &EditorPane::onEditorReady);
    connect(m_bridge, &EditorBridge::imageInsertRequested,
            this, &EditorPane::onImageInsertRequested);
    connect(m_bridge, &EditorBridge::generateTitleRequested, this, [this]() {
        if (m_currentTarget.isValid()) {
            emit generateTitleRequested(m_currentTarget, m_bridge->content());
        }
    });
    connect(m_bridge, &EditorBridge::summarizeRequested, this, [this]() {
        if (m_currentTarget.isValid()) {
            emit summarizeRequested(m_currentTarget, m_bridge->content());
        }
    });
}

void EditorPane::loadItem(const EditorTarget &target)
{
    if (m_currentTarget.isValid() && m_autoSaveTimer->isActive()) {
        m_autoSaveTimer->stop();
        if (!saveCurrentContent()) {
            return;
        }
    }

    if (!target.isValid()) {
        clear();
        return;
    }

    QString title;
    QString content;
    QDateTime createdAt;
    QDateTime updatedAt;

    if (target.kind == EditorTargetKind::Task) {
        const Task task = DatabaseManager::instance().getTask(target.id);
        if (task.id <= 0) {
            clear();
            return;
        }

        title = task.title;
        content = task.content;
        createdAt = task.createdAt;
        updatedAt = task.updatedAt;
    } else if (target.kind == EditorTargetKind::SubTask) {
        const SubTask subtask = DatabaseManager::instance().getSubtask(target.id);
        if (subtask.id <= 0) {
            clear();
            return;
        }

        title = subtask.title;
        content = subtask.content;
        createdAt = subtask.createdAt;
        updatedAt = subtask.updatedAt;
    } else {
        clear();
        return;
    }

    m_currentTarget = target;
    const QString timestamp = displayTimestamp(createdAt, updatedAt);

    if (m_editorReady) {
        setEditorMetadata(m_webView, QStringLiteral("setPageTitle"), title);
        setEditorMetadata(m_webView, QStringLiteral("setPageTimestamp"), timestamp);
        m_bridge->loadContent(content);
    } else {
        m_pendingContent = content;
        m_pendingTitle = title;
        m_pendingTimestamp = timestamp;
    }
}

void EditorPane::clear()
{
    m_autoSaveTimer->stop();
    m_currentTarget = EditorTarget();
    m_pendingContent.clear();
    m_pendingTitle.clear();
    m_pendingTimestamp.clear();

    if (m_editorReady) {
        setEditorMetadata(m_webView, QStringLiteral("setPageTitle"), QString());
        setEditorMetadata(m_webView, QStringLiteral("setPageTimestamp"), QString());
        m_bridge->loadContent(QString());
    }
}

void EditorPane::onEditorContentChanged(const QString &content)
{
    if (!m_currentTarget.isValid()) {
        return;
    }

    if (content.startsWith(QStringLiteral("__TITLE__:"))) {
        const QString newTitle = content.mid(10);

        bool saved = false;
        if (m_currentTarget.kind == EditorTargetKind::Task) {
            saved = DatabaseManager::instance().updateTaskTitle(m_currentTarget.id, newTitle);
        } else if (m_currentTarget.kind == EditorTargetKind::SubTask) {
            saved = DatabaseManager::instance().updateSubtaskTitle(m_currentTarget.id, newTitle);
        }

        if (!saved) {
            emit saveFailed(QStringLiteral("Failed to save the item title."));
            return;
        }

        emit titleChanged(m_currentTarget, newTitle);
        return;
    }

    m_autoSaveTimer->start();
}

void EditorPane::onEditorReady()
{
    m_editorReady = true;
    if (!m_pendingTitle.isEmpty() || !m_pendingContent.isEmpty() || !m_pendingTimestamp.isEmpty()) {
        setEditorMetadata(m_webView, QStringLiteral("setPageTitle"), m_pendingTitle);
        setEditorMetadata(m_webView, QStringLiteral("setPageTimestamp"), m_pendingTimestamp);
        m_bridge->loadContent(m_pendingContent);
        m_pendingContent.clear();
        m_pendingTitle.clear();
        m_pendingTimestamp.clear();
    }
}

void EditorPane::onAutoSave()
{
    saveCurrentContent();
}

bool EditorPane::saveCurrentContent()
{
    if (!m_currentTarget.isValid()) {
        return true;
    }

    const QString content = m_bridge->content();
    auto &database = DatabaseManager::instance();

    bool contentSaved = false;
    bool historySaved = false;
    QString currentTitle;

    if (m_currentTarget.kind == EditorTargetKind::Task) {
        contentSaved = database.updateTaskContent(m_currentTarget.id, content);
        historySaved = database.saveContentSnapshot(m_currentTarget.id, content);
        currentTitle = database.getTask(m_currentTarget.id).title;
    } else if (m_currentTarget.kind == EditorTargetKind::SubTask) {
        contentSaved = database.updateSubtaskContent(m_currentTarget.id, content);
        historySaved = database.saveSubtaskContentSnapshot(m_currentTarget.id, content);
        currentTitle = database.getSubtask(m_currentTarget.id).title;
    }

    if (!contentSaved || !historySaved) {
        emit saveFailed(QStringLiteral("Failed to save the item note."));
        return false;
    }

    emit contentChanged(m_currentTarget, content);

    if (!m_titleGenerationPending && currentTitle.trimmed().isEmpty()) {
        QString plain = content;
        plain.remove(QRegularExpression("<[^>]*>"));
        plain = plain.trimmed();
        if (!plain.isEmpty()) {
            m_titleGenerationPending = true;
            emit autoGenerateTitleRequested(m_currentTarget, content);
        }
    }

    return true;
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
