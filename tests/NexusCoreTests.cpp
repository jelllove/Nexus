#include <QAbstractItemModelTester>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTemporaryDir>
#include <QtTest>

#include "db/DatabaseManager.h"
#include "models/ProductListModel.h"
#include "models/TaskListModel.h"

class NexusCoreTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void productLifecycle();
    void taskLifecycle();
    void taskOrdering();
    void subtaskLifecycle();
    void productDeletionCascades();
    void searchTracksContentChanges();
    void settingsRoundTrip();
    void taskReorderRollsBackOnFailure();
    void taskReorderRollsBackFailedCommit();
    void taskReorderDoesNotCommitCallerTransaction();
    void productModelEditingAndReordering();
    void taskModelExpansion();
    void taskModelCompletionFilter();
    void filteredTaskReorderPreservesHiddenSlots();
    void filteredSearchReorderPreservesOtherGroups();
    void taskModelReordersWithinPriority();
    void taskModelReportsReorderFailure();
    void deletedTaskModelReportsDeletedStatus();
    void operationHistoryCoversTables_data();
    void operationHistoryCoversTables();
    void operationHistoryTracksStatuses();
    void operationHistoryTracksOrderingAndMoves();
    void operationHistorySurvivesCascadedDeletion();
    void operationHistoryRollsBackWithWrites();
    void operationHistoryFailureRejectsWrite();
    void operationHistoryDoesNotRecordReads();
    void operationHistoryTracksSettingReplacement();
    void operationHistoryMigratesExistingDatabase();
};

void NexusCoreTests::taskModelCompletionFilter()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Filter");
    QList<int> tasks;
    for (int i = 0; i < 5; ++i) {
        const int id = db.addTask(product, "Filter task");
        QVERIFY(db.updateTaskWorkStatus(id, static_cast<TaskWorkStatus>(i)));
        tasks.append(id);
    }
    const int unfinishedChild = db.addSubtask(tasks[3], "Unfinished child");
    const int doneChild = db.addSubtask(tasks[0], "Done child");
    QVERIFY(db.updateSubtaskWorkStatus(doneChild, TaskWorkStatus::Completed));
    TaskListModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.loadTasks(product);
    model.toggleExpand(tasks[3]);
    model.toggleExpand(tasks[0]);
    QCOMPARE(model.rowCount(), 7);
    QSqlQuery writes;
    QVERIFY(writes.exec("SELECT COUNT(*) FROM database_operations"));
    QVERIFY(writes.next());
    const int writesBefore = writes.value(0).toInt();
    writes.finish();
    QVERIFY2(model.setProperty("showCompleted", false), "Completion filter must be available");
    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(model.rowForTaskId(tasks[3]), -1);
    QCOMPARE(model.rowForSubTaskId(unfinishedChild), -1);
    QCOMPARE(model.rowForSubTaskId(doneChild), -1);
    QVERIFY(!model.index(model.rowForTaskId(tasks[0])).data(TaskListModel::HasSubTasksRole).toBool());
    QVERIFY(model.isExpanded(tasks[0]));
    QVERIFY(model.setProperty("showCompleted", true));
    QCOMPARE(model.rowCount(), 7);
    QVERIFY(model.rowForSubTaskId(doneChild) >= 0);
    QVERIFY(writes.exec("SELECT COUNT(*) FROM database_operations"));
    QVERIFY(writes.next());
    QCOMPARE(writes.value(0).toInt(), writesBefore);
    writes.finish();
    QVERIFY(model.setProperty("showCompleted", false));
    model.loadSearchResults(db.searchTasks("Filter"));
    QCOMPARE(model.rowCount(), 4);
    model.refresh();
    QCOMPARE(model.rowCount(), 4);
    for (int id : tasks) QVERIFY(db.archiveTask(id));
    model.loadTasks(product, TaskStatus::Archived);
    QCOMPARE(model.rowCount(), 4);
    for (int id : tasks) QVERIFY(db.deleteTask(id));
    model.loadDeletedTasks();
    QCOMPARE(model.rowCount(), 4);
}

void NexusCoreTests::filteredTaskReorderPreservesHiddenSlots()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Reorder filter");
    QList<int> ids;
    for (int i = 0; i < 5; ++i) ids.append(db.addTask(product, QString::number(i)));
    QVERIFY(db.reorderTasks(ids));
    QVERIFY(db.updateTaskWorkStatus(ids[1], TaskWorkStatus::Completed));
    QVERIFY(db.updateTaskWorkStatus(ids[3], TaskWorkStatus::Completed));
    TaskListModel model;
    model.loadTasks(product);
    QVERIFY(model.setProperty("showCompleted", false));
    QCOMPARE(model.rowCount(), 3);
    QVERIFY(model.moveRows({}, 2, 1, {}, 0));
    QList<int> actual;
    for (const auto &task : db.getTasksForProduct(product)) actual.append(task.id);
    QCOMPARE(actual, QList<int>({ids[4], ids[1], ids[0], ids[3], ids[2]}));
}

