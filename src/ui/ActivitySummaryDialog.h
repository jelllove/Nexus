#pragma once

#include <QDialog>
#include <QPointer>
#include <functional>
#include "services/ActivitySummaryService.h"

class QComboBox;
class QDateEdit;
class QLabel;
class QPushButton;
class QTextBrowser;
class QNetworkReply;
class MarkdownPreviewDialog;

class ActivitySummaryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ActivitySummaryDialog(QWidget *parent = nullptr,
                                   std::function<bool()> savePendingEdits = {});
    ~ActivitySummaryDialog() override;
    void reject() override;

private:
    void invalidatePreview();
    void buildPreview();
    void generate();
    void setBusy(bool busy);
    void cancelRequest();

    QComboBox *m_period;
    QComboBox *m_basis;
    QComboBox *m_language;
    QDateEdit *m_start;
    QDateEdit *m_end;
    QLabel *m_status;
    QTextBrowser *m_source;
    QPushButton *m_preview;
    QPushButton *m_generate;
    QPushButton *m_cancel;
    QString m_payload;
    QString m_reportHeader;
    QString m_timestampAppendix;
    ActivitySummaryLanguage m_previewLanguage = ActivitySummaryLanguage::English;
    std::function<bool()> m_savePendingEdits;
    QPointer<QNetworkReply> m_request;
    QPointer<MarkdownPreviewDialog> m_result;
    bool m_cancelled = false;
};
