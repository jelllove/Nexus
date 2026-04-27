#pragma once

#include <QWidget>
#include <QWebEngineView>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include "editor/EditorBridge.h"

class EditorPane : public QWidget
{
    Q_OBJECT

public:
    explicit EditorPane(QWidget *parent = nullptr);

    void loadTask(int taskId);
    void clear();
    void resetTitleGenerationPending() { m_titleGenerationPending = false; }

signals:
    void contentChanged(int taskId, const QString &content);
    void titleChanged(int taskId, const QString &title);
    void generateTitleRequested(int taskId, const QString &content);
    void summarizeRequested(int taskId, const QString &content);
    void autoGenerateTitleRequested(int taskId, const QString &content);

private slots:
    void onEditorContentChanged(const QString &content);
    void onEditorReady();
    void onAutoSave();
    void onImageInsertRequested();

private:
    void setupUi();

    QWebEngineView *m_webView;
    QWebChannel *m_channel;
    EditorBridge *m_bridge;
    QTimer *m_autoSaveTimer;

    int m_currentTaskId = -1;
    bool m_editorReady = false;
    QString m_pendingContent;
    QString m_pendingTitle;
    QString m_pendingTimestamp;
    bool m_titleGenerationPending = false;
};
