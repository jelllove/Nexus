#pragma once

#include <QDialog>

class QTextBrowser;
class QPushButton;

class MarkdownPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MarkdownPreviewDialog(QWidget *parent = nullptr);
    void setMarkdownContent(const QString &markdown);

private slots:
    void onCopyMarkdown();
    void onCopyHtml();
    void onSaveAs();

private:
    QString m_markdown;
    QTextBrowser *m_view = nullptr;
    QPushButton *m_copyMarkdownButton = nullptr;
    QPushButton *m_copyHtmlButton = nullptr;
    QPushButton *m_saveAsButton = nullptr;
};

