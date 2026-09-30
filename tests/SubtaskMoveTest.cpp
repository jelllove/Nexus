#include "app/MainWindow.h"
#include "db/DatabaseManager.h"
#include "ui/TaskCardDelegate.h"
#include <QDialogButtonBox>
#include <QFocusEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest>
#include <functional>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class SubtaskMoveTest : public QObject
{
    Q_OBJECT
    QTemporaryDir m_data;
    std::unique_ptr<MainWindow> m_window;
    int m_product = -1;
    int m_task = -1;
    int m_first = -1;
    int m_second = -1;
    int m_third = -1;
    int m_otherProduct = -1;
    int m_destination = -1;
    int m_existing = -1;

    QList<int> ids(int taskId)
    {
        QList<int> result;
        for (const auto &subtask : DatabaseManager::instance().getSubtasks(taskId))
            result.append(subtask.id);
        return result;
    }

    void checkOrder(int taskId, const QList<int> &expected)
    {
        QCOMPARE(ids(taskId), expected);
        const auto subtasks = DatabaseManager::instance().getSubtasks(taskId);
        for (int i = 0; i < subtasks.size(); ++i)
            QCOMPARE(subtasks[i].sortOrder, i);
    }

    QList<QVariantList> snapshot()
    {
        QSqlQuery query("SELECT id, task_id, title, content, completed, work_status, "
                        "sort_order, created_at, updated_at FROM subtasks ORDER BY id");
        QList<QVariantList> result;
        while (query.next()) {
            QVariantList row;
            for (int i = 0; i < 9; ++i) row.append(query.value(i));
            result.append(row);
        }
        return result;
    }

    void contextMenu(int subtaskId, const std::function<void(QMenu *)> &visit)
    {
        auto *pane = m_window->findChild<TaskPane *>();
        auto *model = pane->findChild<TaskListModel *>();
        auto *list = pane->findChild<QListView *>();
        const int row = model->rowForSubTaskId(subtaskId);
        QVERIFY(row >= 0);
        list->scrollTo(model->index(row));
        QTimer::singleShot(0, this, [visit]() {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(menu);
            visit(menu);
            menu->close();
        });
        list->customContextMenuRequested(list->visualRect(model->index(row)).center());
    }

    void chooseAction(int subtaskId, const QString &text)
    {
        bool chosen = false;
        contextMenu(subtaskId, [&](QMenu *menu) {
            for (auto *action : menu->actions()) {
                if (action->text() == text && action->isEnabled()) {
                    chosen = true;
                    QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                                      menu->actionGeometry(action).center());
                    return;
                }
            }
        });
        QVERIFY2(chosen, qPrintable(text));
    }

    void onMoveDialog(const std::function<void(QDialog *)> &visit)
    {
        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [timer, visit]() {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog || dialog->objectName() != "moveSubtaskDialog") return;
            timer->stop();
            visit(dialog);
            if (dialog->isVisible()) dialog->reject();
            timer->deleteLater();
        });
        timer->start(10);
        QTimer::singleShot(5000, timer, [timer]() {
            QTest::qFail("Move dialog did not open", __FILE__, __LINE__);
            timer->stop();
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
                dialog->reject();
            timer->deleteLater();
        });
    }

    void answerWarning()
    {
        QTimer::singleShot(0, this, []() {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            QCOMPARE(box->windowTitle(), "Move Sub Task");
            box->accept();
        });
    }

    void requestMove(int subtaskId, int destination, int position)
    {
        m_window->findChild<TaskPane *>()->subTaskMoveRequested(subtaskId, destination, position);
    }

    void moveMouseWithButton(QListView *list, const QPoint &to)
    {
        QMouseEvent move(QEvent::MouseMove, to, list->viewport()->mapToGlobal(to),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(list->viewport(), &move);
    }

    void dragMouse(QListView *list, const QPoint &from, const QPoint &to)
    {
        QTest::mousePress(list->viewport(), Qt::LeftButton, Qt::NoModifier, from);
        moveMouseWithButton(list, to);
        QTest::mouseRelease(list->viewport(), Qt::LeftButton, Qt::NoModifier, to);
    }

    QRect subtaskRect(int subtaskId)
    {
        auto *pane = m_window->findChild<TaskPane *>();
        auto *model = pane->findChild<TaskListModel *>();
        return pane->findChild<QListView *>()->visualRect(model->index(model->rowForSubTaskId(subtaskId)));
    }

    void showBothParents(bool expandDestination = false)
    {
        auto *pane = m_window->findChild<TaskPane *>();
        auto &db = DatabaseManager::instance();
        pane->loadSearchResults({db.getTask(m_task), db.getTask(m_destination)}, "main task");
        if (expandDestination) pane->findChild<TaskListModel *>()->toggleExpand(m_destination);
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setApplicationName("NexusSubtaskMoveTest");
        QCoreApplication::setOrganizationName("NexusSubtaskMoveTest");
        QCoreApplication::setApplicationVersion("1.0.7");
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_data.isValid());
        auto &db = DatabaseManager::instance();
        QVERIFY(db.initialize(m_data.filePath("test.db")));
        db.setSetting("check_updates", "false");
    }

    void init()
    {
        auto &db = DatabaseManager::instance();
        m_product = db.addProduct("Source product");
        m_task = db.addTask(m_product, "Source main task");
        m_first = db.addSubtask(m_task, "First subtask");
        m_second = db.addSubtask(m_task, "Second subtask");
        m_third = db.addSubtask(m_task, "Third subtask");
        m_otherProduct = db.addProduct("Destination product");
        m_destination = db.addTask(m_otherProduct, "Destination main task");
        m_existing = db.addSubtask(m_destination, "Existing destination subtask");
        QVERIFY(m_first > 0 && m_second > 0);
        m_window = std::make_unique<MainWindow>();
        m_window->show();
        QVERIFY(m_window->findChild<ProductPane *>()->selectProductById(m_product));
        m_window->findChild<TaskPane *>()->findChild<TaskListModel *>()->toggleExpand(m_task);
    }

    void menuOffersReorderingAndMoving()
    {
        auto *pane = m_window->findChild<TaskPane *>();
        auto *model = pane->findChild<TaskListModel *>();
        auto *list = pane->findChild<QListView *>();
        QTimer::singleShot(0, this, []() {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(menu);
            QStringList actions;
            for (auto *action : menu->actions()) actions.append(action->text());
            menu->close();
            QVERIFY(actions.contains("Move Up"));
            QVERIFY(actions.contains("Move Down"));
            QVERIFY(actions.contains("Move to Main Task..."));
        });
        list->customContextMenuRequested(list->visualRect(model->index(2)).center());
    }

    void movesUpDownAndKeepsSelection()
    {
        chooseAction(m_second, "Move Up");
        checkOrder(m_task, {m_second, m_first, m_third});
        auto *pane = m_window->findChild<TaskPane *>();
        auto *list = pane->findChild<QListView *>();
        QCOMPARE(list->currentIndex().data(TaskListModel::SubTaskIdRole).toInt(), m_second);
        QCOMPARE(m_window->findChild<EditorPane *>()->currentSubTaskId(), m_second);
        chooseAction(m_second, "Move Down");
        checkOrder(m_task, {m_first, m_second, m_third});
    }

    void disablesBoundaryAndDeletedMoves()
    {
        QMap<QString, bool> enabled;
        auto record = [&](QMenu *menu) {
            for (auto *action : menu->actions()) enabled[action->text()] = action->isEnabled();
        };
        contextMenu(m_first, record);
        QVERIFY(!enabled["Move Up"]);
        QVERIFY(enabled["Move Down"]);
        contextMenu(m_third, record);
        QVERIFY(enabled["Move Up"]);
        QVERIFY(!enabled["Move Down"]);
        QVERIFY(DatabaseManager::instance().deleteTask(m_task));
        auto *pane = m_window->findChild<TaskPane *>();
        pane->restoreView(m_product, TaskPane::ViewMode::Deleted, -1);
        contextMenu(m_second, record);
        QVERIFY(!enabled["Move Up"]);
        QVERIFY(!enabled["Move Down"]);
        QVERIFY(!enabled["Move to Main Task..."]);
    }

    void samePositionAndAppendStayStable()
    {
        auto &db = DatabaseManager::instance();
        QVERIFY(db.moveSubtask(m_first, m_task, 0));
        checkOrder(m_task, {m_first, m_second, m_third});
        QVERIFY(db.moveSubtask(m_third, m_task, 2));
        checkOrder(m_task, {m_first, m_second, m_third});
        QVERIFY(db.moveSubtask(m_first, m_task));
        checkOrder(m_task, {m_second, m_third, m_first});
        QVERIFY(db.moveSubtask(m_first, m_task, 0));
        checkOrder(m_task, {m_first, m_second, m_third});
    }

    void normalizesLegacyGapsAndTies()
    {
        QSqlQuery query;
        QVERIFY(query.exec(QString("UPDATE subtasks SET sort_order = 42 WHERE task_id = %1").arg(m_task)));
        auto &db = DatabaseManager::instance();
        QVERIFY(db.moveSubtask(m_second, m_destination, 0));
        checkOrder(m_task, {m_first, m_third});
        checkOrder(m_destination, {m_second, m_existing});
    }

    void movingRetainsAllNonPlacementFields_data()
    {
        QTest::addColumn<int>("status");
        for (int status = 0; status < 5; ++status)
            QTest::newRow(qPrintable(QString::number(status))) << status;
    }

    void movingRetainsAllNonPlacementFields()
    {
        QFETCH(int, status);
        auto &db = DatabaseManager::instance();
        QVERIFY(db.updateSubtaskContent(m_second, "<p>Keep <strong>rich content</strong></p>"));
        QVERIFY(db.updateSubtaskWorkStatus(m_second, static_cast<TaskWorkStatus>(status)));
        QSqlQuery timestamps;
        QVERIFY(timestamps.exec(QString("UPDATE subtasks SET created_at = '2020-01-02 03:04:05', "
            "updated_at = '2021-02-03 04:05:06' WHERE id = %1").arg(m_second)));
        const SubTask before = db.getSubtask(m_second);
        QVERIFY(db.moveSubtask(m_second, m_destination));
        const SubTask after = db.getSubtask(m_second);
        QCOMPARE(after.id, before.id);
        QCOMPARE(after.taskId, m_destination);
        QCOMPARE(after.title, before.title);
        QCOMPARE(after.content, before.content);
        QCOMPARE(after.workStatus, before.workStatus);
        QCOMPARE(after.completed, before.completed);
        QCOMPARE(after.createdAt, before.createdAt);
        QCOMPARE(after.updatedAt, before.updatedAt);
        QSqlQuery rawCompleted;
        QVERIFY(rawCompleted.exec(QString("SELECT completed FROM subtasks WHERE id = %1").arg(m_second)));
        QVERIFY(rawCompleted.next());
        QCOMPARE(rawCompleted.value(0).toBool(), status == 3);
        checkOrder(m_task, {m_first, m_third});
        checkOrder(m_destination, {m_existing, m_second});
    }

    void supportsEmptyAndArchivedDestinations()
    {
        auto &db = DatabaseManager::instance();
        QVERIFY(db.deleteSubtask(m_existing));
        QVERIFY(db.archiveTask(m_destination));
        QVERIFY(db.archiveProduct(m_otherProduct));
        QVERIFY(db.moveSubtask(m_second, m_destination, 0));
        checkOrder(m_destination, {m_second});
        QVERIFY(db.moveSubtask(m_second, m_task, 1));
        QVERIFY(db.getSubtasks(m_destination).isEmpty());
        checkOrder(m_task, {m_first, m_second, m_third});
    }

    void rejectsInvalidMovesWithoutChangingData()
    {
        auto &db = DatabaseManager::instance();
        const auto before = snapshot();
        const QList<QList<int>> requests = {
            {-1, m_task, 0}, {999999, m_task, 0}, {m_second, -1, 0},
            {m_second, 999999, 0}, {m_second, m_task, -2},
            {m_second, m_task, 3}, {m_second, m_destination, 2}
        };
        for (const auto &request : requests) {
            QString error;
            QVERIFY(!db.moveSubtask(request[0], request[1], request[2], &error));
            QVERIFY(!error.isEmpty());
            QCOMPARE(snapshot(), before);
        }
        QVERIFY(db.deleteTask(m_destination));
        QVERIFY(!db.moveSubtask(m_second, m_destination));
        QCOMPARE(snapshot(), before);
        QVERIFY(db.deleteTask(m_task));
        QVERIFY(!db.moveSubtask(m_second, m_task, 0));
        QCOMPARE(snapshot(), before);
    }

    void rollsBackMidMoveFailure()
    {
        QSqlQuery query;
        QVERIFY(query.exec(QString(
            "CREATE TEMP TRIGGER fail_move BEFORE UPDATE OF sort_order ON subtasks "
            "WHEN NEW.id = %1 BEGIN SELECT RAISE(ABORT, 'simulated ordering failure'); END").arg(m_existing)));
        const auto before = snapshot();
        QString error;
        QVERIFY(!DatabaseManager::instance().moveSubtask(m_second, m_destination, -1, &error));
        QVERIFY(error.contains("simulated ordering failure"));
        QCOMPARE(snapshot(), before);
        QVERIFY(query.exec("DROP TRIGGER fail_move"));
        QVERIFY(DatabaseManager::instance().moveSubtask(m_second, m_destination));
        checkOrder(m_destination, {m_existing, m_second});
    }

    void rejectsReadOnlyDatabase()
    {
        QSqlQuery query;
        QVERIFY(query.exec("PRAGMA query_only=ON"));
        const auto before = snapshot();
        const bool moved = DatabaseManager::instance().moveSubtask(m_second, m_destination);
        QVERIFY(query.exec("PRAGMA query_only=OFF"));
        QVERIFY(!moved);
        QCOMPARE(snapshot(), before);
    }

    void movementIsPersistedForNewConnectionsAndExport()
    {
        auto &db = DatabaseManager::instance();
        QVERIFY(db.moveSubtask(m_second, m_destination, 0));
        {
            auto connection = QSqlDatabase::addDatabase("QSQLITE", "verifyMove");
            connection.setDatabaseName(m_data.filePath("test.db"));
            connection.setConnectOptions("QSQLITE_OPEN_READONLY");
            QVERIFY(connection.open());
            QSqlQuery query(connection);
            QVERIFY(query.exec(QString("SELECT id FROM subtasks WHERE task_id = %1 "
                                       "ORDER BY sort_order, id").arg(m_destination)));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), m_second);
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), m_existing);
        }
        QSqlDatabase::removeDatabase("verifyMove");
        ExportTaskItem item;
        item.title = "Destination main task";
        item.productName = "Destination product";
        for (const auto &subtask : db.getSubtasks(m_destination))
            item.subtasks.append({subtask.title, subtask.workStatus, Task::workStatusIcon(subtask.workStatus)});
        const QString markdown = TaskExportService::buildMarkdown({item}, QDateTime::currentDateTime());
        QVERIFY(markdown.indexOf("Second subtask") >= 0);
        QVERIFY(markdown.indexOf("Second subtask") < markdown.indexOf("Existing destination subtask"));
        QVERIFY(!markdown.contains("First subtask"));
    }

    void pickerFiltersAndMovesAcrossProducts()
    {
        auto &db = DatabaseManager::instance();
        const int deleted = db.addTask(m_otherProduct, "Deleted destination");
        QVERIFY(db.deleteTask(deleted));
        QVERIFY(db.archiveTask(m_destination));
        QVERIFY(db.archiveProduct(m_otherProduct));
        m_window->findChild<ProductPane *>()->loadProducts();
        auto *editor = m_window->findChild<EditorPane *>();
        editor->findChild<EditorBridge *>()->onEditorReady();
        editor->loadSubTask(m_second);
        editor->findChild<EditorBridge *>()->setContent("<p>Pending edit before moving</p>");
        QSignalSpy reloaded(editor->findChild<EditorBridge *>(), &EditorBridge::loadContentRequested);
        bool visited = false;
        onMoveDialog([&](QDialog *dialog) {
            visited = true;
            auto *tree = dialog->findChild<QTreeWidget *>();
            auto *search = dialog->findChild<QLineEdit *>();
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
            QTreeWidgetItem *target = nullptr;
            for (QTreeWidgetItemIterator it(tree); *it; ++it) {
                const int id = (*it)->data(0, Qt::UserRole).toInt();
                QVERIFY(id != m_task && id != deleted);
                if (id == m_destination) target = *it;
            }
            QVERIFY(target);
            tree->setCurrentItem(target);
            search->setText("no-match-at-all");
            QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
            search->setText("DESTINATION product");
            QVERIFY(!target->isHidden());
            QVERIFY(!target->parent()->isHidden());
            QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
            buttons->button(QDialogButtonBox::Ok)->click();
        });
        chooseAction(m_second, "Move to Main Task...");
        QVERIFY(visited);
        checkOrder(m_destination, {m_existing, m_second});
        QCOMPARE(db.getSubtask(m_second).content, "<p>Pending edit before moving</p>");
        QCOMPARE(reloaded.count(), 0);
        QCOMPARE(editor->currentSubTaskId(), m_second);
        QCOMPARE(m_window->findChild<ProductPane *>()->selectedProductId(), m_otherProduct);
        auto *pane = m_window->findChild<TaskPane *>();
        QCOMPARE(pane->viewMode(), TaskPane::ViewMode::Archived);
        QCOMPARE(pane->findChild<QListView *>()->currentIndex().data(TaskListModel::SubTaskIdRole).toInt(), m_second);
        QVERIFY(pane->findChild<TaskListModel *>()->isExpanded(m_destination));
    }

    void cancelAndNoDestinationLeaveDataUntouched()
    {
        auto &db = DatabaseManager::instance();
        QVERIFY(db.deleteTask(m_destination));
        const auto before = snapshot();
        bool visited = false;
        onMoveDialog([&](QDialog *dialog) {
            visited = true;
            QCOMPARE(dialog->findChild<QTreeWidget *>()->topLevelItemCount(), 0);
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
            buttons->button(QDialogButtonBox::Cancel)->click();
        });
        chooseAction(m_first, "Move to Main Task...");
        QVERIFY(visited);
        QCOMPARE(snapshot(), before);
    }

    void searchReorderStaysInSearchAndReparentNavigatesOut()
    {
        QVERIFY(QMetaObject::invokeMethod(m_window.get(), "onSearchRequested",
                                          Q_ARG(QString, QString("Source main"))));
        auto *pane = m_window->findChild<TaskPane *>();
        requestMove(m_second, m_task, 0);
        checkOrder(m_task, {m_second, m_first, m_third});
        QCOMPARE(pane->viewMode(), TaskPane::ViewMode::SearchResults);
        QCOMPARE(pane->findChild<QListView *>()->currentIndex().data(TaskListModel::SubTaskIdRole).toInt(), m_second);
        requestMove(m_second, m_destination, -1);
        QCOMPARE(pane->viewMode(), TaskPane::ViewMode::Active);
        QCOMPARE(pane->findChild<TaskListModel *>()->currentProductId(), m_otherProduct);
        QVERIFY(m_window->findChild<SearchBar *>()->searchText().isEmpty());
        QVERIFY(QMetaObject::invokeMethod(m_window.get(), "onSearchCleared"));
        QCOMPARE(pane->findChild<TaskListModel *>()->currentProductId(), m_otherProduct);
    }

    void savesOtherOpenNoteBeforeFollowingMovedSubtask()
    {
        auto *editor = m_window->findChild<EditorPane *>();
        editor->loadTask(m_task);
        editor->findChild<EditorBridge *>()->setContent("<p>Pending main task edit</p>");
        requestMove(m_second, m_destination, -1);
        QCOMPARE(DatabaseManager::instance().getTask(m_task).content, "<p>Pending main task edit</p>");
        QCOMPARE(editor->currentSubTaskId(), m_second);
    }

    void failedSavePreventsMoveAndKeepsPendingContent()
    {
        auto *editor = m_window->findChild<EditorPane *>();
        editor->loadSubTask(m_second);
        editor->findChild<EditorBridge *>()->setContent("<p>Do not lose this edit</p>");
        QSqlQuery query;
        QVERIFY(query.exec("PRAGMA query_only=ON"));
        answerWarning();
        requestMove(m_second, m_destination, -1);
        QVERIFY(query.exec("PRAGMA query_only=OFF"));
        QCOMPARE(DatabaseManager::instance().getSubtask(m_second).taskId, m_task);
        QCOMPARE(editor->currentSubTaskId(), m_second);
        QVERIFY(editor->saveCurrentContent());
        QCOMPARE(DatabaseManager::instance().getSubtask(m_second).content, "<p>Do not lose this edit</p>");
    }

    void failedMoveDoesNotChangeSelection()
    {
        auto *pane = m_window->findChild<TaskPane *>();
        QVERIFY(pane->revealSubTask(m_second, false));
        answerWarning();
        requestMove(m_second, 999999, -1);
        QCOMPARE(pane->findChild<QListView *>()->currentIndex().data(TaskListModel::SubTaskIdRole).toInt(), m_second);
        QCOMPARE(DatabaseManager::instance().getSubtask(m_second).taskId, m_task);
    }

    void mouseDragReordersSubtasks()
    {
        auto *pane = m_window->findChild<TaskPane *>();
        auto *model = pane->findChild<TaskListModel *>();
        auto *list = pane->findChild<QListView *>();
        const QRect source = list->visualRect(model->index(model->rowForSubTaskId(m_second)));
        const QRect target = list->visualRect(model->index(model->rowForSubTaskId(m_first)));
        dragMouse(list, source.center(), QPoint(target.center().x(), target.top() + 2));
        QCOMPARE(ids(m_task), QList<int>({m_second, m_first, m_third}));
    }

    void mouseDragDownUsesFinalPosition()
    {
        auto *pane = m_window->findChild<TaskPane *>();
        auto *list = pane->findChild<QListView *>();
        const QRect target = subtaskRect(m_third);
        QSignalSpy selected(pane, &TaskPane::subTaskSelected);
        dragMouse(list, subtaskRect(m_first).center(), QPoint(target.center().x(), target.bottom() - 2));
        checkOrder(m_task, {m_second, m_third, m_first});
        QCOMPARE(selected.count(), 0);
        QCOMPARE(list->currentIndex().data(TaskListModel::SubTaskIdRole).toInt(), m_first);
    }

    void mouseDragOntoCollapsedMainTaskAppendsAndSaves()
    {
        showBothParents();
        auto *pane = m_window->findChild<TaskPane *>();
        auto *model = pane->findChild<TaskListModel *>();
        auto *list = pane->findChild<QListView *>();
        auto *editor = m_window->findChild<EditorPane *>();
        auto *bridge = editor->findChild<EditorBridge *>();
        bridge->onEditorReady();
        editor->loadSubTask(m_second);
        bridge->setContent("<p>Pending drag edit</p>");
        QSignalSpy reloaded(bridge, &EditorBridge::loadContentRequested);
        QVERIFY(!model->isExpanded(m_destination));
        const QPoint target = list->visualRect(model->index(model->rowForTaskId(m_destination))).center();
        dragMouse(list, subtaskRect(m_second).center(), target);
        checkOrder(m_task, {m_first, m_third});
        checkOrder(m_destination, {m_existing, m_second});
        QCOMPARE(DatabaseManager::instance().getSubtask(m_second).content, "<p>Pending drag edit</p>");
        QCOMPARE(reloaded.count(), 0);
        QVERIFY(model->isExpanded(m_destination));
        QCOMPARE(list->currentIndex().data(TaskListModel::SubTaskIdRole).toInt(), m_second);
        QCOMPARE(m_window->findChild<ProductPane *>()->selectedProductId(), m_otherProduct);
    }

    void mouseDragBetweenParentsInsertsAtIndicator_data()
    {
        QTest::addColumn<bool>("after");
        QTest::newRow("before") << false;
        QTest::newRow("after") << true;
    }

    void mouseDragBetweenParentsInsertsAtIndicator()
    {
        QFETCH(bool, after);
        showBothParents(true);
        auto *list = m_window->findChild<TaskPane *>()->findChild<QListView *>();
        const QRect target = subtaskRect(m_existing);
        const QPoint drop(target.center().x(), after ? target.bottom() - 2 : target.top() + 2);
        dragMouse(list, subtaskRect(m_second).center(), drop);
        checkOrder(m_destination, after ? QList<int>{m_existing, m_second}
                                        : QList<int>{m_second, m_existing});
        checkOrder(m_task, {m_first, m_third});
    }

    void mouseDragCanMoveLastChildToEmptyMainTask()
    {
        auto &db = DatabaseManager::instance();
        QVERIFY(db.deleteSubtask(m_first));
        QVERIFY(db.deleteSubtask(m_third));
        QVERIFY(db.deleteSubtask(m_existing));
        showBothParents();
        auto *pane = m_window->findChild<TaskPane *>();
        auto *model = pane->findChild<TaskListModel *>();
        auto *list = pane->findChild<QListView *>();
        const QRect last = list->visualRect(model->index(model->rowForTaskId(m_destination)));
        const QPoint belowLast(last.center().x(), last.bottom() + 12);
        QVERIFY(list->viewport()->rect().contains(belowLast));
        dragMouse(list, subtaskRect(m_second).center(), belowLast);
        QVERIFY(db.getSubtasks(m_task).isEmpty());
        checkOrder(m_destination, {m_second});
        TaskListModel reloaded;
        reloaded.loadTasks(m_product);
        QVERIFY(!reloaded.index(reloaded.rowForTaskId(m_task)).data(TaskListModel::HasSubTasksRole).toBool());
    }

    void mouseDragShowsIndicatorAndEscapeCancels()
    {
        auto *pane = m_window->findChild<TaskPane *>();
        auto *list = pane->findChild<QListView *>();
        const QRect target = subtaskRect(m_first);
        const QPoint drop(target.center().x(), target.top() + 2);
        const auto before = snapshot();
        QSignalSpy requests(pane, &TaskPane::subTaskMoveRequested);
        QSignalSpy clicked(list, &QListView::clicked);
        QTest::mousePress(list->viewport(), Qt::LeftButton, Qt::NoModifier, subtaskRect(m_second).center());
        moveMouseWithButton(list, drop);
        QCOMPARE(list->viewport()->cursor().shape(), Qt::ClosedHandCursor);
        const QPixmap image = list->viewport()->grab();
        const QImage pixels = image.toImage();
        int bluePixels = 0;
        for (int x = target.left() + 30; x < target.right() - 6; ++x) {
            if (pixels.pixelColor(qRound(x * image.devicePixelRatio()),
                                  qRound(target.top() * image.devicePixelRatio())) == QColor("#2980b9"))
                ++bluePixels;
        }
        QVERIFY(bluePixels > target.width() / 2);
        QTest::keyClick(list, Qt::Key_Escape);
        QTest::mouseRelease(list->viewport(), Qt::LeftButton, Qt::NoModifier, drop);
        QCOMPARE(snapshot(), before);
        QCOMPARE(requests.count(), 0);
        QCOMPARE(clicked.count(), 0);
        QVERIFY(list->viewport()->cursor().shape() != Qt::ClosedHandCursor);
    }

    void mouseDragOutsideOrOntoSelfDoesNotMove()
    {
        auto *pane = m_window->findChild<TaskPane *>();
        auto *list = pane->findChild<QListView *>();
        const auto before = snapshot();
        QSignalSpy requests(pane, &TaskPane::subTaskMoveRequested);
        const QPoint source = subtaskRect(m_second).center();
        dragMouse(list, source, QPoint(-20, source.y()));
        dragMouse(list, source, source + QPoint(QApplication::startDragDistance() + 2, 0));
        QCOMPARE(requests.count(), 0);
        QCOMPARE(snapshot(), before);
    }

    void mouseDragModelResetCancelsStaleGesture()
    {
        auto *pane = m_window->findChild<TaskPane *>();
        auto *list = pane->findChild<QListView *>();
        const QRect target = subtaskRect(m_first);
        const QPoint drop(target.center().x(), target.top() + 2);
        const auto before = snapshot();
        QSignalSpy requests(pane, &TaskPane::subTaskMoveRequested);
        QTest::mousePress(list->viewport(), Qt::LeftButton, Qt::NoModifier, subtaskRect(m_second).center());
        moveMouseWithButton(list, drop);
        pane->loadTasks(m_product);
        QTest::mouseRelease(list->viewport(), Qt::LeftButton, Qt::NoModifier, drop);
        QCOMPARE(requests.count(), 0);
        QCOMPARE(snapshot(), before);
    }

    void mouseDragRejectsDeletedSourceAndTarget()
    {
        QVERIFY(DatabaseManager::instance().deleteTask(m_destination));
        showBothParents(true);
        auto *pane = m_window->findChild<TaskPane *>();
        auto *model = pane->findChild<TaskListModel *>();
        auto *list = pane->findChild<QListView *>();
        const auto before = snapshot();
        QSignalSpy requests(pane, &TaskPane::subTaskMoveRequested);
        QVERIFY(!model->index(model->rowForSubTaskId(m_existing)).flags().testFlag(Qt::ItemIsDragEnabled));
        const QPoint destination = list->visualRect(model->index(model->rowForTaskId(m_destination))).center();
        dragMouse(list, subtaskRect(m_second).center(), destination);
        dragMouse(list, subtaskRect(m_existing).center(), subtaskRect(m_first).center());
        QCOMPARE(requests.count(), 0);
        QCOMPARE(snapshot(), before);
    }

    void mouseDragFocusLossCancels()
    {
        auto *pane = m_window->findChild<TaskPane *>();
        auto *list = pane->findChild<QListView *>();
        const QRect target = subtaskRect(m_first);
        const QPoint drop(target.center().x(), target.top() + 2);
        const auto before = snapshot();
        QSignalSpy requests(pane, &TaskPane::subTaskMoveRequested);
        QTest::mousePress(list->viewport(), Qt::LeftButton, Qt::NoModifier, subtaskRect(m_second).center());
        moveMouseWithButton(list, drop);
        QFocusEvent focusOut(QEvent::FocusOut, Qt::ActiveWindowFocusReason);
        QApplication::sendEvent(list, &focusOut);
        QTest::mouseRelease(list->viewport(), Qt::LeftButton, Qt::NoModifier, drop);
        QCOMPARE(requests.count(), 0);
        QCOMPARE(snapshot(), before);
    }

    void mouseDragScrollsAtEdgesAndStopsAfterCancel()
    {
        auto &db = DatabaseManager::instance();
        for (int i = 0; i < 40; ++i)
            QVERIFY(db.addSubtask(m_task, QString("Scrolling subtask %1").arg(i)) > 0);
        auto *pane = m_window->findChild<TaskPane *>();
        pane->loadTasks(m_product);
        auto *list = pane->findChild<QListView *>();
        auto *bar = list->verticalScrollBar();
        const auto before = snapshot();
        QTRY_VERIFY_WITH_TIMEOUT(bar->maximum() > 0, 1000);
        const QPoint bottom(list->viewport()->width() / 2, list->viewport()->height() - 2);
        QTest::mousePress(list->viewport(), Qt::LeftButton, Qt::NoModifier, subtaskRect(m_second).center());
        moveMouseWithButton(list, bottom);
        QTRY_VERIFY_WITH_TIMEOUT(bar->value() > 0, 1500);
        const int scrolled = bar->value();
        const QPoint top(bottom.x(), 2);
        moveMouseWithButton(list, top);
        QTRY_VERIFY_WITH_TIMEOUT(bar->value() < scrolled, 1500);
        QTest::keyClick(list, Qt::Key_Escape);
        QTest::mouseRelease(list->viewport(), Qt::LeftButton, Qt::NoModifier, top);
        const int stopped = bar->value();
        QTest::qWait(200);
        QCOMPARE(bar->value(), stopped);
        QCOMPARE(snapshot(), before);
    }

    void mouseDragSaveFailurePreservesPendingContent()
    {
        auto *pane = m_window->findChild<TaskPane *>();
        auto *list = pane->findChild<QListView *>();
        auto *editor = m_window->findChild<EditorPane *>();
        editor->loadSubTask(m_second);
        editor->findChild<EditorBridge *>()->setContent("<p>Pending drag failure edit</p>");
        const QRect target = subtaskRect(m_first);
        const auto before = snapshot();
        QSqlQuery query;
        QVERIFY(query.exec("PRAGMA query_only=ON"));
        answerWarning();
        dragMouse(list, subtaskRect(m_second).center(), QPoint(target.center().x(), target.top() + 2));
        QVERIFY(query.exec("PRAGMA query_only=OFF"));
        QCOMPARE(snapshot(), before);
        QVERIFY(editor->saveCurrentContent());
        QCOMPARE(DatabaseManager::instance().getSubtask(m_second).content, "<p>Pending drag failure edit</p>");
    }

    void normalClicksStillOpenEditorAndStatusMenu()
    {
        auto *pane = m_window->findChild<TaskPane *>();
        auto *model = pane->findChild<TaskListModel *>();
        auto *list = pane->findChild<QListView *>();
        QSignalSpy requests(pane, &TaskPane::subTaskMoveRequested);
        QSignalSpy selected(pane, &TaskPane::subTaskSelected);
        const QPoint source = subtaskRect(m_second).center();
        const QPoint slightlyMoved = source + QPoint(1, 0);
        QCursor::setPos(list->viewport()->mapToGlobal(slightlyMoved));
        dragMouse(list, source, slightlyMoved);
        QCOMPARE(requests.count(), 0);
        QCOMPARE(selected.count(), 1);
        QCOMPARE(m_window->findChild<EditorPane *>()->currentSubTaskId(), m_second);
        QStyleOptionViewItem option;
        option.rect = subtaskRect(m_second);
        const QPoint status = TaskCardDelegate::subtaskStatusIconRect(option).center();
        QCursor::setPos(list->viewport()->mapToGlobal(status));
        bool statusOpened = false;
        QTimer::singleShot(0, this, [&]() {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (menu) {
                statusOpened = menu->actions().size() == 5;
                menu->close();
            }
        });
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, status);
        QVERIFY(statusOpened);
        QCOMPARE(requests.count(), 0);
        QVERIFY(model->index(model->rowForTaskId(m_task)).flags().testFlag(Qt::ItemIsDragEnabled));
        const int other = DatabaseManager::instance().addTask(m_product, "Another task");
        pane->loadTasks(m_product);
        const int sourceRow = model->rowForTaskId(m_task);
        const int destinationRow = model->rowCount();
        QVERIFY(model->moveRows({}, sourceRow, 1, {}, destinationRow));
        QVERIFY(model->rowForTaskId(other) < model->rowForTaskId(m_task));
    }

    void systemMouseDrag_data()
    {
        QTest::addColumn<bool>("reparent");
        QTest::newRow("reorder") << false;
        QTest::newRow("reparent") << true;
    }

    void systemMouseDrag()
    {
#ifdef Q_OS_WIN
        if (QGuiApplication::platformName() != "windows")
            QSKIP("OS mouse input requires the Windows platform plugin and an interactive desktop.");
        QFETCH(bool, reparent);
        if (reparent) showBothParents();
        auto *pane = m_window->findChild<TaskPane *>();
        auto *model = pane->findChild<TaskListModel *>();
        auto *list = pane->findChild<QListView *>();
        m_window->raise();
        m_window->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(m_window.get()));
        list->doItemsLayout();
        const QRect target = reparent
            ? list->visualRect(model->index(model->rowForTaskId(m_destination)))
            : subtaskRect(m_first);
        const QPoint to = list->viewport()->mapToGlobal(reparent
            ? target.center() : QPoint(target.center().x(), target.top() + 2));
        const QPoint from = list->viewport()->mapToGlobal(subtaskRect(m_second).center());
        const QPoint originalCursor = QCursor::pos();
        QCursor::setPos(from);
        QTest::qWait(100);
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        QVERIFY(SendInput(1, &input, sizeof(INPUT)) == 1);
        QTest::qWait(100);
        for (int step = 1; step <= 8; ++step) {
            QCursor::setPos(from + (to - from) * step / 8);
            QTest::qWait(40);
        }
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        const UINT released = SendInput(1, &input, sizeof(INPUT));
        QTest::qWait(100);
        QCursor::setPos(originalCursor);
        QVERIFY(released == 1);
        if (reparent) {
            QTRY_COMPARE(ids(m_destination), QList<int>({m_existing, m_second}));
        } else {
            QTRY_COMPARE(ids(m_task), QList<int>({m_second, m_first, m_third}));
        }
#else
        QSKIP("OS mouse input regression is Windows-specific.");
#endif
    }

    void cleanup()
    {
        m_window.reset();
        QSqlQuery query;
        QVERIFY(query.exec("PRAGMA query_only=OFF"));
        QVERIFY(query.exec("DROP TRIGGER IF EXISTS fail_move"));
        QVERIFY(query.exec("DELETE FROM products"));
    }
};

QTEST_MAIN(SubtaskMoveTest)
#include "SubtaskMoveTest.moc"
