#pragma once

#include <QObject>
#include <QString>

// Bridge object exposed to JavaScript via QWebChannel
class EditorBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString content READ content WRITE setContent NOTIFY contentChanged)

public:
    explicit EditorBridge(QObject *parent = nullptr);

    QString content() const { return m_content; }

public slots:
    // Called from JavaScript when editor content changes
    void setContent(const QString &content);

    // Called from JavaScript to notify content is ready
    void onEditorReady();

    // Called from C++ to load content into editor
    void loadContent(const QString &content);

    // Called from JavaScript to request image insertion
    void requestImageInsert();

signals:
    void contentChanged(const QString &content);
    void editorReady();
    void loadContentRequested(const QString &content);
    void imageInsertRequested();

private:
    QString m_content;
};
