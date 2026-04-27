#include "SettingsDialog.h"
#include "db/DatabaseManager.h"
#include "services/AIService.h"
#include <memory>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

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

    // Database Settings Group
    auto *dbGroup = new QGroupBox("Database", this);
    auto *dbLayout = new QFormLayout(dbGroup);

    auto *dbPathLayout = new QHBoxLayout();
    m_dbPathEdit = new QLineEdit(this);
    m_dbPathEdit->setReadOnly(true);
    dbPathLayout->addWidget(m_dbPathEdit);
    auto *browseBtn = new QPushButton("Browse...", this);
    dbPathLayout->addWidget(browseBtn);
    dbLayout->addRow("Database Path:", dbPathLayout);

    QLabel *dbNote = new QLabel("Changing the path will move the database file to the new location.", this);
    dbNote->setStyleSheet("color: #7f8c8d; font-size: 11px;");
    dbLayout->addRow(dbNote);

    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseDbPath);

    mainLayout->addWidget(dbGroup);

    // AI Settings Group
    auto *aiGroup = new QGroupBox("AI Integration", this);
    auto *aiLayout = new QFormLayout(aiGroup);

    m_aiEndpoint = new QLineEdit(this);
    m_aiEndpoint->setPlaceholderText("https://{resource}.openai.azure.com/openai/deployments/{deploy}/chat/completions");
    aiLayout->addRow("API Endpoint:", m_aiEndpoint);

    m_aiApiKey = new QLineEdit(this);
    m_aiApiKey->setEchoMode(QLineEdit::Password);
    m_aiApiKey->setPlaceholderText("your-api-key");
    aiLayout->addRow("API Key:", m_aiApiKey);

    m_aiModel = new QLineEdit(this);
    m_aiModel->setPlaceholderText("gpt-4o-mini");
    aiLayout->addRow("Model:", m_aiModel);

    QLabel *aiNote = new QLabel("Supports Azure OpenAI and OpenAI-compatible endpoints. "
                                "Azure endpoints auto-detected by URL.", this);
    aiNote->setStyleSheet("color: #7f8c8d; font-size: 11px;");
    aiLayout->addRow(aiNote);

    m_verifyBtn = new QPushButton("Verify Connection", this);
    m_verifyBtn->setStyleSheet(
        "QPushButton { background-color: #27ae60; color: white; border: none; padding: 6px 16px; }"
        "QPushButton:hover { background-color: #2ecc71; }"
        "QPushButton:disabled { background-color: #95a5a6; }");
    aiLayout->addRow(m_verifyBtn);
    connect(m_verifyBtn, &QPushButton::clicked, this, &SettingsDialog::onVerifyAI);

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
    m_dbPathEdit->setText(db.currentDbPath());
    m_aiEndpoint->setText(db.getSetting("ai_endpoint", "https://api.openai.com/v1/chat/completions"));
    m_aiApiKey->setText(db.getSetting("ai_api_key"));
    m_aiModel->setText(db.getSetting("ai_model", "gpt-4o-mini"));
    m_hotkeyEdit->setText(db.getSetting("global_hotkey", "Ctrl+Shift+N"));
    m_checkUpdates->setChecked(db.getSetting("check_updates", "true") == "true");
}

void SettingsDialog::onSave()
{
    auto &db = DatabaseManager::instance();

    // Handle DB path change
    QString newDbPath = m_dbPathEdit->text().trimmed();
    if (!newDbPath.isEmpty() && newDbPath != db.currentDbPath()) {
        if (!db.moveDatabase(newDbPath)) {
            QMessageBox::warning(this, "Error", "Failed to move database to the new location.");
            return;
        }
        // Save the new path to QSettings (read before DB opens)
        QSettings settings;
        settings.setValue("db_path", newDbPath);
    }

    db.setSetting("ai_endpoint", m_aiEndpoint->text().trimmed());
    db.setSetting("ai_api_key", m_aiApiKey->text().trimmed());
    db.setSetting("ai_model", m_aiModel->text().trimmed());
    db.setSetting("global_hotkey", m_hotkeyEdit->text().trimmed());
    db.setSetting("check_updates", m_checkUpdates->isChecked() ? "true" : "false");
    accept();
}

void SettingsDialog::onBrowseDbPath()
{
    QString current = m_dbPathEdit->text();
    QString dir = QFileInfo(current).absolutePath();
    QString path = QFileDialog::getSaveFileName(
        this, "Choose Database Location", dir + "/nexus.db",
        "SQLite Database (*.db)", nullptr,
        QFileDialog::DontConfirmOverwrite);
    if (!path.isEmpty()) {
        m_dbPathEdit->setText(path);
    }
}

void SettingsDialog::onVerifyAI()
{
    QString endpoint = m_aiEndpoint->text().trimmed();
    QString apiKey = m_aiApiKey->text().trimmed();
    QString model = m_aiModel->text().trimmed();

    if (endpoint.isEmpty() || apiKey.isEmpty()) {
        QMessageBox::warning(this, "Verify", "Please enter both endpoint and API key.");
        return;
    }

    m_verifyBtn->setEnabled(false);
    m_verifyBtn->setText("Verifying...");

    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(&AIService::instance(), &AIService::verifyResult,
                    this, [this, conn](bool success, const QString &message) {
        disconnect(*conn);
        m_verifyBtn->setEnabled(true);
        m_verifyBtn->setText("Verify Connection");
        if (success) {
            QMessageBox::information(this, "Verify", message);
        } else {
            QMessageBox::warning(this, "Verify", message);
        }
    });

    AIService::instance().verifyConnection(endpoint, apiKey, model);
}
