#pragma once

#include <QWidget>
#include <QWebEngineView>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include "editor/EditorBridge.h"
#include "models/EditorTarget.h"

class EditorPane : public QWidget
{
    Q_OBJECT

public:
    explicit EditorPane(QWidget *parent = nullptr);

    bool loadItem(const EditorTarget &target);
    void clear();
    EditorTarget currentTarget() const { return m_currentTarget; }
    void resetTitleGenerationPending() { m_titleGenerationPending = false; }

signals:
    void contentChanged(const EditorTarget &target, const QString &content);
    void titleChanged(const EditorTarget &target, const QString &title);
    void generateTitleRequested(const EditorTarget &target, const QString &content);
    void summarizeRequested(const EditorTarget &target, const QString &content);
    void autoGenerateTitleRequested(const EditorTarget &target, const QString &content);
    void saveFailed(const QString &message);

private slots:
    void onEditorContentChanged(const QString &content);
    void onEditorReady();
    void onAutoSave();
    void onImageInsertRequested();

private:
    bool saveCurrentContent();
    void setupUi();

    QWebEngineView *m_webView;
    QWebChannel *m_channel;
    EditorBridge *m_bridge;
    QTimer *m_autoSaveTimer;

    EditorTarget m_currentTarget;
    bool m_editorReady = false;
    QString m_pendingContent;
    QString m_pendingTitle;
    QString m_pendingTimestamp;
    bool m_hasUnsavedChanges = false;
    bool m_titleGenerationPending = false;
};
