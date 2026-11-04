#include "TaskCreationDialog.h"
#include "db/DatabaseManager.h"
#include "services/AIService.h"
#include "services/TaskCreationService.h"
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkReply>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QUrl>
#include <QVBoxLayout>

namespace {

int selectedId(const QComboBox *combo)
{
    const int index = combo->currentIndex();
    if (index < 0 || combo->currentText().compare(combo->itemText(index), Qt::CaseInsensitive) != 0)
        return -1;
    return combo->currentData().toInt();
}

void makeSearchable(QComboBox *combo)
{
    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert);
    combo->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    combo->completer()->setFilterMode(Qt::MatchContains);
    combo->completer()->setCompletionMode(QCompleter::PopupCompletion);
}

}

TaskCreationDialog::TaskCreationDialog(QWidget *parent, std::function<bool()> savePendingEdits)
    : QDialog(parent), m_savePendingEdits(std::move(savePendingEdits))
{
    setObjectName("taskCreationDialog");
    setWindowTitle("AI Quick Task");
    setModal(false);
    resize(660, 380);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *body = new QWidget(scroll);
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(12);
    auto *heading = new QLabel("Describe what you want to do", body);
    auto font = heading->font();
    font.setPointSize(font.pointSize() + 2);
    font.setBold(true);
    heading->setFont(font);
    bodyLayout->addWidget(heading);
    m_input = new QPlainTextEdit(body);
    m_input->setObjectName("taskCreationInput");
    m_input->setAccessibleName("Task description");
    m_input->setPlaceholderText(
        "Write a note about the work. AI can suggest a project, a parent task, a title and context.\n"
        "Nothing is created until you review and choose Create.");
    m_input->setMinimumHeight(130);
    bodyLayout->addWidget(m_input, 1);
    auto *actions = new QHBoxLayout;
    m_analyze = new QPushButton("Analyze with AI...", body);
    m_analyze->setObjectName("taskCreationAnalyze");
    m_manual = new QPushButton("Manual setup", body);
    m_manual->setObjectName("taskCreationManual");
    m_cancelRequest = new QPushButton("Cancel request", body);
    m_cancelRequest->setObjectName("taskCreationCancelRequest");
    for (auto *button : {m_analyze, m_manual, m_cancelRequest}) {
        button->setAutoDefault(false);
        actions->addWidget(button);
    }
    actions->addStretch();
    bodyLayout->addLayout(actions);
    m_review = new QGroupBox("Review before creating", body);
    m_review->setObjectName("taskCreationReview");
    auto *reviewLayout = new QVBoxLayout(m_review);
    m_reason = new QLabel(m_review);
    m_reason->setObjectName("taskCreationReason");
    m_reason->setTextFormat(Qt::PlainText);
    m_reason->setWordWrap(true);
    reviewLayout->addWidget(m_reason);
    m_form = new QFormLayout;
    m_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_kind = new QComboBox(m_review);
    m_kind->setObjectName("taskCreationKind");
    m_kind->addItem("New main task", static_cast<int>(TaskCreationKind::NewTask));
    m_kind->addItem("Subtask of an existing task", static_cast<int>(TaskCreationKind::Subtask));
    m_kind->addItem("New project and main task", static_cast<int>(TaskCreationKind::NewProject));
    m_form->addRow("Create as:", m_kind);
    m_project = new QComboBox(m_review);
    m_project->setObjectName("taskCreationProject");
    makeSearchable(m_project);
    m_form->addRow("Project:", m_project);
    m_projectName = new QLineEdit(m_review);
    m_projectName->setObjectName("taskCreationProjectName");
    m_projectName->setMaxLength(TaskCreationDraft::MaximumProjectNameCharacters);
    m_form->addRow("New project:", m_projectName);
    m_parent = new QComboBox(m_review);
    m_parent->setObjectName("taskCreationParent");
    makeSearchable(m_parent);
    m_form->addRow("Parent task:", m_parent);
    m_title = new QLineEdit(m_review);
    m_title->setObjectName("taskCreationTitle");
    m_title->setMaxLength(TaskCreationDraft::MaximumTitleCharacters);
    m_form->addRow("Title:", m_title);
    m_context = new QPlainTextEdit(m_review);
    m_context->setObjectName("taskCreationContext");
    m_context->setMinimumHeight(100);
    m_form->addRow("Context:", m_context);
    m_priority = new QComboBox(m_review);
    m_priority->setObjectName("taskCreationPriority");
    for (auto priority : {TaskPriority::Critical, TaskPriority::High, TaskPriority::Medium, TaskPriority::Low})
        m_priority->addItem(Task::priorityToString(priority), static_cast<int>(priority));
    m_priority->setCurrentIndex(m_priority->findData(static_cast<int>(TaskPriority::High)));
    m_form->addRow("Priority:", m_priority);
    reviewLayout->addLayout(m_form);
    m_priorityNotice = new QLabel(
        "Subtasks have no separate priority. The parent's priority and status will not change.", m_review);
    m_priorityNotice->setWordWrap(true);
    reviewLayout->addWidget(m_priorityNotice);
    m_validation = new QLabel(m_review);
    m_validation->setTextFormat(Qt::PlainText);
    m_validation->setWordWrap(true);
    reviewLayout->addWidget(m_validation);
    bodyLayout->addWidget(m_review);
    m_review->hide();
    scroll->setWidget(body);
    layout->addWidget(scroll, 1);
    m_status = new QLabel("Analyze to get a recommendation, or choose Manual setup. You make the final decision.", this);
    m_status->setObjectName("taskCreationStatus");
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status);
    auto *buttons = new QDialogButtonBox(this);
    m_create = buttons->addButton("Create", QDialogButtonBox::ActionRole);
    m_create->setObjectName("taskCreationCreate");
    m_create->setAutoDefault(false);
    auto *close = buttons->addButton(QDialogButtonBox::Cancel);
    close->setAutoDefault(false);
    layout->addWidget(buttons);

    connect(m_input, &QPlainTextEdit::textChanged, this, [this]() {
        if (m_busy) cancelRequest();
        m_review->hide();
        m_status->setText(m_input->toPlainText().trimmed().size() > TaskCreationService::MaximumInputCharacters
            ? QString("AI input is limited to %1 characters. Shorten it or use Manual setup.")
                  .arg(TaskCreationService::MaximumInputCharacters)
            : "Input changed. Analyze again or use Manual setup; nothing has been created.");
        setBusy(false);
    });
    connect(m_kind, &QComboBox::currentIndexChanged, this, &TaskCreationDialog::updateMode);
    connect(m_project, &QComboBox::currentIndexChanged, this, [this]() { updateParents(); });
    connect(m_project, &QComboBox::editTextChanged, this, [this]() {
        if (selectedId(m_project) <= 0) updateParents();
        updateCreateButton();
    });
    connect(m_parent, &QComboBox::currentIndexChanged, this, &TaskCreationDialog::updateCreateButton);
    connect(m_parent, &QComboBox::editTextChanged, this, &TaskCreationDialog::updateCreateButton);
    connect(m_projectName, &QLineEdit::textChanged, this, &TaskCreationDialog::updateCreateButton);
    connect(m_title, &QLineEdit::textChanged, this, &TaskCreationDialog::updateCreateButton);
    connect(m_context, &QPlainTextEdit::textChanged, this, &TaskCreationDialog::updateCreateButton);
    connect(m_priority, &QComboBox::currentIndexChanged, this, &TaskCreationDialog::updateCreateButton);
    connect(m_analyze, &QPushButton::clicked, this, &TaskCreationDialog::analyze);
    connect(m_manual, &QPushButton::clicked, this, &TaskCreationDialog::useManualSetup);
    connect(m_cancelRequest, &QPushButton::clicked, this, &TaskCreationDialog::cancelRequest);
    connect(m_create, &QPushButton::clicked, this, &TaskCreationDialog::create);
    connect(close, &QPushButton::clicked, this, &TaskCreationDialog::reject);
    reloadCatalog();
    setBusy(false);
    m_input->setFocus();
}

