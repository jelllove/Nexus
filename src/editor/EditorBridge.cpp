#include "EditorBridge.h"
#include <QDesktopServices>
#include <QUrl>

EditorBridge::EditorBridge(QObject *parent)
    : QObject(parent)
{
}

void EditorBridge::setContent(const QString &content)
{
    if (m_content != content) {
        m_content = content;
        emit contentChanged(content);
    }
}

void EditorBridge::onEditorReady()
{
    emit editorReady();
}

void EditorBridge::loadContent(const QString &content)
{
    m_content = content;
    emit loadContentRequested(content);
}

void EditorBridge::requestImageInsert()
{
    emit imageInsertRequested();
}

void EditorBridge::openExternalUrl(const QString &url)
{
    QDesktopServices::openUrl(QUrl(url));
}

void EditorBridge::requestGenerateTitle()
{
    emit generateTitleRequested();
}

void EditorBridge::requestSummarize()
{
    emit summarizeRequested();
}