void NexusCoreTests::filteredSearchReorderPreservesOtherGroups()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Search source");
    const int other = db.addProduct("Other product");
    const int first = db.addTask(product, "First");
    const int hidden = db.addTask(product, "Hidden");
    const int last = db.addTask(product, "Last");
    const int otherTask = db.addTask(other, "Other");
    const int archived = db.addTask(product, "Archived");
    QVERIFY(db.archiveTask(archived));
    QVERIFY(db.reorderTasks({first, hidden, last}));
    QVERIFY(db.updateTaskWorkStatus(hidden, TaskWorkStatus::Completed));
    const int otherOrder = db.getTask(otherTask).sortOrder;
    const int archivedOrder = db.getTask(archived).sortOrder;
    TaskListModel model;
    model.loadSearchResults({db.getTask(first), db.getTask(otherTask), db.getTask(hidden),
                             db.getTask(archived), db.getTask(last)});
    QVERIFY(model.setProperty("showCompleted", false));
    QVERIFY(model.moveRows({}, model.rowForTaskId(last), 1, {}, 0));
    QList<int> actual;
    for (const auto &task : db.getTasksForProduct(product)) actual.append(task.id);
    QCOMPARE(actual, QList<int>({last, hidden, first}));
    QCOMPARE(db.getTask(otherTask).sortOrder, otherOrder);
    QCOMPARE(db.getTask(archived).sortOrder, archivedOrder);
    QCOMPARE(model.visibleTaskCount(), 4);
    QCOMPARE(model.taskIdAt(0), last);
    QCOMPARE(model.taskIdAt(1), otherTask);
    QCOMPARE(model.taskIdAt(2), archived);
    QCOMPARE(model.taskIdAt(3), first);
}

void NexusCoreTests::initTestCase()
{
    auto &db = DatabaseManager::instance();
    QVERIFY(db.initialize(":memory:"));
    QCOMPARE(db.currentDbPath(), QString(":memory:"));
}

void NexusCoreTests::init()
{
    QSqlQuery query;
    QVERIFY2(query.exec("DROP TRIGGER IF EXISTS fail_task_reorder"),
             qPrintable(query.lastError().text()));
    QVERIFY2(query.exec("DELETE FROM products"), qPrintable(query.lastError().text()));
    QVERIFY2(query.exec("DELETE FROM settings"), qPrintable(query.lastError().text()));
    if (QSqlDatabase::database().tables().contains("database_operations")) {
        QVERIFY2(query.exec("DELETE FROM database_operations"),
                 qPrintable(query.lastError().text()));
    }
}

void NexusCoreTests::operationHistoryCoversTables_data()
{
    QTest::addColumn<QString>("table");
    QTest::addColumn<QString>("insert");
    QTest::addColumn<QString>("update");
    QTest::addColumn<QString>("remove");
    QTest::addColumn<QString>("changedField");
    QTest::newRow("products") << "products"
        << "INSERT INTO products (id, name) VALUES (100, 'private title')"
        << "UPDATE products SET name = 'private replacement' WHERE id = 100"
        << "DELETE FROM products WHERE id = 100" << "name";
    QTest::newRow("tasks") << "tasks"
        << "INSERT INTO tasks (id, product_id, title) VALUES (100, %1, 'private title')"
        << "UPDATE tasks SET title = 'private replacement' WHERE id = 100"
        << "DELETE FROM tasks WHERE id = 100" << "title";
    QTest::newRow("subtasks") << "subtasks"
        << "INSERT INTO subtasks (id, task_id, title) VALUES (100, %2, 'private title')"
        << "UPDATE subtasks SET title = 'private replacement' WHERE id = 100"
        << "DELETE FROM subtasks WHERE id = 100" << "title";
    QTest::newRow("history") << "task_content_history"
        << "INSERT INTO task_content_history (id, task_id, content) VALUES (100, %2, 'private content')"
        << "UPDATE task_content_history SET content = 'private replacement' WHERE id = 100"
        << "DELETE FROM task_content_history WHERE id = 100" << "content";
    QTest::newRow("settings") << "settings"
        << "INSERT INTO settings (key, value) VALUES ('api_key', 'private secret')"
        << "UPDATE settings SET value = 'private replacement' WHERE key = 'api_key'"
        << "DELETE FROM settings WHERE key = 'api_key'" << "value";
}

