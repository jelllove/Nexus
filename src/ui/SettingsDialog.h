#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QFileInfo>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void onSave();
    void onBrowseDbPath();

private:
    void setupUi();
    void loadSettings();

    // Database settings
    QLineEdit *m_dbPathEdit;

    // AI settings
    QLineEdit *m_aiEndpoint;
    QLineEdit *m_aiApiKey;
    QLineEdit *m_aiModel;

    // Hotkey settings
    QLineEdit *m_hotkeyEdit;

    // Update settings
    QCheckBox *m_checkUpdates;
};
