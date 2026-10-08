#include "app/MainWindow.h"
#include "db/DatabaseManager.h"
#include "services/UpdateService.h"
#include "ui/EditorPane.h"
#include "ui/SettingsDialog.h"
#include "FakeUpdateNetwork.h"
#include <QAction>
#include <QCheckBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtTest>
#include <limits>
#include <memory>

class UpdateUiTest : public QObject
{
    Q_OBJECT
    QTemporaryDir m_data;
    std::unique_ptr<TestNetwork> m_network;
    std::unique_ptr<UpdateService> m_updater;
    std::unique_ptr<MainWindow> m_window;
    QStringList m_downloads;
    int m_updateAnswers = 0;
    int m_expectedMessageAnswers = 0;
    QPointer<QTimer> m_answerTimer;
    int m_taskId = -1;
    int m_subtaskId = -1;

    void captureDialog(QWidget *dialog, const QString &fileName)
    {
        const QString directory = qEnvironmentVariable("NEXUS_UPDATE_TEST_SCREENSHOTS");
        if (!directory.isEmpty()) {
            QVERIFY(dialog->grab().save(QDir(directory).filePath(fileName)));
        }
    }

    void triggerManualCheck()
    {
        for (auto *action : m_window->findChildren<QAction *>()) {
            if (action->text() == "Check for &Updates...") {
                action->trigger();
                return;
            }
        }
        QFAIL("Manual update action is missing");
    }

    void createWindow()
    {
        m_network = std::make_unique<TestNetwork>();
        m_updater = std::make_unique<UpdateService>(m_network.get());
        connect(m_updater.get(), &UpdateService::downloadFinished, this,
                [this](const QString &path) { m_downloads.append(path); });
        m_window = std::make_unique<MainWindow>(nullptr, m_updater.get());
        m_window->hide();
    }

    void answerUpdate(bool download, int months = 0, bool close = false)
    {
        m_answerTimer = new QTimer(this);
        connect(m_answerTimer, &QTimer::timeout, this, [this, download, months, close]() {
            auto *dialog = m_window->findChild<QDialog *>("updateAvailableDialog");
            if (!dialog || !dialog->isVisible()) return;
            m_answerTimer->stop();
            ++m_updateAnswers;
            auto *delay = dialog->findChild<QComboBox *>("updateReminderDelay");
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            QVERIFY(delay);
            QVERIFY(buttons);
            QCOMPARE(delay->count(), 4);
            delay->setCurrentIndex(delay->findData(months));
            QCOMPARE(delay->currentData().toInt(), months);
            captureDialog(dialog, QString("update-prompt-%1-months.png").arg(months));
            QVERIFY(buttons->button(QDialogButtonBox::No)->isDefault());
            // A second result must not open another prompt while this one is active.
            emit m_updater->updateAvailable("v1.0.9", installerUrl("v1.0.9"), {}, true);
            if (close) dialog->close();
            else buttons->button(download ? QDialogButtonBox::Yes : QDialogButtonBox::No)->click();
        });
        m_answerTimer->start(10);
    }

    void checkRelease(bool manual, const QString &version = "v1.0.8")
    {
        if (!manual) {
            m_updater->setAutomaticChecksEnabled(true);
            QCoreApplication::processEvents();
        }
        m_updater->checkForUpdate(manual);
        QVERIFY(m_network->reply);
        m_network->reply->finish(releaseData(version));
        if (m_answerTimer) {
            m_answerTimer->stop();
            m_answerTimer->deleteLater();
            m_answerTimer.clear();
        }
        QCoreApplication::processEvents();
    }

    QProgressDialog *downloadDialog() const
    {
        return m_window->findChild<QProgressDialog *>("updateDownloadProgress");
    }