void NexusCoreTests::operationHistoryCoversTables()
{
    QFETCH(QString, table);
    QFETCH(QString, insert);
    QFETCH(QString, update);
    QFETCH(QString, remove);
    QFETCH(QString, changedField);
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Fixture");
    const int task = db.addTask(product, "Fixture");
    QVERIFY(product > 0 && task > 0);
    QSqlQuery query;
    QVERIFY2(query.exec("DELETE FROM database_operations"), qPrintable(query.lastError().text()));
    // Use the same clock: SQLite and Qt can quantize sub-millisecond time differently.
    QVERIFY(query.exec("SELECT strftime('%Y-%m-%dT%H:%M:%fZ', 'now')"));
    QVERIFY(query.next());
    const auto before = QDateTime::fromString(query.value(0).toString(), Qt::ISODateWithMs);
    query.finish();
    insert.replace("%1", QString::number(product));
    insert.replace("%2", QString::number(task));
    QVERIFY2(query.exec(insert), qPrintable(query.lastError().text()));
    QVERIFY2(query.exec(update), qPrintable(query.lastError().text()));
    QVERIFY2(query.exec(remove), qPrintable(query.lastError().text()));
    QVERIFY(query.exec("SELECT strftime('%Y-%m-%dT%H:%M:%fZ', 'now')"));
    QVERIFY(query.next());
    const auto after = QDateTime::fromString(query.value(0).toString(), Qt::ISODateWithMs);
    query.finish();
    QVERIFY2(query.exec("SELECT occurred_at, table_name, record_id, operation, changed_fields "
                        "FROM database_operations ORDER BY id"), qPrintable(query.lastError().text()));
    for (const auto &operation : QStringList{"INSERT", "UPDATE", "DELETE"}) {
        QVERIFY(query.next());
        const QString timestamp = query.value(0).toString();
        QVERIFY(QRegularExpression(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$)")
                    .match(timestamp).hasMatch());
        const auto recorded = QDateTime::fromString(timestamp, Qt::ISODateWithMs);
        QVERIFY2(recorded >= before && recorded <= after,
                 qPrintable(QString("Recorded %1 outside SQLite clock bounds [%2, %3]")
                                .arg(timestamp, before.toString(Qt::ISODateWithMs),
                                     after.toString(Qt::ISODateWithMs))));
        QCOMPARE(query.value(1).toString(), table);
        QCOMPARE(query.value(2).toString(), table == "settings" ? QString("api_key") : QString("100"));
        QCOMPARE(query.value(3).toString(), operation);
        if (operation == "UPDATE")
            QCOMPARE(query.value(4).toString(), changedField);
        else
            QVERIFY(query.value(4).toString().split(',').contains(changedField));
    }
    QVERIFY(!query.next());
    QVERIFY(query.exec("SELECT * FROM database_operations"));
    while (query.next()) {
        for (int column = 0; column < query.record().count(); ++column)
            QVERIFY(!query.value(column).toString().contains("private"));
    }
}

void NexusCoreTests::operationHistoryTracksStatuses()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Statuses");
    const int task = db.addTask(product, "Task");
    const int subtask = db.addSubtask(task, "Step");
    QVERIFY(product > 0 && task > 0 && subtask > 0);
    QSqlQuery query;
    QVERIFY2(query.exec("DELETE FROM database_operations"), qPrintable(query.lastError().text()));
    QVERIFY(db.updateTaskWorkStatus(task, TaskWorkStatus::Ongoing));
    QVERIFY(db.updateTaskWorkStatus(task, TaskWorkStatus::Completed));
    QVERIFY(db.updateTaskTitle(task, "After completion"));
    QVERIFY(db.archiveTask(task));
    QVERIFY(db.reactivateTask(task));
    QVERIFY(db.deleteTask(task));
    QVERIFY(db.restoreTask(task));
    QVERIFY(db.updateSubtaskWorkStatus(subtask, TaskWorkStatus::Waiting));
    QVERIFY(query.exec("SELECT table_name, changed_fields, old_status, new_status, "
                       "old_work_status, new_work_status FROM database_operations ORDER BY id"));
    QVERIFY(query.next());
    QCOMPARE(query.value(4).toInt(), 0);
    QCOMPARE(query.value(5).toInt(), 1);
    QVERIFY(query.value(1).toString().split(',').contains("work_status"));
    QVERIFY(query.next());
    QCOMPARE(query.value(4).toInt(), 1);
    QCOMPARE(query.value(5).toInt(), 3);
    QVERIFY(query.next());
    QVERIFY(query.value(1).toString().split(',').contains("title"));
    for (const auto &transition : QList<QStringList>{
             {"active", "archived"}, {"archived", "active"},
             {"active", "deleted"}, {"deleted", "active"}}) {
        QVERIFY(query.next());
        QCOMPARE(query.value(2).toString(), transition[0]);
        QCOMPARE(query.value(3).toString(), transition[1]);
    }
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("subtasks"));
    QCOMPARE(query.value(4).toInt(), 0);
    QCOMPARE(query.value(5).toInt(), 4);
    QVERIFY(!query.next());
}

