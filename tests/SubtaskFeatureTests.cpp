#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "db/DatabaseManager.h"
#include "models/EditorTarget.h"

class SubtaskFeatureTests : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;
    QString m_databasePath;

    void createLegacyV6Database()
    {
        m_databasePath = m_tempDir.filePath("nexus-v6.db");
        {
            QSqlDatabase legacy = QSqlDatabase::addDatabase("QSQLITE", "legacy-v6");
            legacy.setDatabaseName(m_databasePath);
            QVERIFY(legacy.open());

            QSqlQuery query(legacy);
            QVERIFY(query.exec("PRAGMA foreign_keys=ON"));
            QVERIFY(query.exec(
                "CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT NOT NULL, "
                "sort_order INTEGER DEFAULT 0, created_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
                "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP)"));
            QVERIFY(query.exec(
                "CREATE TABLE tasks (id INTEGER PRIMARY KEY, product_id INTEGER NOT NULL, "
                "title TEXT NOT NULL DEFAULT '', content TEXT DEFAULT '', priority INTEGER DEFAULT 2, "
                "status TEXT DEFAULT 'active', sort_order INTEGER DEFAULT 0, "
                "created_at DATETIME DEFAULT CURRENT_TIMESTAMP, updated_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
                "archived_at DATETIME, due_date DATETIME, completed INTEGER DEFAULT 0, "
                "completed_at DATETIME, work_status INTEGER DEFAULT 0, deleted_at DATETIME, "
                "FOREIGN KEY(product_id) REFERENCES products(id) ON DELETE CASCADE)"));
            QVERIFY(query.exec(
                "CREATE TABLE subtasks (id INTEGER PRIMARY KEY, task_id INTEGER NOT NULL, "
                "title TEXT NOT NULL DEFAULT '', completed INTEGER DEFAULT 0, sort_order INTEGER DEFAULT 0, "
                "created_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
                "FOREIGN KEY(task_id) REFERENCES tasks(id) ON DELETE CASCADE)"));
            QVERIFY(query.exec("CREATE TABLE settings (key TEXT PRIMARY KEY, value TEXT)"));
            QVERIFY(query.exec("INSERT INTO settings VALUES ('db_version', '6')"));
            QVERIFY(query.exec("INSERT INTO products(id, name) VALUES (1, 'Product')"));
            QVERIFY(query.exec(
                "INSERT INTO tasks(id, product_id, title, content) "
                "VALUES (1, 1, 'Parent', '<p>parent-before</p>')"));
            QVERIFY(query.exec("INSERT INTO subtasks(id, task_id, title) VALUES (1, 1, 'Child')"));
            legacy.close();
        }
        QSqlDatabase::removeDatabase("legacy-v6");
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        qRegisterMetaType<EditorTarget>();
        createLegacyV6Database();
        QVERIFY(DatabaseManager::instance().initialize(m_databasePath));
    }

    void editorTargetsKeepLayersDistinct()
    {
        const EditorTarget task = EditorTarget::task(7);
        const EditorTarget subtask = EditorTarget::subtask(7);

        QVERIFY(task.isValid());
        QVERIFY(subtask.isValid());
        QVERIFY(task != subtask);
        QCOMPARE(task.kind, EditorTargetKind::Task);
        QCOMPARE(subtask.kind, EditorTargetKind::SubTask);
        QCOMPARE(task.id, 7);
        QCOMPARE(subtask.id, 7);
        QVERIFY(!EditorTarget().isValid());
    }

    void migrationAddsSubtaskNoteStorage()
    {
        const SubTask subtask = DatabaseManager::instance().getSubtask(1);
        QCOMPARE(subtask.id, 1);
        QCOMPARE(subtask.content, QString());
        QVERIFY(subtask.createdAt.isValid());
        QVERIFY(subtask.updatedAt.isValid());
        QCOMPARE(DatabaseManager::instance().getSetting("db_version"), QString("7"));
    }

    void parentAndSubtaskWritesStayIndependent()
    {
        auto &database = DatabaseManager::instance();
        QVERIFY(database.updateTaskContent(1, "<p>parent-after</p>"));
        QVERIFY(database.updateSubtaskContent(1, "<p>child-after</p>"));
        QVERIFY(database.updateSubtaskTitle(1, "Child renamed"));

        QCOMPARE(database.getTask(1).content, QString("<p>parent-after</p>"));
        QCOMPARE(database.getSubtask(1).content, QString("<p>child-after</p>"));
        QCOMPARE(database.getTask(1).title, QString("Parent"));
        QCOMPARE(database.getSubtask(1).title, QString("Child renamed"));
    }

    void subtaskHistoryIsIndependent()
    {
        auto &database = DatabaseManager::instance();
        QVERIFY(database.saveContentSnapshot(1, "<p>parent snapshot</p>"));
        QVERIFY(database.saveSubtaskContentSnapshot(1, "<p>child snapshot</p>"));

        QCOMPARE(database.getContentHistory(1), QList<QString>{"<p>parent snapshot</p>"});
        QCOMPARE(database.getSubtaskContentHistory(1), QList<QString>{"<p>child snapshot</p>"});
    }
};

QTEST_MAIN(SubtaskFeatureTests)
#include "SubtaskFeatureTests.moc"
