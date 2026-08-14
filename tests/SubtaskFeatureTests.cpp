#include <QtTest>
#include <QDir>
#include <QFile>
#include <QListView>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "db/DatabaseManager.h"
#include "models/EditorTarget.h"
#include "models/SearchResult.h"
#include "models/TaskListModel.h"
#include "ui/TaskCardDelegate.h"
#include "ui/TaskPane.h"

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

    void searchReturnsTypedParentAndSubtaskMatches()
    {
        auto &database = DatabaseManager::instance();
        QVERIFY(database.updateTaskContent(1, "<p>parent-only-key</p>"));
        QVERIFY(database.updateSubtaskContent(1, "<p>child-only-key</p>"));

        const QList<SearchResult> parentResults = database.searchItems("parent-only-key");
        QCOMPARE(parentResults.size(), 1);
        QCOMPARE(parentResults.first().target(), EditorTarget::task(1));

        const QList<SearchResult> childResults = database.searchItems("child-only-key");
        QCOMPARE(childResults.size(), 1);
        QCOMPARE(childResults.first().target(), EditorTarget::subtask(1));
        QCOMPARE(childResults.first().parentTask.id, 1);
    }

    void searchModelPreservesParentContextAndTypedRows()
    {
        auto &database = DatabaseManager::instance();
        QVERIFY(database.updateSubtaskContent(1, "<p>child-only-key</p>"));

        const QList<SearchResult> results = database.searchItems("child-only-key");
        TaskListModel model;
        model.loadSearchResults(results);

        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.targetAt(0), EditorTarget::task(1));
        QCOMPARE(model.targetAt(1), EditorTarget::subtask(1));
        QVERIFY(model.index(1, 0).data(TaskListModel::IsSubTaskRole).toBool());
        QCOMPARE(model.index(1, 0).data(TaskListModel::ContentRole).toString(),
                 QString("<p>child-only-key</p>"));
    }

    void typedContentWritesRouteToTheirOwnLayer()
    {
        auto &database = DatabaseManager::instance();
        const auto write = [&database](const EditorTarget &target, const QString &content) {
            if (target.kind == EditorTargetKind::Task) {
                return database.updateTaskContent(target.id, content);
            }
            if (target.kind == EditorTargetKind::SubTask) {
                return database.updateSubtaskContent(target.id, content);
            }
            return false;
        };

        QVERIFY(write(EditorTarget::task(1), "<p>task routed</p>"));
        QVERIFY(write(EditorTarget::subtask(1), "<p>subtask routed</p>"));
        QCOMPARE(database.getTask(1).content, QString("<p>task routed</p>"));
        QCOMPARE(database.getSubtask(1).content, QString("<p>subtask routed</p>"));
        QVERIFY(!write(EditorTarget(), "<p>invalid</p>"));
    }

    void subtaskRowsUseThirtyTwoPixelHeight()
    {
        TaskListModel model;
        model.loadTasks(1);
        model.toggleExpand(1);

        const QModelIndex subtaskIndex = model.index(1, 0);
        QVERIFY(subtaskIndex.isValid());

        TaskCardDelegate delegate;
        QStyleOptionViewItem option;
        QCOMPARE(delegate.sizeHint(option, subtaskIndex).height(), 32);
    }

    void subtaskBodySelectsButCheckboxOnlyToggles()
    {
        auto &database = DatabaseManager::instance();
        QVERIFY(database.toggleSubtask(1, false));

        TaskPane pane;
        pane.resize(420, 500);
        pane.loadTasks(1);

        auto *model = pane.findChild<TaskListModel *>();
        auto *view = pane.findChild<QListView *>();
        QVERIFY(model);
        QVERIFY(view);

        model->toggleExpand(1);
        pane.show();
        QTest::qWait(50);

        const QModelIndex subtaskIndex = model->index(1, 0);
        QVERIFY(subtaskIndex.isValid());

        QSignalSpy selectionSpy(&pane, &TaskPane::itemSelected);

        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->visualRect(subtaskIndex).center());
        QCOMPARE(selectionSpy.count(), 1);
        QCOMPARE(qvariant_cast<EditorTarget>(selectionSpy.takeFirst().at(0)),
                 EditorTarget::subtask(1));

        QStyleOptionViewItem option;
        option.rect = view->visualRect(subtaskIndex);
        option.font = view->font();

        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          TaskCardDelegate::subtaskCheckboxRect(option).center());
        QCOMPARE(selectionSpy.count(), 0);
        QVERIFY(database.getSubtask(1).completed);
    }

    void searchRefreshKeepsCurrentQueryResults()
    {
        auto &database = DatabaseManager::instance();
        QVERIFY(database.updateSubtaskContent(1, "<p>refresh-search-key</p>"));

        TaskPane pane;
        pane.showSearchResults("refresh-search-key",
                               database.searchItems("refresh-search-key"));

        auto *model = pane.findChild<TaskListModel *>();
        QVERIFY(model);
        QCOMPARE(model->rowCount(), 2);

        pane.refreshCurrentView();
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(model->targetAt(0), EditorTarget::task(1));
        QCOMPARE(model->targetAt(1), EditorTarget::subtask(1));
    }

    void subtaskDataCascadesOnPermanentParentDeletion()
    {
        auto &database = DatabaseManager::instance();
        const int taskId = database.addTask(1, "Disposable parent");
        const int subtaskId = database.addSubtask(taskId, "Disposable child");
        QVERIFY(taskId > 0);
        QVERIFY(subtaskId > 0);
        QVERIFY(database.updateSubtaskContent(subtaskId, "<p>disposable note</p>"));
        QVERIFY(database.saveSubtaskContentSnapshot(subtaskId, "<p>snapshot</p>"));

        QVERIFY(database.permanentlyDeleteTask(taskId));
        QCOMPARE(database.getSubtask(subtaskId).id, -1);
        QVERIFY(database.getSubtaskContentHistory(subtaskId).isEmpty());
        QVERIFY(database.searchItems("disposable note").isEmpty());
    }

    void zzSubtaskSearchFallsBackWhenFtsIsUnavailable()
    {
        auto &database = DatabaseManager::instance();
        QVERIFY(database.updateSubtaskContent(1, "<p>fallback-child-key</p>"));

        {
            QSqlDatabase connection =
                QSqlDatabase::addDatabase("QSQLITE", "disable-subtask-fts");
            connection.setDatabaseName(m_databasePath);
            QVERIFY(connection.open());

            QSqlQuery query(connection);
            QVERIFY(query.exec("DROP TABLE subtasks_fts"));
            connection.close();
        }
        QSqlDatabase::removeDatabase("disable-subtask-fts");

        const QList<SearchResult> results =
            database.searchItems("fallback-child-key");
        QCOMPARE(results.size(), 1);
        QCOMPARE(results.first().target(), EditorTarget::subtask(1));

        QVERIFY(database.updateSubtaskContent(1, "<p>fallback-write-key</p>"));
        QVERIFY(database.toggleSubtask(1, true));
        QCOMPARE(database.getSubtask(1).content, QString("<p>fallback-write-key</p>"));
        QVERIFY(database.getSubtask(1).completed);

        const QList<SearchResult> updatedResults =
            database.searchItems("fallback-write-key");
        QCOMPARE(updatedResults.size(), 1);
        QCOMPARE(updatedResults.first().target(), EditorTarget::subtask(1));
    }

    void cleanupTestCase()
    {
        const QString tempPath = m_tempDir.path();
        QSqlDatabase::database().close();
        QVERIFY2(QFile::remove(m_databasePath),
                 qPrintable(QString("Expected test database to be removable during teardown: %1")
                                .arg(m_databasePath)));
        QVERIFY(m_tempDir.remove());
        QVERIFY2(!QDir(tempPath).exists(),
                 qPrintable(QString("Expected temp directory to be removed during teardown: %1")
                                .arg(tempPath)));
    }
};

QTEST_MAIN(SubtaskFeatureTests)
#include "SubtaskFeatureTests.moc"