void NexusCoreTests::operationHistoryTracksOrderingAndMoves()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Ordering");
    const int first = db.addTask(product, "First");
    const int second = db.addTask(product, "Second");
    const int subtask = db.addSubtask(first, "Step");
    QVERIFY(product > 0 && first > 0 && second > 0 && subtask > 0);
    QSqlQuery query;
    QVERIFY2(query.exec("DELETE FROM database_operations"), qPrintable(query.lastError().text()));
    QVERIFY(db.reorderTasks({second, first}));
    QVERIFY(db.moveSubtask(subtask, second));
    QVERIFY(query.exec("SELECT table_name, changed_fields FROM database_operations ORDER BY id"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("tasks"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("tasks"));
    QVERIFY(query.value(1).toString().split(',').contains("sort_order"));
    bool recordedMove = false;
    while (query.next()) {
        if (query.value(0).toString() == "subtasks"
            && query.value(1).toString().split(',').contains("task_id"))
            recordedMove = true;
    }
    QVERIFY(recordedMove);
}

void NexusCoreTests::operationHistorySurvivesCascadedDeletion()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Cascade");
    const int task = db.addTask(product, "Task");
    QVERIFY(db.addSubtask(task, "Step") > 0);
    db.saveContentSnapshot(task, "Synthetic snapshot");
    QSqlQuery query;
    QVERIFY2(query.exec("DELETE FROM database_operations"), qPrintable(query.lastError().text()));
    QVERIFY(db.deleteProduct(product));
    QVERIFY(query.exec("SELECT table_name, operation FROM database_operations ORDER BY table_name"));
    for (const auto &table : QStringList{"products", "subtasks", "task_content_history", "tasks"}) {
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), table);
        QCOMPARE(query.value(1).toString(), QString("DELETE"));
    }
    QVERIFY(!query.next());
}

void NexusCoreTests::operationHistoryRollsBackWithWrites()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Rollback");
    const int task = db.addTask(product, "Original");
    QSqlQuery query;
    QVERIFY2(query.exec("DELETE FROM database_operations"), qPrintable(query.lastError().text()));
    QVERIFY(QSqlDatabase::database().transaction());
    QVERIFY(db.updateTaskTitle(task, "Uncommitted"));
    QVERIFY(query.exec("SELECT COUNT(*) FROM database_operations"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
    query.finish();
    QVERIFY(QSqlDatabase::database().rollback());
    QCOMPARE(db.getTask(task).title, QString("Original"));
    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression("Failed to add task:.*FOREIGN KEY constraint failed.*"));
    QCOMPARE(db.addTask(-1, "Invalid parent"), -1);
    QVERIFY(query.exec("SELECT COUNT(*) FROM database_operations"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0);
}

void NexusCoreTests::operationHistoryFailureRejectsWrite()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Failure");
    const int task = db.addTask(product, "Original");
    QSqlQuery query;
    QVERIFY2(query.exec("CREATE TEMP TRIGGER fail_operation_log BEFORE INSERT ON database_operations "
                        "BEGIN SELECT RAISE(ABORT, 'synthetic log failure'); END"),
             qPrintable(query.lastError().text()));
    const bool saved = db.updateTaskTitle(task, "Must not persist");
    QVERIFY(query.exec("DROP TRIGGER fail_operation_log"));
    QVERIFY(!saved);
    QCOMPARE(db.getTask(task).title, QString("Original"));
}

void NexusCoreTests::operationHistoryDoesNotRecordReads()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Read");
    const int task = db.addTask(product, "Read");
    QSqlQuery query;
    QVERIFY2(query.exec("DELETE FROM database_operations"), qPrintable(query.lastError().text()));
    db.getAllProducts();
    db.getTask(task);
    db.getTasksForProduct(product);
    db.getSubtasks(task);
    db.getContentHistory(task);
    db.getSetting("example");
    db.searchTasks("Read");
    QVERIFY(query.exec("SELECT COUNT(*) FROM database_operations"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0);
}

