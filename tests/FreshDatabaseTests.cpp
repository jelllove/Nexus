#include <QtTest>
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

        auto &database = DatabaseManager::instance();
        QVERIFY(database.initialize(directory.filePath("fresh.db")));
        QCOMPARE(database.getSetting("db_version"), QString("7"));

        const int productId = database.addProduct("Product");
        const int taskId = database.addTask(productId, "Parent");
        const int subtaskId = database.addSubtask(taskId, "Child");
        QVERIFY(subtaskId > 0);
        QVERIFY(database.updateSubtaskContent(subtaskId, "<p>fresh note</p>"));
        QCOMPARE(database.getSubtask(subtaskId).content, QString("<p>fresh note</p>"));
    }
};

QTEST_GUILESS_MAIN(FreshDatabaseTests)
#include "FreshDatabaseTests.moc"