TaskCreationDialog::~TaskCreationDialog()
{
    cancelRequest();
}

void TaskCreationDialog::reject()
{
    cancelRequest();
    QDialog::reject();
}

bool TaskCreationDialog::reloadCatalog()
{
    const int project = selectedId(m_project);
    const int parent = selectedId(m_parent);
    TaskCreationCatalog source;
    QString error;
    if (!DatabaseManager::instance().collectTaskCreationCatalog(source, error)) {
        m_catalogReady = false;
        m_status->setText(error);
        updateCreateButton();
        return false;
    }
    m_catalog = source;
    m_catalogReady = true;
    const QSignalBlocker blocker(m_project);
    m_project->clear();
    m_project->addItem("Choose a project...", -1);
    for (const auto &item : m_catalog.projects)
        m_project->addItem(item.name, item.id);
    m_project->setCurrentIndex(qMax(0, m_project->findData(project)));
    updateParents(parent);
    updateMode();
    return true;
}

void TaskCreationDialog::updateParents(int selectedParent)
{
    const QSignalBlocker blocker(m_parent);
    m_parent->clear();
    m_parent->addItem("Choose a parent task...", -1);
    const int project = selectedId(m_project);
    for (const auto &task : m_catalog.tasks) {
        if (task.productId == project)
            m_parent->addItem(QString("%1 (#%2)").arg(task.title).arg(task.id), task.id);
    }
    m_parent->setCurrentIndex(qMax(0, m_parent->findData(selectedParent)));
    updateCreateButton();
}

