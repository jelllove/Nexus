#include "MarkdownPreviewDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTextDocument>
#include <QVBoxLayout>

MarkdownPreviewDialog::MarkdownPreviewDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Markdown Preview");
    setModal(false);
    setMinimumSize(820, 620);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    m_view = new QTextBrowser(this);
    m_view->setReadOnly(true);
    m_view->setOpenExternalLinks(true);
    layout->addWidget(m_view, 1);

    auto *buttons = new QDialogButtonBox(this);
    m_copyMarkdownButton = buttons->addButton("Copy Markdown", QDialogButtonBox::ActionRole);
    m_copyHtmlButton = buttons->addButton("Copy HTML", QDialogButtonBox::ActionRole);
    m_saveAsButton = buttons->addButton("Save As .md", QDialogButtonBox::ActionRole);
    QPushButton *closeButton = buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);

    connect(m_copyMarkdownButton, &QPushButton::clicked, this, &MarkdownPreviewDialog::onCopyMarkdown);
    connect(m_copyHtmlButton, &QPushButton::clicked, this, &MarkdownPreviewDialog::onCopyHtml);
    connect(m_saveAsButton, &QPushButton::clicked, this, &MarkdownPreviewDialog::onSaveAs);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
}

void MarkdownPreviewDialog::setMarkdownContent(const QString &markdown)
{
    m_markdown = markdown;
    m_view->setMarkdown(markdown);
}

void MarkdownPreviewDialog::onCopyMarkdown()
{
    QClipboard *clipboard = QApplication::clipboard();
    if (!clipboard) {
        QMessageBox::warning(this, "Copy Failed", "Clipboard is unavailable.");
        return;
    }
    clipboard->setText(m_markdown);
}

void MarkdownPreviewDialog::onCopyHtml()
{
    QClipboard *clipboard = QApplication::clipboard();
    if (!clipboard) {
        QMessageBox::warning(this, "Copy Failed", "Clipboard is unavailable.");
        return;
    }

    QTextDocument document;
    document.setMarkdown(m_markdown);
    const QString html = document.toHtml();
    if (html.trimmed().isEmpty() && !m_markdown.trimmed().isEmpty()) {
        clipboard->setText(m_markdown);
        QMessageBox::warning(this, "Copy HTML Failed",
                             "Failed to convert markdown to HTML. Markdown text was copied instead.");
        return;
    }

    auto *mimeData = new QMimeData();
    mimeData->setHtml(html);
    mimeData->setText(m_markdown);
    clipboard->setMimeData(mimeData);
}

void MarkdownPreviewDialog::onSaveAs()
{
    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (defaultDir.isEmpty()) {
        defaultDir = QDir::homePath();
    }
    QString defaultPath = defaultDir + "/nexus_preview_" +
                          QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".md";

    QString path = QFileDialog::getSaveFileName(
        this,
        "Save Markdown Preview",
        defaultPath,
        "Markdown Files (*.md)");
    if (path.isEmpty()) {
        return;
    }
    if (!path.endsWith(".md", Qt::CaseInsensitive)) {
        path += ".md";
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Save Failed",
                             "Failed to write markdown file:\n" + file.errorString());
        return;
    }
    file.write(m_markdown.toUtf8());
    file.close();
}

