#include <QAbstractItemModelTester>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
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
    void taskModelReordersWithinPriority();
    void taskModelReportsReorderFailure();
    void deletedTaskModelReportsDeletedStatus();
};

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