void TaskCreationDialog::updateMode()
{
    const auto kind = static_cast<TaskCreationKind>(m_kind->currentData().toInt());
    m_form->setRowVisible(m_project, kind != TaskCreationKind::NewProject);
    m_form->setRowVisible(m_projectName, kind == TaskCreationKind::NewProject);
    m_form->setRowVisible(m_parent, kind == TaskCreationKind::Subtask);
    m_form->setRowVisible(m_priority, kind != TaskCreationKind::Subtask);
    m_priorityNotice->setVisible(kind == TaskCreationKind::Subtask);
    updateCreateButton();
}

TaskCreationDraft TaskCreationDialog::currentDraft() const
{
    TaskCreationDraft draft;
    draft.kind = static_cast<TaskCreationKind>(m_kind->currentData().toInt());
    draft.projectId = draft.kind == TaskCreationKind::NewProject ? -1 : selectedId(m_project);
    draft.parentTaskId = draft.kind == TaskCreationKind::Subtask ? selectedId(m_parent) : -1;
    draft.projectName = draft.kind == TaskCreationKind::NewProject ? m_projectName->text() : QString();
    draft.title = m_title->text();
    draft.context = m_context->toPlainText();
    draft.priority = static_cast<TaskPriority>(m_priority->currentData().toInt());
    return draft;
}

void TaskCreationDialog::updateCreateButton()
{
    const QString validation = currentDraft().validationError();
    m_create->setEnabled(!m_busy && !m_created && m_catalogReady && !m_review->isHidden() && validation.isEmpty());
    m_validation->setText(validation.isEmpty()
        ? "Ready. Create will save only the choices and text shown above." : validation);
}

void TaskCreationDialog::setBusy(bool busy)
{
    m_busy = busy;
    m_input->setReadOnly(busy);
    const QString text = m_input->toPlainText().trimmed();
    m_analyze->setEnabled(!busy && !text.isEmpty() && text.size() <= TaskCreationService::MaximumInputCharacters);
    m_manual->setEnabled(!busy);
    m_cancelRequest->setVisible(busy);
    m_review->setEnabled(!busy);
    updateCreateButton();
}

