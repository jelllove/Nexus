#include <QtTest>
#include <QComboBox>
#include <QDateEdit>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextBrowser>
#include <QLabel>
#include <QRegularExpression>
#include <QTimeZone>
#include <QTimer>
#include <memory>
#include "db/DatabaseManager.h"
#include "services/ActivitySummaryService.h"
#include "services/AIService.h"
#include "ui/ActivitySummaryDialog.h"
#include "ui/MarkdownPreviewDialog.h"

class FakeAiServer : public QTcpServer
{
public:
    QByteArray body;
    QByteArray authorization;
    QString response = R"({"choices":[{"message":{"content":"# Weekly summary\nSynthetic result"}}]})";
    bool holdResponse = false;
    int statusCode = 200;
    int requests = 0;

    FakeAiServer()
    {
        connect(this, &QTcpServer::newConnection, this, [this]() {
            auto *socket = nextPendingConnection();
            auto data = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket, data]() {
                data->append(socket->readAll());
                int boundary = data->indexOf("\r\n\r\n");
                if (boundary < 0 || socket->property("handled").toBool()) return;
                const auto headers = data->left(boundary);
                const auto match = QRegularExpression("Content-Length: (\\d+)", QRegularExpression::CaseInsensitiveOption)
                                       .match(QString::fromUtf8(headers));
                if (!match.hasMatch() || data->size() < boundary + 4 + match.captured(1).toInt()) return;
                socket->setProperty("handled", true);
                ++requests;
                for (const auto &line : headers.split('\n'))
                    if (line.trimmed().toLower().startsWith("authorization:")) authorization = line.trimmed();
                body = data->mid(boundary + 4);
                if (holdResponse) return;
                const auto bytes = response.toUtf8();
                socket->write("HTTP/1.1 " + QByteArray::number(statusCode)
                              + " Test response\r\nContent-Type: application/json\r\nContent-Length: "
                              + QByteArray::number(bytes.size()) + "\r\nConnection: close\r\n\r\n" + bytes);
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
    }
};

class ActivitySummaryTest : public QObject
{
    Q_OBJECT
    int m_product = -1;
    int m_task = -1;

    void setDates(const QString &table, int id, const QString &created, const QString &updated)
    {
        QSqlQuery query;
        query.prepare("UPDATE " + table + " SET created_at = ?, updated_at = ? WHERE id = ?");
        query.addBindValue(created);
        query.addBindValue(updated);
        query.addBindValue(id);
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
    }

    void clearEvents()
    {
        QSqlQuery query;
        QVERIFY2(query.exec("DELETE FROM database_operations"), qPrintable(query.lastError().text()));
    }

    bool collect(ActivityTimeBasis basis, QList<TaskActivity> &items, QString &error)
    {
        return DatabaseManager::instance().collectTaskActivity(
            QDateTime::fromString("2026-10-01T00:00:00Z", Qt::ISODate),
            QDateTime::fromString("2026-10-08T00:00:00Z", Qt::ISODate), basis, items, error);
    }

    void configureAi(FakeAiServer &server)
    {
        QVERIFY(server.listen(QHostAddress::LocalHost));
        auto &db = DatabaseManager::instance();
        QVERIFY(db.setSetting("ai_endpoint", QString("http://127.0.0.1:%1/chat").arg(server.serverPort())));
        QVERIFY(db.setSetting("ai_api_key", "synthetic-test-key"));
        QVERIFY(db.setSetting("ai_model", "configured-test-model"));
    }

    void prepareDialog(ActivitySummaryDialog &dialog)
    {
        dialog.show();
        dialog.findChild<QComboBox *>("summaryPeriod")->setCurrentIndex(5);
        dialog.findChild<QDateEdit *>("summaryStart")->setDate(QDate(2026, 10, 1));
        dialog.findChild<QDateEdit *>("summaryEnd")->setDate(QDate(2026, 10, 7));
        dialog.findChild<QPushButton *>("summaryPreview")->click();
    }

    void answerConsent(QMessageBox::StandardButton answer)
    {
        QTimer::singleShot(0, this, [answer]() {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            QCOMPARE(box->defaultButton(), box->button(QMessageBox::No));
            box->button(answer)->click();
        });
    }

private slots:
    void initTestCase()
    {
        QVERIFY(DatabaseManager::instance().initialize(":memory:"));
    }

    void init()
    {
        QSqlQuery query;
        QVERIFY(query.exec("DELETE FROM products"));
        QVERIFY(query.exec("DELETE FROM settings"));
        m_product = DatabaseManager::instance().addProduct("Synthetic product");
        m_task = DatabaseManager::instance().addTask(m_product, "Synthetic task");
        QVERIFY(m_product > 0 && m_task > 0);
        setDates("tasks", m_task, "2026-09-01 00:00:00", "2026-09-01 00:00:00");
        clearEvents();
    }

    void filtersCreationAndModificationSeparately()
    {
        auto &db = DatabaseManager::instance();
        const int newTask = db.addTask(m_product, "Created in range");
        setDates("tasks", newTask, "2026-10-01 00:00:00", "2026-10-09 00:00:00");
        setDates("tasks", m_task, "2026-09-01 00:00:00", "2026-10-07 23:59:59");
        const int outside = db.addTask(m_product, "End boundary excluded");
        setDates("tasks", outside, "2026-10-08 00:00:00", "2026-10-08 00:00:00");
        clearEvents();
        QList<TaskActivity> items;
        QString error;
        QVERIFY2(collect(ActivityTimeBasis::Created, items, error), qPrintable(error));
        QCOMPARE(items.size(), 1);
        QCOMPARE(items[0].id, newTask);
        QCOMPARE(items[0].createdAt,
                 QDateTime::fromString("2026-10-01T00:00:00Z", Qt::ISODate));
        QVERIFY(collect(ActivityTimeBasis::Modified, items, error));
        QCOMPARE(items.size(), 1);
        QCOMPARE(items[0].id, m_task);
        QVERIFY(collect(ActivityTimeBasis::CreatedOrModified, items, error));
        QCOMPARE(items.size(), 2);
        QVERIFY(error.isEmpty());
    }

    void includesSubtasksAndArchivedDeletedTasks()
    {
        auto &db = DatabaseManager::instance();
        const int subtask = db.addSubtask(m_task, "Changed child");
        QVERIFY(db.archiveProduct(m_product));
        QVERIFY(db.deleteTask(m_task));
        setDates("subtasks", subtask, "2026-09-01 00:00:00", "2026-10-04 12:00:00");
        clearEvents();
        QList<TaskActivity> items;
        QString error;
        QVERIFY(collect(ActivityTimeBasis::Modified, items, error));
        QCOMPARE(items.size(), 1);
        QCOMPARE(items[0].tableName, QString("subtasks"));
        QCOMPARE(items[0].parentTitle, QString("Synthetic task"));
        QCOMPARE(items[0].lifecycleStatus, QString("deleted"));
    }

    void retainedEventsFindOverwrittenModificationTime()
    {
        auto &db = DatabaseManager::instance();
        QVERIFY(db.updateTaskWorkStatus(m_task, TaskWorkStatus::Completed));
        QSqlQuery query;
        QVERIFY(query.exec("UPDATE database_operations SET occurred_at = '2026-10-03T10:00:00.000Z'"));
        setDates("tasks", m_task, "2026-09-01 00:00:00", "2026-10-09 12:00:00");
        QList<TaskActivity> items;
        QString error;
        QVERIFY(collect(ActivityTimeBasis::Modified, items, error));
        QCOMPARE(items.size(), 1);
        QCOMPARE(items[0].changes.size(), 1);
        QCOMPARE(items[0].changes[0].newWorkStatus, 3);
        QVERIFY(collect(ActivityTimeBasis::Created, items, error));
        QVERIFY(items.isEmpty());
    }

    void noDataInvalidRangeAndSqlErrorsAreExplicit()
    {
        QString payload, error;
        QVERIFY(!ActivitySummaryService::prepare(QDate(2026, 10, 1), QDate(2026, 10, 7),
                    ActivityTimeBasis::Created, payload, error));
        QVERIFY(error.contains("No tasks"));
        QVERIFY(payload.isEmpty());
        QVERIFY(!ActivitySummaryService::prepare(QDate(2026, 10, 8), QDate(2026, 10, 7),
                    ActivityTimeBasis::Created, payload, error));
        QVERIFY(error.contains("date", Qt::CaseInsensitive));
        QSqlQuery query;
        QVERIFY(query.exec("ALTER TABLE database_operations RENAME TO hidden_operations"));
        QList<TaskActivity> items;
        const bool result = collect(ActivityTimeBasis::Modified, items, error);
        QVERIFY(query.exec("ALTER TABLE hidden_operations RENAME TO database_operations"));
        QVERIFY(!result);
        QVERIFY(!error.isEmpty());
        QVERIFY(items.isEmpty());
    }

    void payloadUsesPlainTextAndExcludesSettings()
    {
        auto &db = DatabaseManager::instance();
        QVERIFY(db.updateTaskContent(m_task, "<p>Current &amp; saved</p><img src='file:///private.png'>"));
        QVERIFY(db.setSetting("ai_api_key", "never-send-this-secret"));
        setDates("tasks", m_task, "2026-10-03 12:00:00", "2026-10-03 13:00:00");
        QString payload, error;
        QVERIFY2(ActivitySummaryService::prepare(QDate(2026, 10, 1), QDate(2026, 10, 7),
                    ActivityTimeBasis::CreatedOrModified, payload, error), qPrintable(error));
        auto json = QJsonDocument::fromJson(payload.toUtf8()).object();
        QCOMPARE(json["items"].toArray().size(), 1);
        QCOMPARE(json["items"].toArray()[0].toObject()["currentContent"].toString(), QString("Current & saved"));
        QVERIFY(!payload.contains("never-send-this-secret"));
        QVERIFY(!payload.contains("private.png"));
        QVERIFY(payload.contains("not historical snapshots"));
        QVERIFY(json.contains("startUtc"));
        QVERIFY(json.contains("endUtcExclusive"));
    }

    void refusesOversizedInputWithoutTruncating()
    {
        auto &db = DatabaseManager::instance();
        QVERIFY(db.updateTaskContent(m_task, QString(65000, 'x')));
        setDates("tasks", m_task, "2026-10-03 12:00:00", "2026-10-03 13:00:00");
        QString payload, error;
        QVERIFY(!ActivitySummaryService::prepare(QDate(2026, 10, 1), QDate(2026, 10, 7),
                    ActivityTimeBasis::Created, payload, error));
        QVERIFY(payload.isEmpty());
        QVERIFY(error.contains("large", Qt::CaseInsensitive));
    }

    void refusesTooManyItemsAndEvents()
    {
        QSqlQuery query;
        query.prepare("WITH RECURSIVE n(x) AS (SELECT 1 UNION ALL SELECT x+1 FROM n WHERE x<1001) "
                      "INSERT INTO tasks(product_id, title, created_at, updated_at) "
                      "SELECT ?, 'Synthetic bulk task', '2026-10-02 12:00:00', '2026-10-02 12:00:00' FROM n");
        query.addBindValue(m_product);
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        QList<TaskActivity> items;
        QString error;
        QVERIFY(!collect(ActivityTimeBasis::Created, items, error));
        QVERIFY(error.contains("1000"));
        QVERIFY(items.isEmpty());
        query.prepare("DELETE FROM tasks WHERE id != ?");
        query.addBindValue(m_task);
        QVERIFY(query.exec());
        clearEvents();
        query.prepare("WITH RECURSIVE n(x) AS (SELECT 1 UNION ALL SELECT x+1 FROM n WHERE x<2001) "
                      "INSERT INTO database_operations(occurred_at, table_name, record_id, operation, changed_fields) "
                      "SELECT '2026-10-03T12:00:00.000Z', 'tasks', ?, 'UPDATE', 'title' FROM n");
        query.addBindValue(QString::number(m_task));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        QVERIFY(!collect(ActivityTimeBasis::Modified, items, error));
        QVERIFY(error.contains("2000"));
        QVERIFY(items.isEmpty());
    }

    void presetsUseCalendarDates()
    {
        const QDate today(2026, 3, 31);
        QCOMPARE(ActivitySummaryService::presetStart(0, today), today);
        QCOMPARE(ActivitySummaryService::presetStart(1, today), today.addDays(-6));
        QCOMPARE(ActivitySummaryService::presetStart(2, today), today.addDays(-13));
        QCOMPARE(ActivitySummaryService::presetStart(3, today), QDate(2026, 3, 1));
        QCOMPARE(ActivitySummaryService::presetStart(4, today), QDate(2026, 1, 1));
    }

    void localDatesProduceUtcBoundaries()
    {
        setDates("tasks", m_task, "2026-10-03 12:00:00", "2026-10-03 13:00:00");
        QString payload, error;
        QVERIFY(ActivitySummaryService::prepare(QDate(2026, 10, 1), QDate(2026, 10, 7),
                                                ActivityTimeBasis::Created, payload, error));
        auto json = QJsonDocument::fromJson(payload.toUtf8()).object();
        QCOMPARE(json["startUtc"].toString(),
                 QDate(2026, 10, 1).startOfDay().toUTC().toString(Qt::ISODateWithMs));
        QCOMPARE(json["endUtcExclusive"].toString(),
                 QDate(2026, 10, 8).startOfDay().toUTC().toString(Qt::ISODateWithMs));
    }

    void previewSavesPendingEditsOrStopsOnFailure()
    {
        bool called = false;
        ActivitySummaryDialog dialog(nullptr, [&]() {
            called = true;
            return false;
        });
        prepareDialog(dialog);
        QVERIFY(called);
        QVERIFY(!dialog.findChild<QPushButton *>("summaryGenerate")->isEnabled());
        QVERIFY(dialog.findChild<QLabel *>("summaryStatus")->text().contains("Could not save"));
        dialog.close();
    }

    void dialogPreviewAndCancellationDoNotSend()
    {
        ActivitySummaryDialog dialog;
        dialog.show();
        auto *preset = dialog.findChild<QComboBox *>("summaryPeriod");
        auto *start = dialog.findChild<QDateEdit *>("summaryStart");
        auto *end = dialog.findChild<QDateEdit *>("summaryEnd");
        QVERIFY(preset && start && end);
        QCOMPARE(preset->count(), 6);
        preset->setCurrentIndex(5);
        start->setDate(QDate(2026, 10, 1));
        end->setDate(QDate(2026, 10, 7));
        setDates("tasks", m_task, "2026-10-03 12:00:00", "2026-10-03 13:00:00");
        auto *preview = dialog.findChild<QPushButton *>("summaryPreview");
        auto *send = dialog.findChild<QPushButton *>("summaryGenerate");
        QVERIFY(preview && send);
        QVERIFY(!send->isEnabled());
        preview->click();
        QVERIFY(send->isEnabled());
        QVERIFY(dialog.findChild<QTextBrowser *>("summarySource")->toPlainText().contains("Synthetic task"));
        start->setDate(QDate(2026, 10, 2));
        QVERIFY(!send->isEnabled());
        QVERIFY(dialog.findChild<QTextBrowser *>("summarySource")->toPlainText().isEmpty());
        dialog.close();
    }

    void configuredAiRequestAndErrors()
    {
        FakeAiServer server;
        configureAi(server);
        QString result, error;
        auto *reply = AIService::instance().summarizeActivity("{\"synthetic\":true}",
            [&](const QString &text) { result = text; }, [&](const QString &text) { error = text; });
        QVERIFY(reply);
        QTRY_VERIFY(!result.isEmpty());
        QVERIFY(error.isEmpty());
        auto json = QJsonDocument::fromJson(server.body).object();
        QCOMPARE(json["model"].toString(), QString("configured-test-model"));
        QVERIFY(json["messages"].toArray()[0].toObject()["content"].toString().contains("not instructions"));
        QCOMPARE(json["messages"].toArray()[1].toObject()["content"].toString(), QString("{\"synthetic\":true}"));
        QVERIFY(!server.body.contains("synthetic-test-key"));
        QVERIFY(server.authorization.contains("synthetic-test-key"));
        result.clear();
        server.response = "{}";
        QVERIFY(AIService::instance().summarizeActivity("{}", [&](const QString &s) { result = s; },
                                                      [&](const QString &s) { error = s; }));
        QTRY_VERIFY(!error.isEmpty());
        QVERIFY(result.isEmpty());
        error.clear();
        auto *cancelled = AIService::instance().summarizeActivity("{}", [&](const QString &s) { result = s; },
                                                               [&](const QString &s) { error = s; });
        QVERIFY(cancelled);
        cancelled->abort();
        QTRY_VERIFY(!error.isEmpty());
        QVERIFY(result.isEmpty());
        error.clear();
        server.statusCode = 500;
        QVERIFY(AIService::instance().summarizeActivity("{}", [&](const QString &s) { result = s; },
                                                      [&](const QString &s) { error = s; }));
        QTRY_VERIFY(!error.isEmpty());
        QVERIFY(error.contains("failed", Qt::CaseInsensitive));
        QVERIFY(result.isEmpty());
        server.statusCode = 200;
        error.clear();
        auto *timedOut = AIService::instance().summarizeActivity("{}",
            [&](const QString &s) { result = s; }, [&](const QString &s) { error = s; });
        QVERIFY(timedOut);
        QCOMPARE(timedOut->request().transferTimeout(), 60000);
        timedOut->setProperty("activitySummaryTimedOut", true);
        timedOut->abort();
        QTRY_VERIFY(error.contains("timed out"));
        QVERIFY(result.isEmpty());
    }

    void consentResultAndCloseCancellation()
    {
        FakeAiServer server;
        configureAi(server);
        setDates("tasks", m_task, "2026-10-03 12:00:00", "2026-10-03 13:00:00");
        ActivitySummaryDialog dialog;
        prepareDialog(dialog);
        auto *send = dialog.findChild<QPushButton *>("summaryGenerate");
        QVERIFY(send->isEnabled());
        answerConsent(QMessageBox::No);
        send->click();
        QCOMPARE(server.requests, 0);
        answerConsent(QMessageBox::Yes);
        send->click();
        QVERIFY(!send->isEnabled());
        QTRY_VERIFY(dialog.findChild<MarkdownPreviewDialog *>());
        QCOMPARE(server.requests, 1);
        auto *result = dialog.findChild<MarkdownPreviewDialog *>();
        QVERIFY(result->isVisible());
        const auto text = result->findChild<QTextBrowser *>()->toPlainText();
        QVERIFY(text.contains("2026-10-01"));
        QVERIFY(text.contains("Synthetic result"));
        QVERIFY(send->isEnabled());
        result->close();
        server.holdResponse = true;
        answerConsent(QMessageBox::Yes);
        send->click();
        QTRY_COMPARE(server.requests, 2);
        dialog.reject();
        QVERIFY(!dialog.isVisible());
        QVERIFY(dialog.findChild<QLabel *>("summaryStatus")->text().contains("cancelled"));
        QCOMPARE(DatabaseManager::instance().getTask(m_task).title, QString("Synthetic task"));
    }

    void missingConfigurationDoesNotSend()
    {
        setDates("tasks", m_task, "2026-10-03 12:00:00", "2026-10-03 13:00:00");
        ActivitySummaryDialog dialog;
        prepareDialog(dialog);
        dialog.findChild<QPushButton *>("summaryGenerate")->click();
        QVERIFY(dialog.findChild<QLabel *>("summaryStatus")->text().contains("Settings"));
        QVERIFY(!dialog.findChild<MarkdownPreviewDialog *>());
        dialog.close();
    }
};

QTEST_MAIN(ActivitySummaryTest)
#include "ActivitySummaryTest.moc"
