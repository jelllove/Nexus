#include "SettingsDialog.h"
#include "db/DatabaseManager.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Nexus Settings");
    setMinimumWidth(500);
    setupUi();
    loadSettings();
}

void SettingsDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    // AI Settings Group
    auto *aiGroup = new QGroupBox("AI Integration", this);
    auto *aiLayout = new QFormLayout(aiGroup);

    m_aiEndpoint = new QLineEdit(this);
    m_aiEndpoint->setPlaceholderText("https://api.openai.com/v1/chat/completions");
    aiLayout->addRow("API Endpoint:", m_aiEndpoint);

    m_aiApiKey = new QLineEdit(this);
    m_aiApiKey->setEchoMode(QLineEdit::Password);
    m_aiApiKey->setPlaceholderText("sk-...");
    aiLayout->addRow("API Key:", m_aiApiKey);

    m_aiModel = new QLineEdit(this);
    m_aiModel->setPlaceholderText("gpt-4o-mini");
    aiLayout->addRow("Model:", m_aiModel);

    QLabel *aiNote = new QLabel("Supports OpenAI-compatible endpoints (OpenAI, Azure, local LLMs)", this);
    aiNote->setStyleSheet("color: #7f8c8d; font-size: 11px;");
    aiLayout->addRow(aiNote);

    mainLayout->addWidget(aiGroup);

    // Hotkey Settings Group
    auto *hotkeyGroup = new QGroupBox("Global Hotkey", this);
    auto *hotkeyLayout = new QFormLayout(hotkeyGroup);

    m_hotkeyEdit = new QLineEdit(this);
    m_hotkeyEdit->setPlaceholderText("Ctrl+Shift+N");
    m_hotkeyEdit->setReadOnly(true);
    hotkeyLayout->addRow("Show/Hide Hotkey:", m_hotkeyEdit);

    QLabel *hotkeyNote = new QLabel("Default: Ctrl+Shift+N", this);
    hotkeyNote->setStyleSheet("color: #7f8c8d; font-size: 11px;");
    hotkeyLayout->addRow(hotkeyNote);

    mainLayout->addWidget(hotkeyGroup);

    // Update Settings Group
    auto *updateGroup = new QGroupBox("Updates", this);
    auto *updateLayout = new QFormLayout(updateGroup);

    m_checkUpdates = new QCheckBox("Check for updates on startup", this);
    updateLayout->addRow(m_checkUpdates);

    mainLayout->addWidget(updateGroup);

    // Buttons
    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onSave);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SettingsDialog::loadSettings()
{
    auto &db = DatabaseManager::instance();
    m_aiEndpoint->setText(db.getSetting("ai_endpoint", "https://api.openai.com/v1/chat/completions"));
    m_aiApiKey->setText(db.getSetting("ai_api_key"));
    m_aiModel->setText(db.getSetting("ai_model", "gpt-4o-mini"));
    m_hotkeyEdit->setText(db.getSetting("global_hotkey", "Ctrl+Shift+N"));
    m_checkUpdates->setChecked(db.getSetting("check_updates", "true") == "true");
}

void SettingsDialog::onSave()
{
    auto &db = DatabaseManager::instance();
    db.setSetting("ai_endpoint", m_aiEndpoint->text().trimmed());
    db.setSetting("ai_api_key", m_aiApiKey->text().trimmed());
    db.setSetting("ai_model", m_aiModel->text().trimmed());
    db.setSetting("global_hotkey", m_hotkeyEdit->text().trimmed());
    db.setSetting("check_updates", m_checkUpdates->isChecked() ? "true" : "false");
    accept();
}