void NexusCoreTests::operationHistoryTracksSettingReplacement()
{
    auto &db = DatabaseManager::instance();
    QSqlQuery query;
    QVERIFY2(query.exec("DELETE FROM database_operations"), qPrintable(query.lastError().text()));
    QVERIFY(db.setSetting("api_key", "private first"));
    QVERIFY(db.setSetting("api_key", "private second"));
    QVERIFY(query.exec("SELECT operation, record_id, changed_fields FROM database_operations ORDER BY id"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("INSERT"));
    QCOMPARE(query.value(1).toString(), QString("api_key"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("UPDATE"));
    QCOMPARE(query.value(1).toString(), QString("api_key"));
    QCOMPARE(query.value(2).toString(), QString("value"));
    QVERIFY(!query.next());
}

void NexusCoreTests::operationHistoryMigratesExistingDatabase()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto &db = DatabaseManager::instance();
    const QString path = directory.filePath("migration.db");
    QSqlDatabase::database().close();
    QVERIFY(db.initialize(path));
    int product = db.addProduct("Existing product");
    int task = db.addTask(product, "Existing task");
    {
        QSqlQuery query;
        QVERIFY2(query.exec("SELECT name FROM sqlite_master WHERE type = 'trigger' "
                            "AND tbl_name IN ('products', 'tasks', 'subtasks', "
                            "'task_content_history', 'settings') AND name LIKE 'audit_%'"),
                 qPrintable(query.lastError().text()));
        QStringList triggers;
        while (query.next()) triggers << query.value(0).toString();
        query.finish();
        for (const auto &trigger : triggers)
            QVERIFY(query.exec("DROP TRIGGER " + trigger));
        QVERIFY(query.exec("DROP TABLE database_operations"));
        QVERIFY(db.setSetting("db_version", "9"));
        QVERIFY(query.exec("CREATE TRIGGER audit_tasks_update AFTER UPDATE ON tasks BEGIN SELECT 1; END"));
    }
    QSqlDatabase::database().close();
    QTest::ignoreMessage(QtCriticalMsg,
                         QRegularExpression("Failed to enable operation logging:.*"));
    QTest::ignoreMessage(QtCriticalMsg,
                         QRegularExpression("Failed to migrate database operation history:.*"));
    QTest::ignoreMessage(QtCriticalMsg, "Database migration failed.");
    QVERIFY(!db.initialize(path));
    QCOMPARE(db.getSetting("db_version"), QString("9"));
    {
        QSqlQuery query;
        QVERIFY(query.exec("SELECT COUNT(*) FROM sqlite_master "
                           "WHERE name = 'database_operations' OR name LIKE 'audit_%'"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        query.finish();
        QVERIFY(query.exec("DROP TRIGGER audit_tasks_update"));
    }
    QSqlDatabase::database().close();
    QVERIFY(db.initialize(path));
    QCOMPARE(db.getTask(task).title, QString("Existing task"));
    QCOMPARE(db.getSetting("db_version"), QString("10"));
    QVERIFY(db.updateTaskWorkStatus(task, TaskWorkStatus::Completed));
    {
        QSqlQuery query;
        QVERIFY(query.exec("SELECT COUNT(*) FROM database_operations WHERE table_name = 'tasks'"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
    }
    QSqlDatabase::database().close();
    QVERIFY(db.initialize(path));
    {
        QSqlQuery query;
        QVERIFY(db.updateTaskTitle(task, "After restart"));
        QVERIFY(query.exec("SELECT COUNT(*) FROM database_operations WHERE table_name = 'tasks'"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 2);
    }
    QSqlDatabase::database().close();
    QVERIFY(db.initialize(":memory:"));
}

void NexusCoreTests::productLifecycle()
{
    auto &db = DatabaseManager::instance();
    QSignalSpy added(&db, &DatabaseManager::productAdded);
    QSignalSpy updated(&db, &DatabaseManager::productUpdated);
    const int id = db.addProduct("Release");
    QVERIFY(id > 0);
    QCOMPARE(added.count(), 1);
    QCOMPARE(added.first().first().toInt(), id);
    QCOMPARE(db.getProduct(id).name, QString("Release"));

    QVERIFY(db.updateProduct(id, "Release checklist"));
    QCOMPARE(db.getProduct(id).name, QString("Release checklist"));
    QVERIFY(db.archiveProduct(id));
    QVERIFY(db.getProductsByStatus(ProductStatus::Active).isEmpty());
    QCOMPARE(db.getProductsByStatus(ProductStatus::Archived).size(), 1);
    QCOMPARE(db.getProduct(id).status, ProductStatus::Archived);
    QVERIFY(db.reactivateProduct(id));
    QCOMPARE(db.getProductsByStatus(ProductStatus::Active).size(), 1);
    QVERIFY(db.getProductsByStatus(ProductStatus::Archived).isEmpty());
    QCOMPARE(updated.count(), 3);
}

void NexusCoreTests::taskLifecycle()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Tasks");
    QVERIFY(product > 0);
    const auto dueDate = QDateTime::fromString("2030-09-15T12:00:00Z", Qt::ISODate);
    QSignalSpy added(&db, &DatabaseManager::taskAdded);
    const int id = db.addTask(product, "Draft", TaskPriority::High, dueDate);
    QVERIFY(id > 0);
    QCOMPARE(added.count(), 1);
    QCOMPARE(db.getTask(id).dueDate, dueDate);
    QCOMPARE(db.getTask(id).priority, TaskPriority::High);
    QVERIFY(db.updateTaskTitle(id, "Final title"));
    const QString content = R"({"type":"doc","content":[{"type":"text","text":"hello"}]})";
    QVERIFY(db.updateTaskContent(id, content));
    QVERIFY(db.updateTaskWorkStatus(id, TaskWorkStatus::Waiting));
    QVERIFY(db.updateTaskDueDate(id, QDateTime()));
    const Task saved = db.getTask(id);
    QCOMPARE(saved.title, QString("Final title"));
    QCOMPARE(saved.content, content);
    QCOMPARE(saved.workStatus, TaskWorkStatus::Waiting);
    QVERIFY(!saved.dueDate.isValid());

    QVERIFY(db.archiveTask(id));
    QCOMPARE(db.getTask(id).status, TaskStatus::Archived);
    QVERIFY(db.getTasksForProduct(product).isEmpty());
    QCOMPARE(db.getTasksForProduct(product, TaskStatus::Archived).size(), 1);
    QVERIFY(db.reactivateTask(id));
    QCOMPARE(db.getTask(id).status, TaskStatus::Active);
    QVERIFY(db.deleteTask(id));
    QCOMPARE(db.getDeletedTasks().size(), 1);
    QCOMPARE(db.getTask(id).status, TaskStatus::Deleted);
    QCOMPARE(db.getTasksForProduct(product, TaskStatus::Deleted).size(), 1);
    QVERIFY(db.restoreTask(id));
    QVERIFY(db.getDeletedTasks().isEmpty());
    QCOMPARE(db.getTask(id).content, content);
    QCOMPARE(db.getTask(id).status, TaskStatus::Active);
}

void NexusCoreTests::taskOrdering()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Ordering");
    QVERIFY(product > 0);
    const auto early = QDateTime::fromString("2030-01-01T12:00:00Z", Qt::ISODate);
    const auto late = early.addDays(2);
    const int noDueDate = db.addTask(product, "No due date");
    const int later = db.addTask(product, "Later", TaskPriority::Medium, late);
    const int earlier = db.addTask(product, "Earlier", TaskPriority::Medium, early);
    const int critical = db.addTask(product, "Critical", TaskPriority::Critical);
    QVERIFY(noDueDate > 0 && later > 0 && earlier > 0 && critical > 0);
    const auto tasks = db.getTasksForProduct(product);
    QCOMPARE(tasks.size(), 4);
    QCOMPARE(tasks[0].id, critical);
    QCOMPARE(tasks[1].id, earlier);
    QCOMPARE(tasks[2].id, later);
    QCOMPARE(tasks[3].id, noDueDate);
}

void NexusCoreTests::subtaskLifecycle()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Subtasks");
    QVERIFY(product > 0);
    const int task = db.addTask(product, "Parent");
    QVERIFY(task > 0);
    const int subtask = db.addSubtask(task, "Step one");
    QVERIFY(subtask > 0);
    QCOMPARE(db.getSubtaskCount(task), 1);
    QVERIFY(db.renameSubtask(subtask, "Renamed step"));
    QVERIFY(db.toggleSubtask(subtask, true));
    const auto subtasks = db.getSubtasks(task);
    QCOMPARE(subtasks.size(), 1);
    QCOMPARE(subtasks.first().title, QString("Renamed step"));
    QVERIFY(subtasks.first().completed);
    QVERIFY(db.deleteSubtask(subtask));
    QCOMPARE(db.getSubtaskCount(task), 0);
}

void NexusCoreTests::productDeletionCascades()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Disposable");
    QVERIFY(product > 0);
    const int task = db.addTask(product, "Parent");
    QVERIFY(task > 0);
    QVERIFY(db.addSubtask(task, "Child") > 0);
    db.saveContentSnapshot(task, "Saved content");
    QCOMPARE(db.getContentHistory(task).size(), 1);
    QVERIFY(db.deleteProduct(product));
    QCOMPARE(db.getTask(task).id, 0);
    QVERIFY(db.getSubtasks(task).isEmpty());
    QVERIFY(db.getContentHistory(task).isEmpty());
}

void NexusCoreTests::searchTracksContentChanges()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Search");
    const int otherProduct = db.addProduct("Other");
    QVERIFY(product > 0 && otherProduct > 0);
    const int task = db.addTask(product, "Needle");
    const int otherTask = db.addTask(otherProduct, "Needle");
    QVERIFY(task > 0 && otherTask > 0);
    QCOMPARE(db.searchTasks("Needle").size(), 2);
    const auto scoped = db.searchTasks("Needle", product);
    QCOMPARE(scoped.size(), 1);
    QCOMPARE(scoped.first().id, task);
    QVERIFY(db.updateTaskTitle(task, "Renamed"));
    QVERIFY(db.searchTasks("Needle", product).isEmpty());
    QVERIFY(db.updateTaskContent(task, "Uniquecontent"));
    const auto contentMatches = db.searchTasks("Uniquecontent", product);
    QCOMPARE(contentMatches.size(), 1);
    QCOMPARE(contentMatches.first().id, task);
    QVERIFY(db.deleteTask(task));
    const auto deletedMatches = db.searchTasks("Uniquecontent", product);
    QCOMPARE(deletedMatches.size(), 1);
    QCOMPARE(deletedMatches.first().status, TaskStatus::Deleted);
    QVERIFY(db.permanentlyDeleteTask(task));
    QVERIFY(db.searchTasks("Uniquecontent", product).isEmpty());
}

void NexusCoreTests::settingsRoundTrip()
{
    auto &db = DatabaseManager::instance();
    QCOMPARE(db.getSetting("missing", "fallback"), QString("fallback"));
    db.setSetting("example", "first");
    db.setSetting("example", "second");
    QCOMPARE(db.getSetting("example"), QString("second"));
}

void NexusCoreTests::taskReorderRollsBackOnFailure()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Transaction");
    QVERIFY(product > 0);
    const int first = db.addTask(product, "First");
    const int second = db.addTask(product, "Second");
    QVERIFY(first > 0 && second > 0);
    QVERIFY(db.reorderTasks({first, second}));
    QSqlQuery query;
    QVERIFY2(query.exec(QString(
        "CREATE TEMP TRIGGER fail_task_reorder BEFORE UPDATE OF sort_order ON tasks "
        "WHEN OLD.id = %1 BEGIN SELECT RAISE(ABORT, 'test write failure'); END").arg(first)),
        qPrintable(query.lastError().text()));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Failed to reorder tasks:"));
    QVERIFY(!db.reorderTasks({second, first}));
    QCOMPARE(db.getTask(first).sortOrder, 0);
    QCOMPARE(db.getTask(second).sortOrder, 1);
}