void TaskCreationDialog::analyze()
{
    if (m_busy || m_created) return;
    auto &db = DatabaseManager::instance();
    if (!AIService::instance().isConfigured()) {
        m_status->setText("Configure the AI endpoint, API key and model in Settings, or choose Manual setup. Nothing was sent.");
        return;
    }
    const QUrl endpoint(db.getSetting("ai_endpoint"));
    if (!endpoint.isValid() || endpoint.host().isEmpty()
        || (endpoint.scheme() != "http" && endpoint.scheme() != "https")) {
        m_status->setText("The AI endpoint must be a valid HTTP or HTTPS URL. Nothing was sent.");
        return;
    }
    if (m_savePendingEdits && !m_savePendingEdits()) {
        m_status->setText("The current note could not be saved. Nothing was sent. Save it before analyzing.");
        return;
    }
    if (!reloadCatalog()) return;
    QString payload, error;
    if (!TaskCreationService::buildPayload(m_input->toPlainText(), m_catalog, payload, error)) {
        m_status->setText(error);
        return;
    }
    QMessageBox consent(QMessageBox::Question, "Send task matching data to AI?",
        QString("Send this input, %1 active project names, and %2 eligible main-task titles with bounded "
                "plain-text note excerpts to your configured AI at %3 using model %4?\n\n"
                "This may contain private work information. Settings and attachments are not included. "
                "If the source is too large, note excerpts are omitted, not destinations. "
                "Show Details to inspect the exact JSON. AI only recommends; nothing will be created yet.")
            .arg(m_catalog.projects.size()).arg(m_catalog.tasks.size())
            .arg(endpoint.host(), db.getSetting("ai_model", "gpt-4o-mini")),
        QMessageBox::Yes | QMessageBox::No, this);
    consent.setDefaultButton(QMessageBox::No);
    consent.setDetailedText(payload);
    if (consent.exec() != QMessageBox::Yes) {
        m_status->setText("Nothing was sent or created. You can continue editing or use Manual setup.");
        return;
    }
    m_review->hide();
    setBusy(true);
    m_status->setText("Matching projects and tasks... Cancel request keeps your input.");
    const quint64 generation = ++m_generation;
    const auto sourceCatalog = m_catalog;
    const QPointer<TaskCreationDialog> guard(this);
    m_request = AIService::instance().suggestTaskCreation(payload,
        [guard, generation, sourceCatalog](const QString &response) {
            if (!guard || guard->m_generation != generation) return;
            guard->m_request.clear();
            guard->setBusy(false);
            TaskCreationSuggestion suggestion;
            QString error;
            if (!TaskCreationService::parseSuggestion(response, sourceCatalog, suggestion, error)) {
                guard->m_status->setText(error + " Retry or choose Manual setup.");
                return;
            }
            guard->applyDraft(suggestion.draft,
                (suggestion.confident ? "AI recommendation: " : "AI is uncertain. Choose a destination manually. ")
                    + suggestion.reason);
            guard->m_status->setText("Review and edit every field below. Nothing is created until you choose Create.");
        },
        [guard, generation](const QString &error) {
            if (!guard || guard->m_generation != generation) return;
            guard->m_request.clear();
            guard->setBusy(false);
            guard->m_status->setText(error + " Retry or choose Manual setup. Your input is unchanged.");
        });
}

void TaskCreationDialog::useManualSetup()
{
    if (m_busy || m_created || !reloadCatalog()) return;
    TaskCreationDraft draft;
    const QString input = m_input->toPlainText().trimmed();
    draft.title = input.section('\n', 0, 0).simplified().left(TaskCreationDraft::MaximumTitleCharacters);
    draft.context = input;
    if (m_catalog.projects.isEmpty()) draft.kind = TaskCreationKind::NewProject;
    applyDraft(draft, "Manual setup. Choose where to create the task and edit its title and context.");
    m_status->setText("Manual setup does not send anything to AI. Review the fields, then choose Create.");
}

void TaskCreationDialog::applyDraft(const TaskCreationDraft &draft, const QString &reason)
{
    const QSignalBlocker kindBlocker(m_kind);
    const QSignalBlocker projectBlocker(m_project);
    m_kind->setCurrentIndex(m_kind->findData(static_cast<int>(draft.kind)));
    m_project->setCurrentIndex(qMax(0, m_project->findData(draft.projectId)));
    m_projectName->setText(draft.projectName);
    m_title->setText(draft.title);
    m_context->setPlainText(draft.context);
    m_priority->setCurrentIndex(m_priority->findData(static_cast<int>(draft.priority)));
    m_reason->setText(reason);
    updateParents(draft.parentTaskId);
    updateMode();
    m_review->show();
    updateCreateButton();
    resize(width(), qMin(760, screen()->availableGeometry().height() - 60));
}

void TaskCreationDialog::create()
{
    if (m_busy || m_created || m_review->isHidden()) return;
    const auto draft = currentDraft();
    const QString validation = draft.validationError();
    if (!validation.isEmpty()) {
        m_status->setText(validation + " Nothing was created.");
        return;
    }
    if (m_savePendingEdits && !m_savePendingEdits()) {
        m_status->setText("The current note could not be saved. Nothing was created; your draft is unchanged.");
        return;
    }
    TaskCreationResult result;
    QString error;
    if (!DatabaseManager::instance().createTaskFromDraft(draft, result, error)) {
        reloadCatalog();
        m_status->setText("The task could not be created: " + error + " Your draft is still editable.");
        return;
    }
    m_created = true;
    m_create->setEnabled(false);
    emit taskCreated(result.projectId, result.taskId, result.subtaskId);
    QDialog::accept();
}

void TaskCreationDialog::cancelRequest()
{
    if (!m_request) return;
    ++m_generation;
    const auto request = m_request;
    m_request.clear();
    request->abort();
    setBusy(false);
    m_status->setText("Request cancelled. Nothing was created; your input is unchanged.");
}
