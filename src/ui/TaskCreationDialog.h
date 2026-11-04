#pragma once

#include "models/TaskCreation.h"
#include <QDialog>
#include <QPointer>
#include <functional>

class QComboBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QNetworkReply;
class QPlainTextEdit;
class QPushButton;

class TaskCreationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TaskCreationDialog(QWidget *parent = nullptr,
                                std::function<bool()> savePendingEdits = {});
    ~TaskCreationDialog() override;
    void reject() override;

signals:
    void taskCreated(int projectId, int taskId, int subtaskId);

private:
    bool reloadCatalog();
    void updateParents(int selectedParent = -1);
    void updateMode();
    void updateCreateButton();
    void setBusy(bool busy);
    void analyze();
    void useManualSetup();
    void applyDraft(const TaskCreationDraft &draft, const QString &reason);
    TaskCreationDraft currentDraft() const;
    void create();
    void cancelRequest();

    QPlainTextEdit *m_input;
    QPushButton *m_analyze;
    QPushButton *m_manual;
    QPushButton *m_cancelRequest;
    QGroupBox *m_review;
    QFormLayout *m_form;
    QComboBox *m_kind;
    QComboBox *m_project;
    QLineEdit *m_projectName;
    QComboBox *m_parent;
    QComboBox *m_priority;
    QLineEdit *m_title;
    QPlainTextEdit *m_context;
    QLabel *m_reason;
    QLabel *m_validation;
    QLabel *m_priorityNotice;
    QLabel *m_status;
    QPushButton *m_create;
    TaskCreationCatalog m_catalog;
    std::function<bool()> m_savePendingEdits;
    QPointer<QNetworkReply> m_request;
    quint64 m_generation = 0;
    bool m_busy = false;
    bool m_catalogReady = false;
    bool m_created = false;
};