void NexusCoreTests::taskReorderRollsBackFailedCommit()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Commit failure");
    QVERIFY(product > 0);
    const int first = db.addTask(product, "First");
    const int second = db.addTask(product, "Second");
    QVERIFY(first > 0 && second > 0);
    QVERIFY(db.reorderTasks({first, second}));
    QSqlQuery query;
    QVERIFY2(query.exec(
        "CREATE TEMP TRIGGER fail_task_reorder AFTER UPDATE OF sort_order ON tasks "
        "BEGIN INSERT INTO subtasks (task_id, title) VALUES (-1, 'invalid parent'); END"),
        qPrintable(query.lastError().text()));
    QVERIFY2(query.exec("PRAGMA defer_foreign_keys = ON"), qPrintable(query.lastError().text()));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Failed to commit task reorder:"));
    QVERIFY(!db.reorderTasks({second, first}));
    QCOMPARE(db.getTask(first).sortOrder, 0);
    QCOMPARE(db.getTask(second).sortOrder, 1);
    QCOMPARE(db.getSubtaskCount(-1), 0);
    auto connection = QSqlDatabase::database();
    QVERIFY(connection.transaction());
    QVERIFY(connection.rollback());
}

void NexusCoreTests::taskReorderDoesNotCommitCallerTransaction()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Caller transaction");
    QVERIFY(product > 0);
    const int task = db.addTask(product, "Task");
    QVERIFY(task > 0);
    auto connection = QSqlDatabase::database();
    QVERIFY(connection.transaction());
    db.setSetting("uncommitted", "value");
    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression("^Failed to start transaction for reorderTasks:"));
    const bool reordered = db.reorderTasks({task});
    const bool rolledBack = connection.rollback();
    QVERIFY(!reordered);
    QVERIFY(rolledBack);
    QCOMPARE(db.getSetting("uncommitted", "missing"), QString("missing"));
}