    void answerDialogs(const QStringList &answers)
    {
        m_expectedMessageAnswers += answers.size();
        auto *timer = new QTimer(this);
        auto pending = std::make_shared<QStringList>(answers);
        connect(timer, &QTimer::timeout, this, [this, timer, pending]() {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box || pending->isEmpty()) return;
            for (auto *button : box->buttons()) {
                if (button->text() == pending->first()) {
                    pending->removeFirst();
                    --m_expectedMessageAnswers;
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
    }

    void init()
    {
        auto &db = DatabaseManager::instance();
        db.setSetting("check_updates", "false");
        db.setSetting("auto_install_updates", "true");
        db.setSetting("update_reminder_after", "");
        m_updateAnswers = 0;
        m_expectedMessageAnswers = 0;
        createWindow();
        QVERIFY(!m_updater->findChild<QTimer *>("updateCheckTimer")->isActive());
    }

    void cleanup()
    {
        QSqlQuery query;
        QVERIFY(query.exec("PRAGMA query_only=OFF"));
        m_window.reset();
        m_updater.reset();
        m_network.reset();
        for (const QString &path : m_downloads) {
            if (QFile::exists(path)) QVERIFY(QFile::remove(path));
        }
        m_downloads.clear();
        QCOMPARE(m_expectedMessageAnswers, 0);
    }

    void deferringNeverDownloadsAndPersistsDeadline_data()
    {
        QTest::addColumn<int>("months");
        QTest::addColumn<bool>("close");
        QTest::newRow("six-hours") << 0 << false;
        QTest::newRow("one-month") << 1 << false;
        QTest::newRow("two-months") << 2 << false;
        QTest::newRow("three-months") << 3 << false;
        QTest::newRow("window-close") << 0 << true;
    }

    void deferringNeverDownloadsAndPersistsDeadline()
    {
        QFETCH(int, months);
        QFETCH(bool, close);
        const auto before = QDateTime::currentDateTimeUtc();
        answerUpdate(false, months, close);
        checkRelease(true);
        const auto after = QDateTime::currentDateTimeUtc();
        QCOMPARE(m_updateAnswers, 1);
        QCOMPARE(m_network->requests.size(), 1);
        QVERIFY(!downloadDialog());
        const auto deadline = QDateTime::fromString(
            DatabaseManager::instance().getSetting("update_reminder_after"), Qt::ISODateWithMs);
        QVERIFY(deadline.isValid());
        QVERIFY(deadline >= (months ? before.addMonths(months) : before.addSecs(6 * 3600)));
        QVERIFY(deadline <= (months ? after.addMonths(months) : after.addSecs(6 * 3600)));
    }

    void snoozeSurvivesReopenAndSuppressesAllAutomaticVersions()
    {
        answerUpdate(false, 2);
        checkRelease(true);
        const QString saved = DatabaseManager::instance().getSetting("update_reminder_after");
        m_window.reset();
        m_updater.reset();
        m_network.reset();
        {
            auto connection = QSqlDatabase::database();
            connection.close();
            QVERIFY(connection.open());
        }
        createWindow();
        m_updateAnswers = 0;
        for (const auto &version : {"v1.0.8", "v1.0.9"}) {
            answerUpdate(false);
            checkRelease(false, version);
            QCOMPARE(m_updateAnswers, 0);
            QVERIFY(!downloadDialog());
        }
        QCOMPARE(m_network->requests.size(), 2);
        QCOMPARE(DatabaseManager::instance().getSetting("update_reminder_after"), saved);
        QVERIFY(m_window->isHidden());
    }

    void manualCheckBypassesSnoozeWithoutShorteningItOnNotNow()
    {
        auto &db = DatabaseManager::instance();
        const QString saved = QDateTime::currentDateTimeUtc().addMonths(3).toString(Qt::ISODateWithMs);
        db.setSetting("update_reminder_after", saved);
        answerUpdate(false);
        triggerManualCheck();
        checkRelease(true);
        QCOMPARE(m_updateAnswers, 1);
        QCOMPARE(m_network->requests.size(), 1);
        QCOMPARE(db.getSetting("update_reminder_after"), saved);
    }

    void expiredSnoozeRemindsAboutTheSameRelease()
    {
        auto &db = DatabaseManager::instance();
        db.setSetting("update_reminder_after",
                      QDateTime::currentDateTimeUtc().addMonths(1).toString(Qt::ISODateWithMs));
        answerUpdate(false);
        checkRelease(false);
        QCOMPARE(m_updateAnswers, 0);
        db.setSetting("update_reminder_after",
                      QDateTime::currentDateTimeUtc().addSecs(-1).toString(Qt::ISODateWithMs));
        answerUpdate(false);
        checkRelease(false);
        QCOMPARE(m_updateAnswers, 1);
        QCOMPARE(m_network->requests.size(), 2);
    }

    void invalidSnoozeIsReportedAndDoesNotBlockReminders()
    {
        DatabaseManager::instance().setSetting("update_reminder_after", "invalid-date");
        QTest::ignoreMessage(QtWarningMsg, "Invalid update reminder date; reminders will resume.");
        answerUpdate(false);
        checkRelease(false);
        QCOMPARE(m_updateAnswers, 1);
    }

    void failedSnoozeSaveIsReported()
    {
        QSqlQuery query;
        QVERIFY(query.exec("PRAGMA query_only=ON"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Failed to save setting"));
        answerUpdate(false, 1);
        answerDialogs({"OK"});
        checkRelease(true);
        QVERIFY(DatabaseManager::instance().getSetting("update_reminder_after").isEmpty());
        QCOMPARE(m_network->requests.size(), 1);
    }

    void yesShowsProgressAndClearsSnooze()
    {
        auto &db = DatabaseManager::instance();
        db.setSetting("update_reminder_after",
                      QDateTime::currentDateTimeUtc().addMonths(1).toString(Qt::ISODateWithMs));
        answerUpdate(true);
        checkRelease(true);
        QCOMPARE(m_updateAnswers, 1);
        QCOMPARE(m_network->requests.size(), 2);
        QCOMPARE(m_network->requests.last().url(), QUrl(installerUrl()));
        QVERIFY(db.getSetting("update_reminder_after").isEmpty());
        QPointer<QProgressDialog> dialog = downloadDialog();
        QVERIFY(dialog);
        QVERIFY(dialog->isVisible());
        QCOMPARE(dialog->maximum(), 0);
        emit m_network->reply->downloadProgress(1024, 4096);
        QCOMPARE(dialog->maximum(), 100);
        QCOMPARE(dialog->value(), 25);
        QVERIFY(dialog->labelText().contains("25%"));
        QVERIFY(dialog->labelText().contains("/"));
        captureDialog(dialog, "download-progress.png");
        triggerManualCheck();
        QCOMPARE(m_network->requests.size(), 2);
        emit m_network->reply->downloadProgress(2048, -1);
        QCOMPARE(dialog->maximum(), 0);
        QVERIFY(dialog->labelText().contains("unknown"));
        QVERIFY(!dialog->findChild<QProgressBar *>()->isTextVisible());
        captureDialog(dialog, "download-unknown-size.png");
        const auto largeSize = std::numeric_limits<qint64>::max();
        emit m_network->reply->downloadProgress(largeSize / 2, largeSize);
        QVERIFY(dialog->value() >= 49 && dialog->value() <= 50);
        emit m_network->reply->downloadProgress(4096, 4096);
        QCOMPARE(dialog->value(), 100);
        QVERIFY(dialog->isVisible());
        answerDialogs({"Later"});
        m_network->reply->finish("test installer - never executed");
        QVERIFY(!dialog || !dialog->isVisible());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!downloadDialog());
        QVERIFY(!m_updater->isBusy());
        QCOMPARE(m_downloads.size(), 1);
        answerDialogs({"Later"});
        triggerManualCheck();
        QCOMPARE(m_network->requests.size(), 2);
    }

    void cachedDownloadClosesProgressWithoutAnotherRequest()
    {
        answerUpdate(true);
        checkRelease(true);
        answerDialogs({"Later"});
        m_network->reply->finish("test installer - never executed");
        answerUpdate(true);
        answerDialogs({"Later"});
        checkRelease(true);
        QCOMPARE(m_network->requests.size(), 3);
        QCOMPARE(m_downloads.size(), 2);
        QCOMPARE(m_downloads.first(), m_downloads.last());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!downloadDialog());
    }

    void closingProgressAbortsDownloadAndAllowsRetry()
    {
        answerUpdate(true);
        checkRelease(true);
        QSignalSpy cancelled(m_updater.get(), &UpdateService::downloadCanceled);
        QSignalSpy errors(m_updater.get(), &UpdateService::error);
        auto *dialog = downloadDialog();
        QVERIFY(dialog);
        dialog->close();
        QCOMPARE(cancelled.count(), 1);
        QCOMPARE(errors.count(), 0);
        QVERIFY(!m_updater->isBusy());
        QVERIFY(m_downloads.isEmpty());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!downloadDialog());
        answerUpdate(true);
        checkRelease(true);
        QCOMPARE(m_network->requests.size(), 4);
        QVERIFY(downloadDialog()->isVisible());
        auto *cancel = downloadDialog()->findChild<QPushButton *>();
        QVERIFY(cancel);
        cancel->click();
        QCOMPARE(cancelled.count(), 2);
    }

    void downloadFailureClosesProgressAndShowsError()
    {
        answerUpdate(true);
        checkRelease(true);
        QVERIFY(downloadDialog());
        QTest::ignoreMessage(QtWarningMsg, "Update: Download failed: simulated failure");
        answerDialogs({"OK"});
        m_network->reply->finish({}, QNetworkReply::TimeoutError);
        QVERIFY(!m_updater->isBusy());
        QVERIFY(m_downloads.isEmpty());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!downloadDialog());
    }

    void invalidDownloadUrlClosesProgressAndShowsError()
    {
        answerUpdate(true);
        answerDialogs({"OK"});
        QTest::ignoreMessage(QtWarningMsg, "Update: Invalid Nexus installer download URL");
        emit m_updater->updateAvailable("v1.0.8", "http://example.com/setup.exe", {}, true);
        QVERIFY(m_network->requests.isEmpty());
        QVERIFY(m_downloads.isEmpty());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!downloadDialog());
        m_answerTimer->deleteLater();
        m_answerTimer.clear();
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
        QVERIFY(!m_updater->isBusy());
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

    void settingsDescribeSixHoursAndRequireDownloadConsent()
    {
        SettingsDialog settings;
        bool foundCheck = false;
        bool foundDownload = false;
        for (auto *box : settings.findChildren<QCheckBox *>()) {
            if (box->text() == "Check for updates on startup and every 6 hours") {
                foundCheck = true;
                box->setChecked(true);
            }
            if (box->text() == "Automatically download updates (ask before installing)") {
                foundDownload = true;
                box->setChecked(false);
            }
        }
        QVERIFY(foundCheck);
        QVERIFY(!foundDownload);
        QVERIFY(QMetaObject::invokeMethod(&settings, "onSave"));
        QCOMPARE(DatabaseManager::instance().getSetting("check_updates"), "true");
        DatabaseManager::instance().setSetting("check_updates", "false");
    }

    void settingsShowSavedReminderDate()
    {
        const auto deadline = QDateTime::currentDateTimeUtc().addMonths(2);
        DatabaseManager::instance().setSetting("update_reminder_after", deadline.toString(Qt::ISODateWithMs));
        SettingsDialog settings;
        bool found = false;
        for (auto *label : settings.findChildren<QLabel *>()) {
            if (label->text().startsWith("Automatic reminders paused until ")) {
                found = true;
                QVERIFY(label->text().contains(QLocale().toString(deadline.toLocalTime(), QLocale::ShortFormat)));
            }
        }
        QVERIFY(found);
    }

    void settingsApplyWithoutRestartWhileWindowIsHidden()
    {
        auto *timer = m_updater->findChild<QTimer *>("updateCheckTimer");
        for (bool enabled : {true, false}) {
            QTimer::singleShot(0, this, [this, enabled]() {
                auto *dialog = m_window->findChild<SettingsDialog *>();
                QVERIFY(dialog);
                for (auto *box : dialog->findChildren<QCheckBox *>()) {
                    if (box->text() == "Check for updates on startup and every 6 hours")
                        box->setChecked(enabled);
                }
                QVERIFY(QMetaObject::invokeMethod(dialog, "onSave"));
            });
            QVERIFY(QMetaObject::invokeMethod(m_window.get(), "showSettings"));
            QCOMPARE(timer->isActive(), enabled);
            QVERIFY(m_window->isHidden());
            // Stop before processing the queued startup request: this test never accesses GitHub.
            if (enabled) m_updater->setAutomaticChecksEnabled(false);
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
