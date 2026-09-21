#include "app/MainWindow.h"
#include "db/DatabaseManager.h"
#include "services/UpdateService.h"
#include "ui/EditorPane.h"
#include "ui/SettingsDialog.h"
#include <QAction>
#include <QCheckBox>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtTest>
#include <memory>

class UpdateUiTest : public QObject
{
    Q_OBJECT
    QTemporaryDir m_data;
    std::unique_ptr<MainWindow> m_window;
    int m_taskId = -1;
    int m_subtaskId = -1;

    void answerDialogs(const QStringList &answers)
    {
        auto *timer = new QTimer(this);
        auto pending = std::make_shared<QStringList>(answers);
        connect(timer, &QTimer::timeout, this, [timer, pending]() {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box || pending->isEmpty()) return;
            for (auto *button : box->buttons()) {
                if (button->text() == pending->first()) {
                    pending->removeFirst();
                    button->click();
                    if (pending->isEmpty()) {
                        timer->stop();
                        timer->deleteLater();
                    }
                    return;
                }
            }
        });
        timer->start(10);
        QTimer::singleShot(5000, timer, [timer, pending]() {
            if (!pending->isEmpty()) {
                QTest::qFail("Expected update dialog was not displayed", __FILE__, __LINE__);
                if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                    box->reject();
                timer->stop();
                timer->deleteLater();
            }
        });
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setApplicationName("NexusUpdateTest");
        QCoreApplication::setOrganizationName("NexusUpdateTest");
        QCoreApplication::setApplicationVersion("1.0.7");
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_data.isValid());
        auto &db = DatabaseManager::instance();
        QVERIFY(db.initialize(m_data.filePath("test.db")));
        db.setSetting("check_updates", "false");
        const int productId = db.addProduct("Update tests");
        m_taskId = db.addTask(productId, "Test task");
        m_subtaskId = db.addSubtask(m_taskId, "Test subtask");
        QVERIFY(m_taskId > 0);
        QVERIFY(m_subtaskId > 0);
        m_window = std::make_unique<MainWindow>();
        m_window->hide();
        QVERIFY(!UpdateService::instance().findChild<QTimer *>("updateCheckTimer")->isActive());
    }

    void savesTaskAndSubtaskWithoutWaitingForAutosave()
    {
        auto *editor = m_window->findChild<EditorPane *>();
        QVERIFY(editor);
        auto *bridge = editor->findChild<EditorBridge *>();
        editor->loadTask(m_taskId);
        bridge->setContent("<p>Pending task edit</p>");
        QVERIFY(editor->saveCurrentContent());
        QCOMPARE(DatabaseManager::instance().getTask(m_taskId).content, "<p>Pending task edit</p>");
        editor->loadSubTask(m_subtaskId);
        bridge->setContent("<p>Pending subtask edit</p>");
        QVERIFY(editor->saveCurrentContent());
        QCOMPARE(DatabaseManager::instance().getSubtask(m_subtaskId).content, "<p>Pending subtask edit</p>");
    }

    void titleMessageCannotOverwriteNoteOnUpdate()
    {
        auto *editor = m_window->findChild<EditorPane *>();
        editor->loadTask(m_taskId);
        auto *bridge = editor->findChild<EditorBridge *>();
        bridge->setContent("<p>Keep this body</p>");
        bridge->setContent("__TITLE__:Renamed task");
        QVERIFY(editor->saveCurrentContent());
        QCOMPARE(DatabaseManager::instance().getTask(m_taskId).content, "<p>Keep this body</p>");
        QCOMPARE(DatabaseManager::instance().getTask(m_taskId).title, "Renamed task");
    }

    void laterDoesNotCloseApplicationAndManualActionReopensPrompt()
    {
        QSignalSpy quitting(qApp, &QCoreApplication::aboutToQuit);
        QTemporaryFile installer(m_data.filePath("deferred-XXXXXX.exe"));
        QVERIFY(installer.open());
        const QString path = installer.fileName();
        installer.close();
        answerDialogs({"Later"});
        QVERIFY(QMetaObject::invokeMethod(m_window.get(), "onDownloadFinished",
                                          Q_ARG(QString, path)));
        QCOMPARE(quitting.count(), 0);
        bool triggered = false;
        for (auto *action : m_window->findChildren<QAction *>()) {
            if (action->text() == "Check for &Updates...") {
                answerDialogs({"Later"});
                action->trigger();
                triggered = true;
                break;
            }
        }
        QVERIFY(triggered);
        QCOMPARE(quitting.count(), 0);
        QVERIFY(!UpdateService::instance().isBusy());
    }

    void installIsBlockedWhenNoteCannotBeSaved()
    {
        auto *editor = m_window->findChild<EditorPane *>();
        editor->loadSubTask(m_subtaskId);
        editor->findChild<EditorBridge *>()->setContent("<p>Unsaved edit</p>");
        QSqlQuery query;
        QVERIFY(query.exec("PRAGMA query_only=ON"));
        QSignalSpy quitting(qApp, &QCoreApplication::aboutToQuit);
        QTest::ignoreMessage(QtWarningMsg, qPrintable(
            QString("Failed to save subtask content: %1").arg(m_subtaskId)));
        answerDialogs({"Install now", "OK"});
        QVERIFY(QMetaObject::invokeMethod(m_window.get(), "onDownloadFinished",
                                          Q_ARG(QString, m_data.filePath("missing-installer.exe"))));
        QVERIFY(query.exec("PRAGMA query_only=OFF"));
        QCOMPARE(quitting.count(), 0);
        QVERIFY(editor->saveCurrentContent());
        QCOMPARE(DatabaseManager::instance().getSubtask(m_subtaskId).content, "<p>Unsaved edit</p>");
    }

    void failedInstallerLaunchKeepsApplicationOpen()
    {
        QSignalSpy quitting(qApp, &QCoreApplication::aboutToQuit);
        answerDialogs({"Install now", "OK"});
        QVERIFY(QMetaObject::invokeMethod(m_window.get(), "onDownloadFinished",
                                          Q_ARG(QString, m_data.filePath("missing-installer.exe"))));
        QCOMPARE(quitting.count(), 0);
    }

    void settingsDescribeIntervalAndPersistExistingKeys()
    {
        SettingsDialog settings;
        bool foundCheck = false;
        bool foundDownload = false;
        for (auto *box : settings.findChildren<QCheckBox *>()) {
            if (box->text() == "Check for updates on startup and every 2 hours") {
                foundCheck = true;
                box->setChecked(true);
            }
            if (box->text() == "Automatically download updates (ask before installing)") {
                foundDownload = true;
                box->setChecked(false);
            }
        }
        QVERIFY(foundCheck);
        QVERIFY(foundDownload);
        QVERIFY(QMetaObject::invokeMethod(&settings, "onSave"));
        QCOMPARE(DatabaseManager::instance().getSetting("check_updates"), "true");
        QCOMPARE(DatabaseManager::instance().getSetting("auto_install_updates"), "false");
        DatabaseManager::instance().setSetting("check_updates", "false");
    }

    void settingsApplyWithoutRestartWhileWindowIsHidden()
    {
        auto *timer = UpdateService::instance().findChild<QTimer *>("updateCheckTimer");
        for (bool enabled : {true, false}) {
            QTimer::singleShot(0, this, [this, enabled]() {
                auto *dialog = m_window->findChild<SettingsDialog *>();
                QVERIFY(dialog);
                for (auto *box : dialog->findChildren<QCheckBox *>()) {
                    if (box->text() == "Check for updates on startup and every 2 hours")
                        box->setChecked(enabled);
                }
                QVERIFY(QMetaObject::invokeMethod(dialog, "onSave"));
            });
            QVERIFY(QMetaObject::invokeMethod(m_window.get(), "showSettings"));
            QCOMPARE(timer->isActive(), enabled);
            QVERIFY(m_window->isHidden());
            // Stop before processing the queued startup request: this test never accesses GitHub.
            if (enabled) UpdateService::instance().setAutomaticChecksEnabled(false);
        }
    }

    void cleanupTestCase()
    {
        m_window.reset();
        QSqlDatabase::database().close();
    }
};

QTEST_MAIN(UpdateUiTest)
#include "UpdateUiTest.moc"