void NexusCoreTests::productModelEditingAndReordering()
{
    auto &db = DatabaseManager::instance();
    const int first = db.addProduct("First");
    const int second = db.addProduct("Second");
    QVERIFY(first > 0 && second > 0);
    QVERIFY(db.reorderProducts({first, second}));
    ProductListModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.loadProducts();
    QCOMPARE(model.rowCount(), 2);
    QVERIFY(model.setData(model.index(0, 0), "  Renamed  "));
    QCOMPARE(db.getProduct(first).name, QString("Renamed"));
    QVERIFY(!model.setData(model.index(0, 0), "  "));
    QVERIFY(model.moveRows({}, 0, 1, {}, 2));
    QCOMPARE(model.productIdAt(0), second);
    QCOMPARE(db.getAllProducts().first().id, second);
}

void NexusCoreTests::taskModelExpansion()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Expansion");
    QVERIFY(product > 0);
    const int task = db.addTask(product, "Parent");
    QVERIFY(task > 0);
    const int subtask = db.addSubtask(task, "Child");
    QVERIFY(subtask > 0);
    TaskListModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.loadTasks(product);
    QCOMPARE(model.rowCount(), 1);
    model.toggleExpand(task);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(1, 0), TaskListModel::SubTaskIdRole).toInt(), subtask);
    QCOMPARE(model.data(model.index(1, 0), TaskListModel::ParentTaskIdRole).toInt(), task);
    QVERIFY(model.data(model.index(1, 0), TaskListModel::IsSubTaskRole).toBool());
    QVERIFY(!model.canDropAt(1, 0));
    model.toggleExpand(task);
    QCOMPARE(model.rowCount(), 1);
}

