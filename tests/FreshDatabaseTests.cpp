#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "db/DatabaseManager.h"

class FreshDatabaseTests : public QObject
{
    Q_OBJECT

private slots:
    void freshDatabaseUsesVersionSevenSchema()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString tempPath = directory.path();
        const QString databasePath = directory.filePath("fresh.db");

        auto &database = DatabaseManager::instance();
        QVERIFY(database.initialize(databasePath));
        QCOMPARE(database.getSetting("db_version"), QString("7"));

        const int productId = database.addProduct("Product");
        const int taskId = database.addTask(productId, "Parent");
        const int subtaskId = database.addSubtask(taskId, "Child");
        QVERIFY(subtaskId > 0);
        QVERIFY(database.updateSubtaskContent(subtaskId, "<p>fresh note</p>"));
        QCOMPARE(database.getSubtask(subtaskId).content, QString("<p>fresh note</p>"));

        QSqlDatabase::database().close();
        QVERIFY2(QFile::remove(databasePath),
                 qPrintable(QString("Expected test database to be removable during teardown: %1")
                                .arg(databasePath)));
        QVERIFY(directory.remove());
        QVERIFY2(!QDir(tempPath).exists(),
                 qPrintable(QString("Expected temp directory to be removed during teardown: %1")
                                .arg(tempPath)));
    }
};

QTEST_GUILESS_MAIN(FreshDatabaseTests)
#include "FreshDatabaseTests.moc"
