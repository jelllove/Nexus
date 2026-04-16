#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void onSave();

private:
    void setupUi();
    void loadSettings();

    // AI settings
    QLineEdit *m_aiEndpoint;
    QLineEdit *m_aiApiKey;
    QLineEdit *m_aiModel;

    // Hotkey settings
    QLineEdit *m_hotkeyEdit;

    // Database settings
    QLineEdit *m_dbPathEdit;
};