void NexusCoreTests::taskModelReordersWithinPriority()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Drag");
    QVERIFY(product > 0);
    const int first = db.addTask(product, "First", TaskPriority::High);
    const int second = db.addTask(product, "Second", TaskPriority::High);
    const int low = db.addTask(product, "Low", TaskPriority::Low);
    QVERIFY(first > 0 && second > 0 && low > 0);
    QVERIFY(db.reorderTasks({first, second}));
    TaskListModel model;
    model.loadTasks(product);
    QVERIFY(!model.canDropAt(0, 2));
    QVERIFY(model.moveRows({}, 1, 1, {}, 0));
    QCOMPARE(model.taskIdAt(0), second);
    QCOMPARE(db.getTasksForProduct(product).first().id, second);
    QCOMPARE(model.taskIdAt(2), low);
}

void NexusCoreTests::taskModelReportsReorderFailure()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Failed drag");
    QVERIFY(product > 0);
    const int first = db.addTask(product, "First");
    const int second = db.addTask(product, "Second");
    QVERIFY(first > 0 && second > 0);
    QVERIFY(db.reorderTasks({first, second}));
    TaskListModel model;
    model.loadTasks(product);
    QSqlQuery query;
    QVERIFY2(query.exec(
        "CREATE TEMP TRIGGER fail_task_reorder BEFORE UPDATE OF sort_order ON tasks "
        "BEGIN SELECT RAISE(ABORT, 'test write failure'); END"),
        qPrintable(query.lastError().text()));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Failed to reorder tasks:"));
    QVERIFY(!model.moveRows({}, 0, 1, {}, 2));
    QCOMPARE(model.taskIdAt(0), first);
    QCOMPARE(db.getTasksForProduct(product).first().id, first);
}

void NexusCoreTests::deletedTaskModelReportsDeletedStatus()
{
    auto &db = DatabaseManager::instance();
    const int product = db.addProduct("Trash");
    QVERIFY(product > 0);
    const int task = db.addTask(product, "Deleted");
    QVERIFY(task > 0);
    QVERIFY(db.deleteTask(task));
    TaskListModel model;
    model.loadDeletedTasks();
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), TaskListModel::StatusRole).toString(),
             QString("deleted"));
}

QTEST_GUILESS_MAIN(NexusCoreTests)
#include "NexusCoreTests.moc"
