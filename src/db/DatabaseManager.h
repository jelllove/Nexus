#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QList>
#include "models/Product.h"
#include "models/Task.h"

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    static DatabaseManager& instance();

    bool initialize(const QString &dbPath = QString());

    // Database path and backup
    QString currentDbPath() const;
    bool moveDatabase(const QString &newPath);
    bool backupDatabase();
    void cleanupOldBackups(int maxBackups = 10);

    // Product CRUD
    QList<Product> getAllProducts();
    Product getProduct(int id);
    int addProduct(const QString &name);
    bool updateProduct(int id, const QString &name);
    bool deleteProduct(int id);
    bool reorderProducts(const QList<int> &productIds);

    // Task CRUD
    QList<Task> getTasksForProduct(int productId, TaskStatus status = TaskStatus::Active);
    Task getTask(int id);
    int addTask(int productId, const QString &title, TaskPriority priority = TaskPriority::Medium, const QDateTime &dueDate = QDateTime());
    bool updateTask(const Task &task);
    bool updateTaskTitle(int taskId, const QString &title);
    bool updateTaskContent(int taskId, const QString &content);
    bool updateTaskPriority(int taskId, TaskPriority priority);
    bool updateTaskDueDate(int taskId, const QDateTime &dueDate);
    bool archiveTask(int taskId);
    bool reactivateTask(int taskId);
    bool updateTaskWorkStatus(int taskId, TaskWorkStatus workStatus);
    bool deleteTask(int taskId);
    bool reorderTasks(const QList<int> &taskIds);

    // Content history (undo/redo)
    void saveContentSnapshot(int taskId, const QString &content);
    QList<QString> getContentHistory(int taskId);
    void cleanupOldHistory(int taskId, int maxAgeMinutes = 60);

    // Search
    QList<Task> searchTasks(const QString &query, int productId = -1);

    // Settings
    QString getSetting(const QString &key, const QString &defaultValue = QString());
    void setSetting(const QString &key, const QString &value);

signals:
    void productAdded(int id);
    void productUpdated(int id);
    void productDeleted(int id);
    void taskAdded(int id);
    void taskUpdated(int id);
    void taskDeleted(int id);
    void taskArchived(int id);
    void taskReactivated(int id);

private:
    DatabaseManager() = default;
    ~DatabaseManager() = default;
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool createTables();
    bool createFtsTables();
    bool migrateDatabase();
    void updateFtsIndex(int taskId, const QString &title, const QString &content);
    void removeFtsEntry(int taskId);

    QSqlDatabase m_db;
    QString m_dbPath;
};
