#include "app/MainWindow.h"
#include "db/DatabaseManager.h"
#include "services/AIService.h"
#include "services/TaskCreationService.h"
#include "services/TaskExportService.h"
#include "ui/TaskCreationDialog.h"
#include "FakeAiServer.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimer>
#include <QtTest>

class TaskCreationTest : public QObject
{
    Q_OBJECT
    int m_project = -1;
    int m_task = -1;

    int count(const QString &table)
    {
        QSqlQuery query;
        if (!query.exec("SELECT COUNT(*) FROM " + table) || !query.next()) {
            QTest::qFail(qPrintable(query.lastError().text()), __FILE__, __LINE__);
            return -1;
        }
        return query.value(0).toInt();
    }

    TaskCreationCatalog catalog()
    {
        TaskCreationCatalog result;
        QString error;
        if (!DatabaseManager::instance().collectTaskCreationCatalog(result, error))
            QTest::qFail(qPrintable(error), __FILE__, __LINE__);
        return result;
    }

    QJsonObject suggestion(const QString &kind = "subtask")
    {
        return {
            {"kind", kind},
            {"projectId", kind == "project" ? QJsonValue(QJsonValue::Null) : QJsonValue(m_project)},
            {"parentTaskId", kind == "subtask" ? QJsonValue(m_task) : QJsonValue(QJsonValue::Null)},
            {"projectName", kind == "project" ? "New project" : ""},
            {"title", "Improve task capture"},
            {"context", "Add a shortcut.\nKeep final confirmation."},
            {"reason", "Related to the existing capture task."},
            {"confident", true}
        };
    }

    QString json(const QJsonObject &object)
    {
        return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    }

    void configureAi(FakeAiServer &server, const QJsonObject &result)
    {
        QVERIFY(server.listen(QHostAddress::LocalHost));
        auto &db = DatabaseManager::instance();
        QVERIFY(db.setSetting("ai_endpoint", QString("http://127.0.0.1:%1/chat").arg(server.serverPort())));
        QVERIFY(db.setSetting("ai_api_key", "synthetic-test-key"));
        QVERIFY(db.setSetting("ai_model", "configured-test-model"));
        server.response = json({{"choices", QJsonArray{QJsonObject{
            {"message", QJsonObject{{"content", json(result)}}}}}}});
    }

    void answerConsent(QMessageBox::StandardButton answer)
    {
        QTimer::singleShot(0, this, [answer]() {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            QCOMPARE(box->defaultButton(), box->button(QMessageBox::No));
            QVERIFY(box->detailedText().contains("projects"));
            box->button(answer)->click();
        });
    }

    void analyze(TaskCreationDialog &dialog)
    {
        dialog.show();
        dialog.findChild<QPlainTextEdit *>("taskCreationInput")->setPlainText("Improve task capture");
        answerConsent(QMessageBox::Yes);
        dialog.findChild<QPushButton *>("taskCreationAnalyze")->click();
    }

    void manual(TaskCreationDialog &dialog)
    {
        dialog.show();
        dialog.findChild<QPlainTextEdit *>("taskCreationInput")->setPlainText("A manual task\nWith context.");
        dialog.findChild<QPushButton *>("taskCreationManual")->click();
    }

    TaskCreationDraft draft(TaskCreationKind kind = TaskCreationKind::NewTask)
    {
        TaskCreationDraft result;
        result.kind = kind;
        result.projectId = kind == TaskCreationKind::NewProject ? -1 : m_project;
        result.parentTaskId = kind == TaskCreationKind::Subtask ? m_task : -1;
        result.projectName = kind == TaskCreationKind::NewProject ? "A new project" : "";
        result.title = "A confirmed task";
        result.context = "Keep <script>literal text</script> & details.\nSecond line.";
        return result;
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setApplicationName("NexusTaskCreationTest");
        QCoreApplication::setOrganizationName("NexusTaskCreationTest");
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        auto &db = DatabaseManager::instance();
        QVERIFY(db.initialize(":memory:"));
        QVERIFY(db.setSetting("check_updates", "false"));
        m_project = db.addProduct("Nexus");
        m_task = db.addTask(m_project, "Task capture", TaskPriority::Medium);
        QVERIFY(m_project > 0 && m_task > 0);
    }

    void assistantHasButtonAndShortcut()
    {
        MainWindow window;
        auto *button = window.findChild<QPushButton *>("taskCreationButton");
        QVERIFY2(button, "The main window must offer AI quick task creation without selecting a project.");
        auto *action = window.findChild<QAction *>("taskCreationAction");
        QVERIFY(action);
        QCOMPARE(action->shortcut(), QKeySequence("Ctrl+Alt+N"));
        QCOMPARE(action->shortcutContext(), Qt::ApplicationShortcut);
        window.show();
        button->click();
        auto *dialog = window.findChild<TaskCreationDialog *>();
        QVERIFY(dialog);
        QVERIFY(dialog->isVisible());
        QVERIFY(!dialog->isModal());
        dialog->findChild<QPlainTextEdit *>("taskCreationInput")->setPlainText("Keep this draft");
        action->trigger();
        QCOMPARE(window.findChildren<TaskCreationDialog *>().size(), 1);
        QCOMPARE(dialog->findChild<QPlainTextEdit *>("taskCreationInput")->toPlainText(), QString("Keep this draft"));
        dialog->reject();
    }

