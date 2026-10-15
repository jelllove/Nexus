#include "ActivitySummaryDialog.h"
#include "MarkdownPreviewDialog.h"
#include "db/DatabaseManager.h"
#include "services/ActivitySummaryService.h"
#include "services/AIService.h"
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkReply>
#include <QPushButton>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

ActivitySummaryDialog::ActivitySummaryDialog(QWidget *parent, std::function<bool()> savePendingEdits)
    : QDialog(parent), m_savePendingEdits(std::move(savePendingEdits))
{
    setObjectName("activitySummaryDialog");
    setWindowTitle("AI Activity Summary");
    setModal(false);
    resize(850, 650);
    auto *layout = new QVBoxLayout(this);
    auto *notice = new QLabel(
        "Summarize all products' tasks and subtasks, including archived and recycled tasks.\n"
        "Dates include the full selected local calendar days. Preview is local; sending requires confirmation.\n"
        "Notes reflect current saved content, not past versions. Permanently deleted notes are unavailable.", this);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    auto *form = new QFormLayout;
    m_period = new QComboBox(this);
    m_period->setObjectName("summaryPeriod");
    m_period->addItems({"1 day (today)", "1 week", "2 weeks", "1 month", "3 months", "Custom dates"});
    m_period->setCurrentIndex(1);
    form->addRow("Period:", m_period);
    m_start = new QDateEdit(QDate::currentDate().addDays(-6), this);
    m_end = new QDateEdit(QDate::currentDate(), this);
    m_start->setObjectName("summaryStart");
    m_end->setObjectName("summaryEnd");
    for (auto *edit : {m_start, m_end}) {
        edit->setDisplayFormat("yyyy-MM-dd");
        edit->setCalendarPopup(true);
        edit->setEnabled(false);
    }
    form->addRow("From (inclusive):", m_start);
    form->addRow("Through (inclusive):", m_end);
    m_basis = new QComboBox(this);
    m_basis->setObjectName("summaryBasis");
    m_basis->addItem("Created or modified", static_cast<int>(ActivityTimeBasis::CreatedOrModified));
    m_basis->addItem("Created", static_cast<int>(ActivityTimeBasis::Created));
    m_basis->addItem("Modified (including recorded changes)", static_cast<int>(ActivityTimeBasis::Modified));
    form->addRow("Select by:", m_basis);
    m_language = new QComboBox(this);
    m_language->setObjectName("summaryLanguage");
    m_language->addItem("English", static_cast<int>(ActivitySummaryLanguage::English));
    m_language->addItem(QStringLiteral("中文"), static_cast<int>(ActivitySummaryLanguage::Chinese));
    form->addRow("Summary language:", m_language);
    layout->addLayout(form);
    m_source = new QTextBrowser(this);
    m_source->setObjectName("summarySource");
    m_source->setOpenExternalLinks(false);
    layout->addWidget(m_source, 1);
    m_status = new QLabel("Choose a period and preview the exact material before sending.", this);
    m_status->setObjectName("summaryStatus");
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status);
    auto *buttons = new QDialogButtonBox(this);
    m_preview = buttons->addButton("Preview source", QDialogButtonBox::ActionRole);
    m_generate = buttons->addButton("Generate with configured AI...", QDialogButtonBox::ActionRole);
    m_cancel = buttons->addButton("Cancel request", QDialogButtonBox::ActionRole);
    m_preview->setObjectName("summaryPreview");
    m_generate->setObjectName("summaryGenerate");
    m_generate->setEnabled(false);
    m_cancel->setEnabled(false);
    auto *close = buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);
    connect(m_period, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index != 5) {
            m_end->setDate(QDate::currentDate());
            m_start->setDate(ActivitySummaryService::presetStart(index, m_end->date()));
        }
        m_start->setEnabled(index == 5);
        m_end->setEnabled(index == 5);
        invalidatePreview();
    });
    connect(m_start, &QDateEdit::dateChanged, this, &ActivitySummaryDialog::invalidatePreview);
    connect(m_end, &QDateEdit::dateChanged, this, &ActivitySummaryDialog::invalidatePreview);
    connect(m_basis, &QComboBox::currentIndexChanged, this, &ActivitySummaryDialog::invalidatePreview);
    connect(m_language, &QComboBox::currentIndexChanged, this, &ActivitySummaryDialog::invalidatePreview);
    connect(m_preview, &QPushButton::clicked, this, &ActivitySummaryDialog::buildPreview);
    connect(m_generate, &QPushButton::clicked, this, &ActivitySummaryDialog::generate);
    connect(m_cancel, &QPushButton::clicked, this, &ActivitySummaryDialog::cancelRequest);
    connect(close, &QPushButton::clicked, this, &ActivitySummaryDialog::reject);
}

ActivitySummaryDialog::~ActivitySummaryDialog()
{
    cancelRequest();
}

void ActivitySummaryDialog::reject()
{
    cancelRequest();
    QDialog::reject();
}

void ActivitySummaryDialog::invalidatePreview()
{
    m_payload.clear();
    m_timestampAppendix.clear();
    m_reportHeader.clear();
    m_source->clear();
    m_generate->setEnabled(false);
    m_status->setText("Options changed. Preview source again before generating.");
}