    void keyboardShortcutOpensAssistant()
    {
        MainWindow window;
        window.show();
        window.activateWindow();
        QTRY_VERIFY(window.isActiveWindow());
        QTest::keyClick(&window, Qt::Key_N, Qt::ControlModifier | Qt::AltModifier);
        QTRY_VERIFY(window.findChild<TaskCreationDialog *>());
        window.findChild<TaskCreationDialog *>()->reject();
    }

    void creationRevealsNoteFromOtherViews_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<int>("view");
        QTest::newRow("main-from-archived") << static_cast<int>(TaskCreationKind::NewTask)
                                          << static_cast<int>(TaskPane::ViewMode::Archived);
        QTest::newRow("subtask-from-deleted") << static_cast<int>(TaskCreationKind::Subtask)
                                            << static_cast<int>(TaskPane::ViewMode::Deleted);
        QTest::newRow("project-from-search") << static_cast<int>(TaskCreationKind::NewProject)
                                           << static_cast<int>(TaskPane::ViewMode::SearchResults);
    }

    void creationRevealsNoteFromOtherViews()
    {
        QFETCH(int, kind);
        QFETCH(int, view);
        MainWindow window;
        window.show();
        auto *products = window.findChild<ProductPane *>();
        auto *tasks = window.findChild<TaskPane *>();
        QVERIFY(products->selectProductById(m_project));
        if (view == static_cast<int>(TaskPane::ViewMode::SearchResults)) {
            QVERIFY(QMetaObject::invokeMethod(&window, "onSearchRequested", Q_ARG(QString, QString("Task"))));
        } else {
            tasks->restoreView(m_project, static_cast<TaskPane::ViewMode>(view), -1);
        }
        window.findChild<QPushButton *>("taskCreationButton")->click();
        auto *dialog = window.findChild<TaskCreationDialog *>();
        QVERIFY(dialog);
        manual(*dialog);
        QSignalSpy created(dialog, &TaskCreationDialog::taskCreated);
        auto *kindCombo = dialog->findChild<QComboBox *>("taskCreationKind");
        kindCombo->setCurrentIndex(kindCombo->findData(kind));
        if (kind == static_cast<int>(TaskCreationKind::NewProject)) {
            dialog->findChild<QLineEdit *>("taskCreationProjectName")->setText("A project from capture");
        } else {
            auto *project = dialog->findChild<QComboBox *>("taskCreationProject");
            project->setCurrentIndex(project->findData(m_project));
            if (kind == static_cast<int>(TaskCreationKind::Subtask)) {
                auto *parent = dialog->findChild<QComboBox *>("taskCreationParent");
                parent->setCurrentIndex(parent->findData(m_task));
            }
        }
        auto *createButton = dialog->findChild<QPushButton *>("taskCreationCreate");
        QVERIFY(createButton->isEnabled());
        createButton->click();
        QCOMPARE(created.size(), 1);
        const auto ids = created[0];
        QCOMPARE(products->selectedProductId(), ids[0].toInt());
        QCOMPARE(tasks->viewMode(), TaskPane::ViewMode::Active);
        QVERIFY(window.findChild<SearchBar *>()->searchText().isEmpty());
        auto *editor = window.findChild<EditorPane *>();
        if (kind == static_cast<int>(TaskCreationKind::Subtask)) {
            QCOMPARE(editor->currentSubTaskId(), ids[2].toInt());
            QVERIFY(tasks->findChild<TaskListModel *>()->rowForSubTaskId(ids[2].toInt()) >= 0);
        } else {
            QCOMPARE(editor->currentTaskId(), ids[1].toInt());
            QCOMPARE(tasks->selectedTaskId(), ids[1].toInt());
        }
    }

    void catalogExcludesIneligibleDestinations()
    {
        auto &db = DatabaseManager::instance();
        const int archivedProject = db.addProduct("Archived project");
        db.addTask(archivedProject, "Task in archived project");
        QVERIFY(db.archiveProduct(archivedProject));
        const int archived = db.addTask(m_project, "Archived task");
        QVERIFY(db.archiveTask(archived));
        const int deleted = db.addTask(m_project, "Deleted task");
        QVERIFY(db.deleteTask(deleted));
        const int completed = db.addTask(m_project, "Completed task");
        QVERIFY(db.updateTaskWorkStatus(completed, TaskWorkStatus::Completed));
        const auto source = catalog();
        QCOMPARE(source.projects.size(), 1);
        QCOMPARE(source.projects[0].id, m_project);
        QCOMPARE(source.tasks.size(), 1);
        QCOMPARE(source.tasks[0].id, m_task);
    }

    void catalogReadFailureIsReported()
    {
        QSqlQuery query;
        QVERIFY(query.exec("ALTER TABLE tasks RENAME TO unavailable_tasks"));
        TaskCreationCatalog source;
        source.projects.append(Product{});
        QString error;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Failed to collect task creation catalog:"));
        QVERIFY(!DatabaseManager::instance().collectTaskCreationCatalog(source, error));
        QVERIFY(!error.isEmpty());
        QVERIFY(source.projects.isEmpty());
        QVERIFY(source.tasks.isEmpty());
    }

    void payloadIsBoundedPlainTextWithoutSettings()
    {
        auto &db = DatabaseManager::instance();
        QVERIFY(db.setSetting("ai_api_key", "never-send-this-setting"));
        QVERIFY(db.updateTaskContent(m_task, "<p>Capture &amp; review</p><img src=\"file:///private.png\">"));
        QString payload, error;
        QVERIFY2(TaskCreationService::buildPayload("Add capture", catalog(), payload, error), qPrintable(error));
        const auto object = QJsonDocument::fromJson(payload.toUtf8()).object();
        QCOMPARE(object["input"].toString(), QString("Add capture"));
        QCOMPARE(object["projects"].toArray()[0].toObject()["id"].toInt(), m_project);
        const auto task = object["tasks"].toArray()[0].toObject();
        QCOMPARE(task["id"].toInt(), m_task);
        QVERIFY(task["context"].toString().contains("Capture & review"));
        QVERIFY(!payload.contains("<img"));
        QVERIFY(!payload.contains("private.png"));
        QVERIFY(!payload.contains("never-send-this-setting"));
        QVERIFY(payload.size() <= TaskCreationService::MaximumPayloadCharacters);
    }

    void payloadRetainsEveryDestinationWhenNotesAreOmitted()
    {
        auto source = catalog();
        source.tasks.clear();
        for (int i = 0; i < 180; ++i) {
            Task task;
            task.id = i + 1;
            task.productId = m_project;
            task.title = QString("Task %1").arg(i);
            task.content = "<p>" + QString(1000, 'x') + "</p>";
            source.tasks.append(task);
        }
        QString payload, error;
        QVERIFY2(TaskCreationService::buildPayload("Capture", source, payload, error), qPrintable(error));
        const auto object = QJsonDocument::fromJson(payload.toUtf8()).object();
        QCOMPARE(object["tasks"].toArray().size(), source.tasks.size());
        QCOMPARE(object["notesIncluded"].toBool(), false);
        QVERIFY(payload.size() <= TaskCreationService::MaximumPayloadCharacters);
    }

    void payloadRejectsInvalidInputAndOversizedCatalog()
    {
        QString payload = "old payload", error;
        QVERIFY(!TaskCreationService::buildPayload("  ", catalog(), payload, error));
        QVERIFY(payload.isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(!TaskCreationService::buildPayload(
            QString(TaskCreationService::MaximumInputCharacters + 1, 'x'), catalog(), payload, error));
        auto source = catalog();
        source.projects[0].name = QString(TaskCreationService::MaximumPayloadCharacters + 1, 'x');
        QVERIFY(!TaskCreationService::buildPayload("Capture", source, payload, error));
        QVERIFY(payload.isEmpty());
        QVERIFY(error.contains("manual", Qt::CaseInsensitive));
    }

    void payloadAcceptsExactInputAndSourceLimits()
    {
        QString payload, error;
        QVERIFY(TaskCreationService::buildPayload(
            QString(TaskCreationService::MaximumInputCharacters, 'x'), catalog(), payload, error));
        auto source = catalog();
        source.tasks.clear();
        source.projects[0].name = "x";
        QVERIFY(TaskCreationService::buildPayload("Capture", source, payload, error));
        source.projects[0].name = QString(TaskCreationService::MaximumPayloadCharacters - payload.size() + 1, 'x');
        QVERIFY2(TaskCreationService::buildPayload("Capture", source, payload, error), qPrintable(error));
        QCOMPARE(payload.size(), TaskCreationService::MaximumPayloadCharacters);
        source.projects[0].name += "x";
        QVERIFY(!TaskCreationService::buildPayload("Capture", source, payload, error));
        QVERIFY(payload.isEmpty());
    }

    void parsesAllCreationKinds_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::newRow("subtask") << "subtask";
        QTest::newRow("task") << "task";
        QTest::newRow("project") << "project";
    }

    void parsesAllCreationKinds()
    {
        QFETCH(QString, kind);
        TaskCreationSuggestion result;
        QString error;
        QVERIFY2(TaskCreationService::parseSuggestion(json(suggestion(kind)), catalog(), result, error), qPrintable(error));
        QCOMPARE(result.draft.kind, kind == "subtask" ? TaskCreationKind::Subtask
                                    : kind == "project" ? TaskCreationKind::NewProject : TaskCreationKind::NewTask);
        QCOMPARE(result.draft.title, QString("Improve task capture"));
        QCOMPARE(result.draft.priority, TaskPriority::High);
        QVERIFY(!result.reason.isEmpty());
        QVERIFY(result.confident);
        QVERIFY(TaskCreationService::parseSuggestion("```json\n" + json(suggestion(kind)) + "\n```",
                                                     catalog(), result, error));
    }

    void uncertainSuggestionsRequireManualSelection()
    {
        auto object = suggestion();
        object["confident"] = false;
        TaskCreationSuggestion result;
        QString error;
        QVERIFY(TaskCreationService::parseSuggestion(json(object), catalog(), result, error));
        QVERIFY(!result.confident);
        QCOMPARE(result.draft.projectId, -1);
        QCOMPARE(result.draft.parentTaskId, -1);
        QCOMPARE(result.draft.kind, TaskCreationKind::NewTask);
        QVERIFY(!result.draft.title.isEmpty());
    }

    void rejectsMalformedAndInventedSuggestions()
    {
        const auto source = catalog();
        QList<QJsonObject> invalid;
        auto object = suggestion();
        object["projectId"] = 999999;
        invalid.append(object);
        object = suggestion();
        object["parentTaskId"] = 999999;
        invalid.append(object);
        object = suggestion();
        object["projectId"] = 1.5;
        invalid.append(object);
        object = suggestion();
        object["parentTaskId"] = QString::number(m_task);
        invalid.append(object);
        object = suggestion();
        object["title"] = " ";
        invalid.append(object);
        object = suggestion();
        object.remove("context");
        invalid.append(object);
        object = suggestion();
        object["context"] = QString(TaskCreationDraft::MaximumContextCharacters + 1, 'x');
        invalid.append(object);
        object = suggestion();
        object["confident"] = "yes";
        invalid.append(object);
        object = suggestion();
        object["kind"] = "delete";
        invalid.append(object);
        object = suggestion("task");
        object["parentTaskId"] = m_task;
        invalid.append(object);
        const int otherProject = DatabaseManager::instance().addProduct("Other project");
        const int otherTask = DatabaseManager::instance().addTask(otherProject, "Other task");
        object = suggestion();
        object["parentTaskId"] = otherTask;
        invalid.append(object);
        for (const auto &candidate : invalid) {
            TaskCreationSuggestion result;
            result.draft.title = "Stale";
            QString error;
            QVERIFY2(!TaskCreationService::parseSuggestion(json(candidate), source, result, error),
                     qPrintable(json(candidate)));
            QVERIFY(!error.isEmpty());
            QVERIFY(result.draft.title.isEmpty());
        }
        TaskCreationSuggestion result;
        QString error;
        QVERIFY(!TaskCreationService::parseSuggestion("not JSON", source, result, error));
    }

    void createsMainTaskWithEscapedContextAndHighPriority()
    {
        auto &db = DatabaseManager::instance();
        TaskCreationResult result;
        QString error;
        const auto input = draft();
        QSignalSpy created(&db, &DatabaseManager::taskAdded);
        QVERIFY2(db.createTaskFromDraft(input, result, error), qPrintable(error));
        const auto task = db.getTask(result.taskId);
        QCOMPARE(task.productId, m_project);
        QCOMPARE(task.title, input.title);
        QCOMPARE(task.priority, TaskPriority::High);
        QCOMPARE(task.workStatus, TaskWorkStatus::NotStarted);
        QVERIFY(task.content.contains("&lt;script&gt;literal text&lt;/script&gt;"));
        QVERIFY(!task.content.contains("<script>"));
        QVERIFY(TaskExportService::extractPlainText(task.content).contains("Second line."));
        QCOMPARE(result.subtaskId, -1);
        QCOMPARE(created.size(), 1);
        QVERIFY(!db.searchTasks("literal").isEmpty());
    }

    void createsSubtaskWithoutChangingParent()
    {
        auto &db = DatabaseManager::instance();
        const int existing = db.addSubtask(m_task, "Existing child");
        const auto parent = db.getTask(m_task);
        TaskCreationResult result;
        QString error;
        QSignalSpy updated(&db, &DatabaseManager::taskUpdated);
        QVERIFY2(db.createTaskFromDraft(draft(TaskCreationKind::Subtask), result, error), qPrintable(error));
        const auto child = db.getSubtask(result.subtaskId);
        QCOMPARE(child.taskId, m_task);
        QVERIFY(child.sortOrder > db.getSubtask(existing).sortOrder);
        QVERIFY(child.content.contains("&lt;script&gt;"));
        QCOMPARE(child.workStatus, TaskWorkStatus::NotStarted);
        QCOMPARE(db.getTask(m_task).content, parent.content);
        QCOMPARE(db.getTask(m_task).priority, parent.priority);
        QCOMPARE(db.getTask(m_task).workStatus, parent.workStatus);
        QCOMPARE(result.taskId, m_task);
        QCOMPARE(updated.size(), 1);
    }

    void createsProjectAndTaskBeforeEmittingNotifications()
    {
        auto &db = DatabaseManager::instance();
        TaskCreationResult result;
        QString error;
        bool observedCompleteTask = false;
        const auto connection = connect(&db, &DatabaseManager::productAdded, this, [&](int id) {
            const auto tasks = db.getTasksForProduct(id);
            observedCompleteTask = tasks.size() == 1 && !tasks[0].content.isEmpty();
        });
        QVERIFY2(db.createTaskFromDraft(draft(TaskCreationKind::NewProject), result, error), qPrintable(error));
        disconnect(connection);
        QVERIFY(observedCompleteTask);
        QCOMPARE(db.getProduct(result.projectId).name, QString("A new project"));
        QCOMPARE(db.getTask(result.taskId).productId, result.projectId);
    }

    void rejectsChangedDestinations_data()
    {
        QTest::addColumn<QString>("change");
        for (const auto &name : {"archived-project", "archived-task", "deleted-task", "completed-task", "missing-task", "wrong-project"})
            QTest::newRow(name) << QString(name);
    }

    void rejectsChangedDestinations()
    {
        QFETCH(QString, change);
        auto &db = DatabaseManager::instance();
        auto input = draft(TaskCreationKind::Subtask);
        if (change == "archived-project") QVERIFY(db.archiveProduct(m_project));
        else if (change == "archived-task") QVERIFY(db.archiveTask(m_task));
        else if (change == "deleted-task") QVERIFY(db.deleteTask(m_task));
        else if (change == "completed-task") QVERIFY(db.updateTaskWorkStatus(m_task, TaskWorkStatus::Completed));
        else if (change == "missing-task") input.parentTaskId = 999999;
        else input.projectId = db.addProduct("Other project");
        const int writes = count("database_operations");
        TaskCreationResult result;
        QString error;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Failed to create task from draft:"));
        QVERIFY(!db.createTaskFromDraft(input, result, error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(result.taskId, -1);
        QCOMPARE(count("subtasks"), 0);
        QCOMPARE(count("database_operations"), writes);
    }

    void failedInsertRollsBackNewProjectAndAudit()
    {
        auto &db = DatabaseManager::instance();
        QSqlQuery query;
        QVERIFY(query.exec("CREATE TEMP TRIGGER fail_task_creation BEFORE INSERT ON tasks "
                           "BEGIN SELECT RAISE(FAIL, 'synthetic insertion failure'); END"));
        const int products = count("products");
        const int writes = count("database_operations");
        QSignalSpy added(&db, &DatabaseManager::productAdded);
        TaskCreationResult result;
        QString error;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Failed to create task from draft:"));
        QVERIFY(!db.createTaskFromDraft(draft(TaskCreationKind::NewProject), result, error));
        QVERIFY(error.contains("synthetic insertion failure"));
        QCOMPARE(count("products"), products);
        QCOMPARE(count("database_operations"), writes);
        QCOMPARE(added.size(), 0);
        QCOMPARE(result.projectId, -1);
    }

    void failedCommitRollsBackAllRecords()
    {
        auto &db = DatabaseManager::instance();
        QSqlQuery query;
        QVERIFY(query.exec("CREATE TEMP TRIGGER fail_task_creation AFTER INSERT ON tasks "
                           "BEGIN INSERT INTO subtasks (task_id, title) VALUES (-1, 'invalid parent'); END"));
        QVERIFY(query.exec("PRAGMA defer_foreign_keys = ON"));
        const int writes = count("database_operations");
        TaskCreationResult result;
        QString error;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Failed to create task from draft:"));
        QVERIFY(!db.createTaskFromDraft(draft(TaskCreationKind::NewProject), result, error));
        QCOMPARE(count("products"), 1);
        QCOMPARE(count("tasks"), 1);
        QCOMPARE(count("subtasks"), 0);
        QCOMPARE(count("database_operations"), writes);
        auto connection = QSqlDatabase::database();
        QVERIFY(connection.transaction());
        QVERIFY(connection.rollback());
    }

    void preservesCallerTransaction()
    {
        auto &db = DatabaseManager::instance();
        auto connection = QSqlDatabase::database();
        QVERIFY(connection.transaction());
        QVERIFY(db.setSetting("uncommitted", "keep ownership"));
        TaskCreationResult result;
        QString error;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Failed to create task from draft:"));
        const bool created = db.createTaskFromDraft(draft(), result, error);
        const bool rolledBack = connection.rollback();
        QVERIFY(!created);
        QVERIFY(rolledBack);
        QCOMPARE(db.getSetting("uncommitted", "missing"), QString("missing"));
        QCOMPARE(count("tasks"), 1);
    }

    void rejectsDuplicateProjectAndEmptyFields()
    {
        auto &db = DatabaseManager::instance();
        auto input = draft(TaskCreationKind::NewProject);
        input.projectName = "  nexus  ";
        TaskCreationResult result;
        QString error;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Failed to create task from draft:"));
        QVERIFY(!db.createTaskFromDraft(input, result, error));
        QVERIFY(error.contains("already exists"));
        input = draft();
        input.context = " ";
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Failed to create task from draft:"));
        QVERIFY(!db.createTaskFromDraft(input, result, error));
        QCOMPARE(count("tasks"), 1);
    }

    void fieldLimitsAreInclusiveAndEnforcedBeforeWrites()
    {
        auto &db = DatabaseManager::instance();
        auto input = draft();
        input.title = QString(TaskCreationDraft::MaximumTitleCharacters, 't');
        input.context = QString(TaskCreationDraft::MaximumContextCharacters, 'c');
        TaskCreationResult result;
        QString error;
        QVERIFY2(db.createTaskFromDraft(input, result, error), qPrintable(error));
        QCOMPARE(db.getTask(result.taskId).title.size(), TaskCreationDraft::MaximumTitleCharacters);
        const int writes = count("database_operations");
        input.title += "t";
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Failed to create task from draft:"));
        QVERIFY(!db.createTaskFromDraft(input, result, error));
        input.title.chop(1);
        input.context += "c";
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Failed to create task from draft:"));
        QVERIFY(!db.createTaskFromDraft(input, result, error));
        QCOMPARE(count("database_operations"), writes);
        input = draft(TaskCreationKind::NewProject);
        input.projectName = QString(TaskCreationDraft::MaximumProjectNameCharacters, 'p');
        QVERIFY2(db.createTaskFromDraft(input, result, error), qPrintable(error));
        QCOMPARE(db.getProduct(result.projectId).name.size(), TaskCreationDraft::MaximumProjectNameCharacters);
        input.projectName += "p";
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Failed to create task from draft:"));
        QVERIFY(!db.createTaskFromDraft(input, result, error));
        QCOMPARE(count("products"), 2);
    }

    void aiRecommendationWaitsForFinalConfirmation()
    {
        FakeAiServer server;
        configureAi(server, suggestion());
        TaskCreationDialog dialog;
        QSignalSpy created(&dialog, &TaskCreationDialog::taskCreated);
        analyze(dialog);
        auto *review = dialog.findChild<QGroupBox *>("taskCreationReview");
        QTRY_VERIFY(review->isVisible());
        QCOMPARE(count("tasks"), 1);
        QCOMPARE(count("subtasks"), 0);
        QCOMPARE(created.size(), 0);
        QCOMPARE(dialog.findChild<QComboBox *>("taskCreationKind")->currentData().toInt(),
                 static_cast<int>(TaskCreationKind::Subtask));
        QCOMPARE(dialog.findChild<QComboBox *>("taskCreationProject")->currentData().toInt(), m_project);
        QCOMPARE(dialog.findChild<QComboBox *>("taskCreationParent")->currentData().toInt(), m_task);
        const auto request = QJsonDocument::fromJson(server.body).object();
        QCOMPARE(request["model"].toString(), QString("configured-test-model"));
        QVERIFY(request["messages"].toArray()[0].toObject()["content"].toString().contains("untrusted"));
        dialog.findChild<QPushButton *>("taskCreationCreate")->click();
        QCOMPARE(created.size(), 1);
        QCOMPARE(count("subtasks"), 1);
        QCOMPARE(dialog.result(), QDialog::Accepted);
    }

    void recommendationFieldsCanAllBeOverridden()
    {
        auto &db = DatabaseManager::instance();
        const int other = db.addProduct("Another project");
        FakeAiServer server;
        configureAi(server, suggestion());
        TaskCreationDialog dialog;
        analyze(dialog);
        QTRY_VERIFY(dialog.findChild<QGroupBox *>("taskCreationReview")->isVisible());
        auto *kind = dialog.findChild<QComboBox *>("taskCreationKind");
        kind->setCurrentIndex(kind->findData(static_cast<int>(TaskCreationKind::NewTask)));
        auto *project = dialog.findChild<QComboBox *>("taskCreationProject");
        project->setCurrentIndex(project->findData(other));
        dialog.findChild<QLineEdit *>("taskCreationTitle")->setText("My edited title");
        dialog.findChild<QPlainTextEdit *>("taskCreationContext")->setPlainText("My edited context.");
        auto *priority = dialog.findChild<QComboBox *>("taskCreationPriority");
        QCOMPARE(priority->currentData().toInt(), static_cast<int>(TaskPriority::High));
        priority->setCurrentIndex(priority->findData(static_cast<int>(TaskPriority::Low)));
        dialog.findChild<QPushButton *>("taskCreationCreate")->click();
        const auto tasks = db.getTasksForProduct(other);
        QCOMPARE(tasks.size(), 1);
        QCOMPARE(tasks[0].title, QString("My edited title"));
        QVERIFY(tasks[0].content.contains("My edited context."));
        QCOMPARE(tasks[0].priority, TaskPriority::Low);
        QCOMPARE(count("subtasks"), 0);
    }

    void newProjectSuggestionCanBeEditedBeforeCreation()
    {
        FakeAiServer server;
        configureAi(server, suggestion("project"));
        TaskCreationDialog dialog;
        analyze(dialog);
        QTRY_VERIFY(dialog.findChild<QGroupBox *>("taskCreationReview")->isVisible());
        QCOMPARE(count("products"), 1);
        dialog.findChild<QLineEdit *>("taskCreationProjectName")->setText("Edited project");
        dialog.findChild<QPushButton *>("taskCreationCreate")->click();
        QCOMPARE(count("products"), 2);
        bool found = false;
        for (const auto &project : DatabaseManager::instance().getAllProducts())
            found = found || project.name == "Edited project";
        QVERIFY(found);
    }

    void uncertainResponseLeavesDropdownsUnselected()
    {
        FakeAiServer server;
        auto object = suggestion("task");
        object["projectId"] = QJsonValue(QJsonValue::Null);
        object["confident"] = false;
        configureAi(server, object);
        TaskCreationDialog dialog;
        analyze(dialog);
        QTRY_VERIFY(dialog.findChild<QGroupBox *>("taskCreationReview")->isVisible());
        auto *project = dialog.findChild<QComboBox *>("taskCreationProject");
        QVERIFY(project->currentData().toInt() <= 0);
        QVERIFY(!dialog.findChild<QPushButton *>("taskCreationCreate")->isEnabled());
        project->setCurrentIndex(project->findData(m_project));
        QVERIFY(dialog.findChild<QPushButton *>("taskCreationCreate")->isEnabled());
        dialog.reject();
        QCOMPARE(count("tasks"), 1);
    }

    void manualPathDoesNotRequireAiConfiguration()
    {
        TaskCreationDialog dialog;
        manual(dialog);
        auto *project = dialog.findChild<QComboBox *>("taskCreationProject");
        project->setCurrentIndex(project->findData(m_project));
        auto *create = dialog.findChild<QPushButton *>("taskCreationCreate");
        QVERIFY(create->isEnabled());
        create->click();
        QCOMPARE(count("tasks"), 2);
    }

    void manualSetupSupportsAnEmptyDatabase()
    {
        QSqlQuery query;
        QVERIFY(query.exec("DELETE FROM products"));
        TaskCreationDialog dialog;
        manual(dialog);
        QCOMPARE(dialog.findChild<QComboBox *>("taskCreationKind")->currentData().toInt(),
                 static_cast<int>(TaskCreationKind::NewProject));
        dialog.findChild<QLineEdit *>("taskCreationProjectName")->setText("My first project");
        auto *createButton = dialog.findChild<QPushButton *>("taskCreationCreate");
        QVERIFY(createButton->isEnabled());
        createButton->click();
        QCOMPARE(count("products"), 1);
        QCOMPARE(count("tasks"), 1);
    }

    void unmatchedDropdownTextCannotReuseAnOldSelection()
    {
        TaskCreationDialog dialog;
        manual(dialog);
        auto *project = dialog.findChild<QComboBox *>("taskCreationProject");
        project->setCurrentIndex(project->findData(m_project));
        auto *createButton = dialog.findChild<QPushButton *>("taskCreationCreate");
        QVERIFY(createButton->isEnabled());
        project->setEditText("A project that does not exist");
        QVERIFY(!createButton->isEnabled());
        QCOMPARE(count("tasks"), 1);
    }

    void consentDeclineDoesNotSendOrCreate()
    {
        FakeAiServer server;
        configureAi(server, suggestion());
        TaskCreationDialog dialog;
        dialog.show();
        auto *input = dialog.findChild<QPlainTextEdit *>("taskCreationInput");
        input->setPlainText("Keep this input");
        answerConsent(QMessageBox::No);
        dialog.findChild<QPushButton *>("taskCreationAnalyze")->click();
        QCOMPARE(server.requests, 0);
        QCOMPARE(input->toPlainText(), QString("Keep this input"));
        QCOMPARE(count("tasks"), 1);
        QVERIFY(!dialog.findChild<QGroupBox *>("taskCreationReview")->isVisible());
    }

    void aiFailureIsVisibleAndManualPathRemainsAvailable_data()
    {
        QTest::addColumn<QString>("failure");
        QTest::newRow("network") << "network";
        QTest::newRow("invalid-json") << "invalid-json";
        QTest::newRow("unknown-id") << "unknown-id";
    }

    void aiFailureIsVisibleAndManualPathRemainsAvailable()
    {
        QFETCH(QString, failure);
        FakeAiServer server;
        auto object = suggestion();
        if (failure == "unknown-id") object["parentTaskId"] = 999999;
        configureAi(server, object);
        if (failure == "network") server.statusCode = 500;
        if (failure == "invalid-json")
            server.response = R"({"choices":[{"message":{"content":"not JSON"}}]})";
        TaskCreationDialog dialog;
        QSignalSpy unrelatedError(&AIService::instance(), &AIService::error);
        analyze(dialog);
        QTRY_COMPARE(server.requests, 1);
        QTRY_VERIFY(dialog.findChild<QPushButton *>("taskCreationAnalyze")->isEnabled());
        const auto message = dialog.findChild<QLabel *>("taskCreationStatus")->text();
        QVERIFY(message.contains("failed", Qt::CaseInsensitive) || message.contains("invalid", Qt::CaseInsensitive));
        QCOMPARE(count("tasks"), 1);
        QCOMPARE(count("subtasks"), 0);
        QCOMPARE(unrelatedError.size(), 0);
        QVERIFY(dialog.findChild<QPushButton *>("taskCreationManual")->isEnabled());
        QCOMPARE(dialog.findChild<QPlainTextEdit *>("taskCreationInput")->toPlainText(), QString("Improve task capture"));
    }

    void aiRequestReportsTimeoutSeparatelyFromCancellation_data()
    {
        QTest::addColumn<bool>("timedOut");
        QTest::newRow("cancelled") << false;
        QTest::newRow("timed-out") << true;
    }

    void aiRequestReportsTimeoutSeparatelyFromCancellation()
    {
        QFETCH(bool, timedOut);
        FakeAiServer server;
        configureAi(server, suggestion());
        server.holdResponse = true;
        QString payload, error, result;
        QVERIFY(TaskCreationService::buildPayload("Capture", catalog(), payload, error));
        auto *reply = AIService::instance().suggestTaskCreation(payload,
            [&](const QString &text) { result = text; }, [&](const QString &text) { error = text; });
        QVERIFY(reply);
        QCOMPARE(reply->request().transferTimeout(), 60000);
        if (timedOut) reply->setProperty("aiRequestTimedOut", true);
        reply->abort();
        QTRY_VERIFY(!error.isEmpty());
        QCOMPARE(error.contains("timed out"), timedOut);
        QVERIFY(result.isEmpty());
    }

    void cancellingAndDeletingDialogAreSafe()
    {
        FakeAiServer server;
        configureAi(server, suggestion());
        server.holdResponse = true;
        auto dialog = std::make_unique<TaskCreationDialog>();
        analyze(*dialog);
        QTRY_COMPARE(server.requests, 1);
        dialog->findChild<QPushButton *>("taskCreationCancelRequest")->click();
        QVERIFY(dialog->findChild<QLabel *>("taskCreationStatus")->text().contains("cancelled"));
        dialog->findChild<QPushButton *>("taskCreationManual")->click();
        QVERIFY(dialog->findChild<QGroupBox *>("taskCreationReview")->isVisible());
        server.sendResponse();
        QCoreApplication::processEvents();
        QCOMPARE(dialog->findChild<QComboBox *>("taskCreationKind")->currentData().toInt(),
                 static_cast<int>(TaskCreationKind::NewTask));
        analyze(*dialog);
        QTRY_COMPARE(server.requests, 2);
        dialog.reset();
        server.sendResponse();
        QCoreApplication::processEvents();
        QCOMPARE(count("subtasks"), 0);
        QCOMPARE(count("tasks"), 1);
    }

    void changingInputInvalidatesReview()
    {
        TaskCreationDialog dialog;
        manual(dialog);
        dialog.findChild<QPlainTextEdit *>("taskCreationInput")->setPlainText("A changed request");
        QVERIFY(!dialog.findChild<QGroupBox *>("taskCreationReview")->isVisible());
        QVERIFY(!dialog.findChild<QPushButton *>("taskCreationCreate")->isEnabled());
        QCOMPARE(count("tasks"), 1);
    }

    void failedSavePreservesDraftAndPreventsCreation()
    {
        TaskCreationDialog dialog(nullptr, []() { return false; });
        manual(dialog);
        auto *project = dialog.findChild<QComboBox *>("taskCreationProject");
        project->setCurrentIndex(project->findData(m_project));
        dialog.findChild<QPushButton *>("taskCreationCreate")->click();
        QCOMPARE(count("tasks"), 1);
        QVERIFY(dialog.findChild<QGroupBox *>("taskCreationReview")->isVisible());
        QVERIFY(dialog.findChild<QLabel *>("taskCreationStatus")->text().contains("could not be saved"));
    }

    void failedSavePreventsSendingAiSource()
    {
        FakeAiServer server;
        configureAi(server, suggestion());
        TaskCreationDialog dialog(nullptr, []() { return false; });
        dialog.show();
        dialog.findChild<QPlainTextEdit *>("taskCreationInput")->setPlainText("Do not send unsaved context");
        dialog.findChild<QPushButton *>("taskCreationAnalyze")->click();
        QCOMPARE(server.requests, 0);
        QVERIFY(dialog.findChild<QLabel *>("taskCreationStatus")->text().contains("could not be saved"));
    }

    void staleParentFailurePreservesEditableDraft()
    {
        FakeAiServer server;
        configureAi(server, suggestion());
        TaskCreationDialog dialog;
        analyze(dialog);
        QTRY_VERIFY(dialog.findChild<QGroupBox *>("taskCreationReview")->isVisible());
        QVERIFY(DatabaseManager::instance().archiveTask(m_task));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Failed to create task from draft:"));
        dialog.findChild<QPushButton *>("taskCreationCreate")->click();
        QCOMPARE(count("subtasks"), 0);
        QVERIFY(dialog.isVisible());
        QCOMPARE(dialog.findChild<QLineEdit *>("taskCreationTitle")->text(), QString("Improve task capture"));
        QVERIFY(dialog.findChild<QLabel *>("taskCreationStatus")->text().contains("active", Qt::CaseInsensitive));
        auto *kind = dialog.findChild<QComboBox *>("taskCreationKind");
        kind->setCurrentIndex(kind->findData(static_cast<int>(TaskCreationKind::NewTask)));
        dialog.findChild<QPushButton *>("taskCreationCreate")->click();
        QCOMPARE(count("tasks"), 2);
    }
};

QTEST_MAIN(TaskCreationTest)
#include "TaskCreationTest.moc"