void ActivitySummaryDialog::buildPreview()
{
    invalidatePreview();
    if (m_savePendingEdits && !m_savePendingEdits()) {
        m_status->setText("Could not save pending edits. Summary was not prepared or sent.");
        return;
    }
    QString error;
    const auto basis = static_cast<ActivityTimeBasis>(m_basis->currentData().toInt());
    m_previewLanguage = static_cast<ActivitySummaryLanguage>(m_language->currentData().toInt());
    if (!ActivitySummaryService::prepare(m_start->date(), m_end->date(), basis, m_payload, error,
                                        m_previewLanguage, &m_timestampAppendix)) {
        m_status->setText(error);
        return;
    }
    m_source->setPlainText(m_payload);
    const bool chinese = m_previewLanguage == ActivitySummaryLanguage::Chinese;
    const QString selection = !chinese ? m_basis->currentText()
        : basis == ActivityTimeBasis::Created ? QStringLiteral("创建时间")
        : basis == ActivityTimeBasis::Modified ? QStringLiteral("修改时间") : QStringLiteral("创建或修改时间");
    m_reportHeader = (chinese
        ? QStringLiteral("# 工作总结\n\n"
                         "- 本地日期范围：%1 至 %2（包含首尾日期）\n"
                         "- 筛选依据：%3\n"
                         "- 总结语言：中文\n\n"
                         "> 基于当前已保存的笔记及期间可用的事件，而非历史正文快照。"
                         "早期操作历史及永久删除的笔记可能缺失。\n\n")
        : QString("# Activity summary\n\n"
                            "- Selected local dates: %1 through %2 (inclusive)\n"
                            "- Selection: %3\n"
                            "- Summary language: English\n\n"
                            "> Based on current saved notes and available in-range events, not historical "
                            "content snapshots. Earlier history and permanently deleted notes may be missing.\n\n"))
                         .arg(m_start->date().toString(Qt::ISODate),
                              m_end->date().toString(Qt::ISODate), selection);
    m_generate->setEnabled(true);
    m_status->setText(QString("Local preview ready (%1 characters). This exact snapshot will be sent only after confirmation.")
                          .arg(m_payload.size()));
}

void ActivitySummaryDialog::generate()
{
    if (m_request || m_payload.isEmpty()) return;
    if (!AIService::instance().isConfigured()) {
        m_status->setText("Configure your AI endpoint, API key and model in Settings first. Nothing was sent.");
        return;
    }
    auto &db = DatabaseManager::instance();
    const QUrl endpoint(db.getSetting("ai_endpoint"));
    if (!endpoint.isValid() || endpoint.host().isEmpty()
        || (endpoint.scheme() != "http" && endpoint.scheme() != "https")) {
        m_status->setText("The configured AI endpoint must be a valid HTTP or HTTPS URL. Nothing was sent.");
        return;
    }
    const QString host = endpoint.host();
    if (QMessageBox::question(this, "Send activity to AI?",
            QString("Send the previewed task titles, note text, timestamps and statuses to your configured "
                    "AI service at %1 using model %2, with output in %3?\n\n"
                    "The material can contain private work information. Settings values and attachments "
                    "are not included. The generated summary will not modify your tasks.")
                .arg(host, db.getSetting("ai_model", "gpt-4o-mini"), m_language->currentText()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    setBusy(true);
    m_cancelled = false;
    m_status->setText("Generating summary... You can cancel the request.");
    const QPointer<ActivitySummaryDialog> guard(this);
    m_request = AIService::instance().summarizeActivity(m_payload,
        [guard](const QString &summary) {
            if (!guard) return;
            guard->m_request.clear();
            guard->setBusy(false);
            guard->m_status->setText("Summary generated. Review it before using it; AI can make mistakes.");
            if (!guard->m_result) guard->m_result = new MarkdownPreviewDialog(guard);
            guard->m_result->setWindowTitle("AI Activity Summary - Result");
            guard->m_result->setMarkdownContent(guard->m_reportHeader + summary + guard->m_timestampAppendix);
            guard->m_result->show();
            guard->m_result->raise();
        },
        [guard](const QString &error) {
            if (!guard) return;
            guard->m_request.clear();
            guard->setBusy(false);
            guard->m_status->setText(guard->m_cancelled
                ? "Request cancelled. No summary was generated or saved." : error);
        }, m_previewLanguage);
}

void ActivitySummaryDialog::setBusy(bool busy)
{
    m_period->setEnabled(!busy);
    m_basis->setEnabled(!busy);
    m_language->setEnabled(!busy);
    m_start->setEnabled(!busy && m_period->currentIndex() == 5);
    m_end->setEnabled(!busy && m_period->currentIndex() == 5);
    m_preview->setEnabled(!busy);
    m_generate->setEnabled(!busy && !m_payload.isEmpty());
    m_cancel->setEnabled(busy);
}

void ActivitySummaryDialog::cancelRequest()
{
    if (!m_request) return;
    m_cancelled = true;
    m_request->abort();
    m_request.clear();
}
