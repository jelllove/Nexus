#include "DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <QDateTime>
#include <QVariant>

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager inst;
    return inst;
}

bool DatabaseManager::initialize(const QString &dbPath)
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");

    QString path = dbPath;
    if (path.isEmpty()) {
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dataDir);
        path = dataDir + "/nexus.db";
    }

    m_db.setDatabaseName(path);
    m_dbPath = path;

    if (!m_db.open()) {
        qCritical() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }

    // Enable WAL mode for better concurrent read/write performance
    QSqlQuery query(m_db);
    query.exec("PRAGMA journal_mode=WAL");
    query.exec("PRAGMA foreign_keys=ON");

    qInfo() << "Database opened at:" << path;

    if (!createTables() || !createFtsTables())
        return false;

    migrateDatabase();
    return true;
}

bool DatabaseManager::createTables()
{
    QSqlQuery query(m_db);

    // Products table
    if (!query.exec(
        "CREATE TABLE IF NOT EXISTS products ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  sort_order INTEGER DEFAULT 0,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")")) {
        qCritical() << "Failed to create products table:" << query.lastError().text();
        return false;
    }

    // Tasks table
    if (!query.exec(
        "CREATE TABLE IF NOT EXISTS tasks ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  product_id INTEGER NOT NULL,"
        "  title TEXT NOT NULL DEFAULT 'New Task',"
        "  content TEXT DEFAULT '',"
        "  priority INTEGER DEFAULT 2,"
        "  status TEXT DEFAULT 'active',"
        "  sort_order INTEGER DEFAULT 0,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  archived_at DATETIME,"
        "  FOREIGN KEY (product_id) REFERENCES products(id) ON DELETE CASCADE"
        ")")) {
        qCritical() << "Failed to create tasks table:" << query.lastError().text();
        return false;
    }

    // Content history for undo/redo
    if (!query.exec(
        "CREATE TABLE IF NOT EXISTS task_content_history ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  task_id INTEGER NOT NULL,"
        "  content TEXT NOT NULL,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  FOREIGN KEY (task_id) REFERENCES tasks(id) ON DELETE CASCADE"
        ")")) {
        qCritical() << "Failed to create task_content_history table:" << query.lastError().text();
        return false;
    }

    // Settings table
    if (!query.exec(
        "CREATE TABLE IF NOT EXISTS settings ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT"
        ")")) {
        qCritical() << "Failed to create settings table:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::createFtsTables()
{
    QSqlQuery query(m_db);

    // FTS5 virtual table for full-text search
    if (!query.exec(
        "CREATE VIRTUAL TABLE IF NOT EXISTS tasks_fts USING fts5("
        "  title, content, content=tasks, content_rowid=id"
        ")")) {
        qWarning() << "Failed to create FTS table (FTS5 may not be available):" << query.lastError().text();
        // Non-fatal: fall back to LIKE-based search
        return true;
    }

    // Triggers to keep FTS in sync
    query.exec(
        "CREATE TRIGGER IF NOT EXISTS tasks_ai AFTER INSERT ON tasks BEGIN "
        "  INSERT INTO tasks_fts(rowid, title, content) VALUES (new.id, new.title, new.content); "
        "END");
    query.exec(
        "CREATE TRIGGER IF NOT EXISTS tasks_ad AFTER DELETE ON tasks BEGIN "
        "  INSERT INTO tasks_fts(tasks_fts, rowid, title, content) VALUES ('delete', old.id, old.title, old.content); "
        "END");
    query.exec(
        "CREATE TRIGGER IF NOT EXISTS tasks_au AFTER UPDATE ON tasks BEGIN "
        "  INSERT INTO tasks_fts(tasks_fts, rowid, title, content) VALUES ('delete', old.id, old.title, old.content); "
        "  INSERT INTO tasks_fts(rowid, title, content) VALUES (new.id, new.title, new.content); "
        "END");

    return true;
}

bool DatabaseManager::migrateDatabase()
{
    QSqlQuery query(m_db);

    // Get current DB version (default 1 for existing DBs without version tracking)
    int dbVersion = getSetting("db_version", "1").toInt();

    // Migration v1 -> v2: add due_date column
    if (dbVersion < 2) {
        query.exec("PRAGMA table_info(tasks)");
        bool hasDueDate = false;
        while (query.next()) {
            if (query.value(1).toString() == "due_date") {
                hasDueDate = true;
                break;
            }
        }
        if (!hasDueDate) {
            if (!query.exec("ALTER TABLE tasks ADD COLUMN due_date DATETIME")) {
                qWarning() << "Failed to add due_date column:" << query.lastError().text();
                return false;
            }
            qInfo() << "Migration v2: added due_date column to tasks table";
        }
        setSetting("db_version", "2");
        dbVersion = 2;
    }

    // Migration v2 -> v3: add completed and completed_at columns
    if (dbVersion < 3) {
        query.exec("PRAGMA table_info(tasks)");
        bool hasCompleted = false;
        while (query.next()) {
            if (query.value(1).toString() == "completed") {
                hasCompleted = true;
                break;
            }
        }
        if (!hasCompleted) {
            if (!query.exec("ALTER TABLE tasks ADD COLUMN completed INTEGER DEFAULT 0")) {
                qWarning() << "Failed to add completed column:" << query.lastError().text();
                return false;
            }
            if (!query.exec("ALTER TABLE tasks ADD COLUMN completed_at DATETIME")) {
                qWarning() << "Failed to add completed_at column:" << query.lastError().text();
                return false;
            }
            qInfo() << "Migration v3: added completed, completed_at columns to tasks table";
        }
        setSetting("db_version", "3");
        dbVersion = 3;
    }

    // Migration v3 -> v4: add work_status column, migrate completed -> work_status
    if (dbVersion < 4) {
        query.exec("PRAGMA table_info(tasks)");
        bool hasWorkStatus = false;
        while (query.next()) {
            if (query.value(1).toString() == "work_status") {
                hasWorkStatus = true;
                break;
            }
        }
        if (!hasWorkStatus) {
            if (!query.exec("ALTER TABLE tasks ADD COLUMN work_status INTEGER DEFAULT 0")) {
                qWarning() << "Failed to add work_status column:" << query.lastError().text();
                return false;
            }
            // Migrate: completed=1 -> work_status=3 (Completed)
            query.exec("UPDATE tasks SET work_status = 3 WHERE completed = 1");
            qInfo() << "Migration v4: added work_status column, migrated completed tasks";
        }
        setSetting("db_version", "4");
        dbVersion = 4;
    }

    // Future migrations go here:
    // if (dbVersion < 6) { ... setSetting("db_version", "6"); dbVersion = 6; }

    // Migration v4 -> v5: add deleted_at column for soft-delete
    if (dbVersion < 5) {
        query.exec("PRAGMA table_info(tasks)");
        bool hasDeletedAt = false;
        while (query.next()) {
            if (query.value(1).toString() == "deleted_at") {
                hasDeletedAt = true;
                break;
            }
        }
        if (!hasDeletedAt) {
            if (!query.exec("ALTER TABLE tasks ADD COLUMN deleted_at DATETIME")) {
                qWarning() << "Failed to add deleted_at column:" << query.lastError().text();
                return false;
            }
            qInfo() << "Migration v5: added deleted_at column to tasks table";
        }
        setSetting("db_version", "5");
        dbVersion = 5;
    }

    // Migration v5 -> v6: create subtasks table
    if (dbVersion < 6) {
        if (!query.exec(
            "CREATE TABLE IF NOT EXISTS subtasks ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  task_id INTEGER NOT NULL,"
            "  title TEXT NOT NULL DEFAULT '',"
            "  completed INTEGER DEFAULT 0,"
            "  sort_order INTEGER DEFAULT 0,"
            "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
            "  FOREIGN KEY (task_id) REFERENCES tasks(id) ON DELETE CASCADE"
            ")")) {
            qWarning() << "Failed to create subtasks table:" << query.lastError().text();
            return false;
        }
        qInfo() << "Migration v6: created subtasks table";
        setSetting("db_version", "6");
        dbVersion = 6;
    }

    return true;
}

// --- Product CRUD ---

QList<Product> DatabaseManager::getAllProducts()
{
    QList<Product> products;
    QSqlQuery query(m_db);
    query.exec("SELECT id, name, sort_order, created_at, updated_at FROM products ORDER BY sort_order, id");

    while (query.next()) {
        Product p;
        p.id = query.value(0).toInt();
        p.name = query.value(1).toString();
        p.sortOrder = query.value(2).toInt();
        p.createdAt = query.value(3).toDateTime();
        p.updatedAt = query.value(4).toDateTime();
        products.append(p);
    }
    return products;
}

Product DatabaseManager::getProduct(int id)
{
    Product p;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, name, sort_order, created_at, updated_at FROM products WHERE id = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        p.id = query.value(0).toInt();
        p.name = query.value(1).toString();
        p.sortOrder = query.value(2).toInt();
        p.createdAt = query.value(3).toDateTime();
        p.updatedAt = query.value(4).toDateTime();
    }
    return p;
}

int DatabaseManager::addProduct(const QString &name)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO products (name) VALUES (?)");
    query.addBindValue(name);
    if (query.exec()) {
        int id = query.lastInsertId().toInt();
        emit productAdded(id);
        return id;
    }
    qWarning() << "Failed to add product:" << query.lastError().text();
    return -1;
}

bool DatabaseManager::updateProduct(int id, const QString &name)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE products SET name = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    query.addBindValue(name);
    query.addBindValue(id);
    if (query.exec()) {
        emit productUpdated(id);
        return true;
    }
    return false;
}

bool DatabaseManager::deleteProduct(int id)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM products WHERE id = ?");
    query.addBindValue(id);
    if (query.exec()) {
        emit productDeleted(id);
        return true;
    }
    return false;
}

bool DatabaseManager::reorderProducts(const QList<int> &productIds)
{
    QSqlQuery query(m_db);
    m_db.transaction();
    for (int i = 0; i < productIds.size(); ++i) {
        query.prepare("UPDATE products SET sort_order = ? WHERE id = ?");
        query.addBindValue(i);
        query.addBindValue(productIds[i]);
        if (!query.exec()) {
            m_db.rollback();
            return false;
        }
    }
    return m_db.commit();
}

// --- Task CRUD ---

QList<Task> DatabaseManager::getTasksForProduct(int productId, TaskStatus status)
{
    QList<Task> tasks;
    QSqlQuery query(m_db);
    QString statusStr = (status == TaskStatus::Active) ? "active" : "archived";
    query.prepare(
        "SELECT id, product_id, title, content, priority, status, sort_order, "
        "created_at, updated_at, archived_at, due_date, work_status "
        "FROM tasks WHERE product_id = ? AND status = ? "
        "ORDER BY priority ASC, CASE WHEN due_date IS NOT NULL THEN 0 ELSE 1 END, due_date ASC, sort_order ASC, id DESC");
    query.addBindValue(productId);
    query.addBindValue(statusStr);
    query.exec();

    while (query.next()) {
        Task t;
        t.id = query.value(0).toInt();
        t.productId = query.value(1).toInt();
        t.title = query.value(2).toString();
        t.content = query.value(3).toString();
        t.priority = static_cast<TaskPriority>(query.value(4).toInt());
        t.status = (query.value(5).toString() == "active") ? TaskStatus::Active : TaskStatus::Archived;
        t.sortOrder = query.value(6).toInt();
        t.createdAt = query.value(7).toDateTime();
        t.updatedAt = query.value(8).toDateTime();
        t.archivedAt = query.value(9).toDateTime();
        t.dueDate = query.value(10).toDateTime();
        t.workStatus = static_cast<TaskWorkStatus>(query.value(11).toInt());
        tasks.append(t);
    }
    return tasks;
}

Task DatabaseManager::getTask(int id)
{
    Task t;
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT id, product_id, title, content, priority, status, sort_order, "
        "created_at, updated_at, archived_at, due_date, work_status "
        "FROM tasks WHERE id = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        t.id = query.value(0).toInt();
        t.productId = query.value(1).toInt();
        t.title = query.value(2).toString();
        t.content = query.value(3).toString();
        t.priority = static_cast<TaskPriority>(query.value(4).toInt());
        t.status = (query.value(5).toString() == "active") ? TaskStatus::Active : TaskStatus::Archived;
        t.sortOrder = query.value(6).toInt();
        t.createdAt = query.value(7).toDateTime();
        t.updatedAt = query.value(8).toDateTime();
        t.archivedAt = query.value(9).toDateTime();
        t.dueDate = query.value(10).toDateTime();
        t.workStatus = static_cast<TaskWorkStatus>(query.value(11).toInt());
    }
    return t;
}

int DatabaseManager::addTask(int productId, const QString &title, TaskPriority priority, const QDateTime &dueDate)
{
    QSqlQuery query(m_db);
    if (dueDate.isValid()) {
        query.prepare("INSERT INTO tasks (product_id, title, priority, due_date) VALUES (?, ?, ?, ?)");
        query.addBindValue(productId);
        query.addBindValue(title);
        query.addBindValue(static_cast<int>(priority));
        query.addBindValue(dueDate.toString(Qt::ISODate));
    } else {
        query.prepare("INSERT INTO tasks (product_id, title, priority) VALUES (?, ?, ?)");
        query.addBindValue(productId);
        query.addBindValue(title);
        query.addBindValue(static_cast<int>(priority));
    }
    if (query.exec()) {
        int id = query.lastInsertId().toInt();
        emit taskAdded(id);
        return id;
    }
    qWarning() << "Failed to add task:" << query.lastError().text();
    return -1;
}

bool DatabaseManager::updateTask(const Task &task)
{
    QSqlQuery query(m_db);
    query.prepare(
        "UPDATE tasks SET title = ?, content = ?, priority = ?, status = ?, "
        "sort_order = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    query.addBindValue(task.title);
    query.addBindValue(task.content);
    query.addBindValue(static_cast<int>(task.priority));
    query.addBindValue(task.status == TaskStatus::Active ? "active" : "archived");
    query.addBindValue(task.sortOrder);
    query.addBindValue(task.id);
    if (query.exec()) {
        emit taskUpdated(task.id);
        return true;
    }
    return false;
}

bool DatabaseManager::updateTaskTitle(int taskId, const QString &title)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE tasks SET title = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    query.addBindValue(title);
    query.addBindValue(taskId);
    if (query.exec()) {
        emit taskUpdated(taskId);
        return true;
    }
    return false;
}

bool DatabaseManager::updateTaskContent(int taskId, const QString &content)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE tasks SET content = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    query.addBindValue(content);
    query.addBindValue(taskId);
    if (query.exec()) {
        emit taskUpdated(taskId);
        return true;
    }
    return false;
}

bool DatabaseManager::updateTaskPriority(int taskId, TaskPriority priority)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE tasks SET priority = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    query.addBindValue(static_cast<int>(priority));
    query.addBindValue(taskId);
    if (query.exec()) {
        emit taskUpdated(taskId);
        return true;
    }
    return false;
}

bool DatabaseManager::updateTaskDueDate(int taskId, const QDateTime &dueDate)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE tasks SET due_date = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    if (dueDate.isValid())
        query.addBindValue(dueDate.toString(Qt::ISODate));
    else
        query.addBindValue(QVariant());
    query.addBindValue(taskId);
    if (query.exec()) {
        emit taskUpdated(taskId);
        return true;
    }
    return false;
}

bool DatabaseManager::archiveTask(int taskId)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE tasks SET status = 'archived', archived_at = CURRENT_TIMESTAMP, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    query.addBindValue(taskId);
    if (query.exec()) {
        emit taskArchived(taskId);
        return true;
    }
    return false;
}

bool DatabaseManager::updateTaskWorkStatus(int taskId, TaskWorkStatus workStatus)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE tasks SET work_status = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    query.addBindValue(static_cast<int>(workStatus));
    query.addBindValue(taskId);
    if (query.exec()) {
        emit taskUpdated(taskId);
        return true;
    }
    return false;
}

bool DatabaseManager::reactivateTask(int taskId)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE tasks SET status = 'active', archived_at = NULL, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    query.addBindValue(taskId);
    if (query.exec()) {
        emit taskReactivated(taskId);
        return true;
    }
    return false;
}

bool DatabaseManager::deleteTask(int taskId)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE tasks SET status = 'deleted', deleted_at = CURRENT_TIMESTAMP WHERE id = ?");
    query.addBindValue(taskId);
    if (query.exec()) {
        emit taskDeleted(taskId);
        return true;
    }
    return false;
}

bool DatabaseManager::restoreTask(int taskId)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE tasks SET status = 'active', deleted_at = NULL WHERE id = ?");
    query.addBindValue(taskId);
    return query.exec();
}

bool DatabaseManager::permanentlyDeleteTask(int taskId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM tasks WHERE id = ?");
    query.addBindValue(taskId);
    return query.exec();
}

void DatabaseManager::purgeOldDeletedTasks(int maxAgeDays)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM tasks WHERE status = 'deleted' "
                  "AND deleted_at < datetime('now', ? || ' days')");
    query.addBindValue(QString("-%1").arg(maxAgeDays));
    query.exec();
}

QList<Task> DatabaseManager::getDeletedTasks()
{
    QList<Task> tasks;
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT id, product_id, title, content, priority, status, sort_order, "
        "created_at, updated_at, archived_at, due_date, work_status, deleted_at "
        "FROM tasks WHERE status = 'deleted' "
        "ORDER BY deleted_at DESC");
    query.exec();

    while (query.next()) {
        Task t;
        t.id = query.value(0).toInt();
        t.productId = query.value(1).toInt();
        t.title = query.value(2).toString();
        t.content = query.value(3).toString();
        t.priority = static_cast<TaskPriority>(query.value(4).toInt());
        t.status = TaskStatus::Deleted;
        t.sortOrder = query.value(6).toInt();
        t.createdAt = query.value(7).toDateTime();
        t.updatedAt = query.value(8).toDateTime();
        t.dueDate = query.value(10).toDateTime();
        t.workStatus = static_cast<TaskWorkStatus>(query.value(11).toInt());
        tasks.append(t);
    }
    return tasks;
}

bool DatabaseManager::reorderTasks(const QList<int> &taskIds)
{
    QSqlQuery query(m_db);
    m_db.transaction();
    for (int i = 0; i < taskIds.size(); ++i) {
        query.prepare("UPDATE tasks SET sort_order = ? WHERE id = ?");
        query.addBindValue(i);
        query.addBindValue(taskIds[i]);
        if (!query.exec()) {
            m_db.rollback();
            return false;
        }
    }
    return m_db.commit();
}

// --- Sub-tasks ---

QList<SubTask> DatabaseManager::getSubtasks(int taskId)
{
    QList<SubTask> subtasks;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, task_id, title, completed, sort_order FROM subtasks "
                  "WHERE task_id = ? ORDER BY sort_order ASC, id ASC");
    query.addBindValue(taskId);
    query.exec();
    while (query.next()) {
        SubTask st;
        st.id = query.value(0).toInt();
        st.taskId = query.value(1).toInt();
        st.title = query.value(2).toString();
        st.completed = query.value(3).toBool();
        st.sortOrder = query.value(4).toInt();
        subtasks.append(st);
    }
    return subtasks;
}

int DatabaseManager::getSubtaskCount(int taskId)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM subtasks WHERE task_id = ?");
    query.addBindValue(taskId);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int DatabaseManager::addSubtask(int taskId, const QString &title)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO subtasks (task_id, title, sort_order) "
                  "VALUES (?, ?, (SELECT COALESCE(MAX(sort_order), 0) + 1 FROM subtasks WHERE task_id = ?))");
    query.addBindValue(taskId);
    query.addBindValue(title);
    query.addBindValue(taskId);
    if (query.exec()) {
        return query.lastInsertId().toInt();
    }
    return -1;
}

bool DatabaseManager::toggleSubtask(int subtaskId, bool completed)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE subtasks SET completed = ? WHERE id = ?");
    query.addBindValue(completed ? 1 : 0);
    query.addBindValue(subtaskId);
    return query.exec();
}

bool DatabaseManager::deleteSubtask(int subtaskId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM subtasks WHERE id = ?");
    query.addBindValue(subtaskId);
    return query.exec();
}

bool DatabaseManager::renameSubtask(int subtaskId, const QString &title)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE subtasks SET title = ? WHERE id = ?");
    query.addBindValue(title);
    query.addBindValue(subtaskId);
    return query.exec();
}

// --- Content History (Undo/Redo) ---

void DatabaseManager::saveContentSnapshot(int taskId, const QString &content)
{
    // Clean up old snapshots first
    cleanupOldHistory(taskId);

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO task_content_history (task_id, content) VALUES (?, ?)");
    query.addBindValue(taskId);
    query.addBindValue(content);
    query.exec();
}

QList<QString> DatabaseManager::getContentHistory(int taskId)
{
    QList<QString> history;
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT content FROM task_content_history "
        "WHERE task_id = ? AND created_at >= datetime('now', '-60 minutes') "
        "ORDER BY id ASC");
    query.addBindValue(taskId);
    query.exec();

    while (query.next()) {
        history.append(query.value(0).toString());
    }
    return history;
}

void DatabaseManager::cleanupOldHistory(int taskId, int maxAgeMinutes)
{
    QSqlQuery query(m_db);
    query.prepare(
        "DELETE FROM task_content_history "
        "WHERE task_id = ? AND created_at < datetime('now', '-' || ? || ' minutes')");
    query.addBindValue(taskId);
    query.addBindValue(maxAgeMinutes);
    query.exec();
}

// --- Search ---

QList<Task> DatabaseManager::searchTasks(const QString &query, int productId)
{
    QList<Task> tasks;
    QSqlQuery q(m_db);

    // Try FTS5 first
    QString sql;
    if (productId > 0) {
        sql = "SELECT t.id, t.product_id, t.title, t.content, t.priority, t.status, "
              "t.sort_order, t.created_at, t.updated_at, t.archived_at, t.due_date, "
              "t.work_status "
              "FROM tasks t INNER JOIN tasks_fts f ON t.id = f.rowid "
              "WHERE tasks_fts MATCH ? AND t.product_id = ? "
              "ORDER BY rank";
        q.prepare(sql);
        q.addBindValue(query);
        q.addBindValue(productId);
    } else {
        sql = "SELECT t.id, t.product_id, t.title, t.content, t.priority, t.status, "
              "t.sort_order, t.created_at, t.updated_at, t.archived_at, t.due_date, "
              "t.work_status "
              "FROM tasks t INNER JOIN tasks_fts f ON t.id = f.rowid "
              "WHERE tasks_fts MATCH ? "
              "ORDER BY rank";
        q.prepare(sql);
        q.addBindValue(query);
    }

    if (!q.exec()) {
        // Fallback to LIKE-based search
        qWarning() << "FTS search failed, using LIKE fallback:" << q.lastError().text();
        QString likeQuery = "%" + query + "%";
        if (productId > 0) {
            q.prepare(
                "SELECT id, product_id, title, content, priority, status, sort_order, "
                "created_at, updated_at, archived_at, due_date, work_status FROM tasks "
                "WHERE (title LIKE ? OR content LIKE ?) AND product_id = ?");
            q.addBindValue(likeQuery);
            q.addBindValue(likeQuery);
            q.addBindValue(productId);
        } else {
            q.prepare(
                "SELECT id, product_id, title, content, priority, status, sort_order, "
                "created_at, updated_at, archived_at, due_date, work_status FROM tasks "
                "WHERE title LIKE ? OR content LIKE ?");
            q.addBindValue(likeQuery);
            q.addBindValue(likeQuery);
        }
        q.exec();
    }

    while (q.next()) {
        Task t;
        t.id = q.value(0).toInt();
        t.productId = q.value(1).toInt();
        t.title = q.value(2).toString();
        t.content = q.value(3).toString();
        t.priority = static_cast<TaskPriority>(q.value(4).toInt());
        t.status = (q.value(5).toString() == "active") ? TaskStatus::Active : TaskStatus::Archived;
        t.sortOrder = q.value(6).toInt();
        t.createdAt = q.value(7).toDateTime();
        t.updatedAt = q.value(8).toDateTime();
        t.archivedAt = q.value(9).toDateTime();
        t.dueDate = q.value(10).toDateTime();
        t.workStatus = static_cast<TaskWorkStatus>(q.value(11).toInt());
        tasks.append(t);
    }
    return tasks;
}

// --- Database Path & Backup ---

QString DatabaseManager::currentDbPath() const
{
    return m_dbPath;
}

bool DatabaseManager::moveDatabase(const QString &newPath)
{
    if (newPath == m_dbPath)
        return true;

    // Close current connection
    m_db.close();

    // Copy the file to new location
    QDir().mkpath(QFileInfo(newPath).absolutePath());
    if (QFile::exists(newPath)) {
        QFile::remove(newPath);
    }

    bool copied = QFile::copy(m_dbPath, newPath);
    if (!copied) {
        qWarning() << "Failed to copy database to" << newPath;
        // Reopen at old path
        m_db.setDatabaseName(m_dbPath);
        m_db.open();
        return false;
    }

    // Also copy WAL and SHM files if they exist
    for (const QString &suffix : {"-wal", "-shm"}) {
        QString src = m_dbPath + suffix;
        QString dst = newPath + suffix;
        if (QFile::exists(src)) {
            QFile::remove(dst);
            QFile::copy(src, dst);
        }
    }

    // Open at new path
    m_db.setDatabaseName(newPath);
    if (!m_db.open()) {
        qWarning() << "Failed to open database at new path:" << newPath;
        m_db.setDatabaseName(m_dbPath);
        m_db.open();
        return false;
    }

    // Remove old files
    QString oldPath = m_dbPath;
    m_dbPath = newPath;
    QFile::remove(oldPath);
    QFile::remove(oldPath + "-wal");
    QFile::remove(oldPath + "-shm");

    qInfo() << "Database moved to:" << newPath;
    return true;
}

bool DatabaseManager::backupDatabase()
{
    QString today = QDate::currentDate().toString("yyyyMMdd");
    QString backupDir = QFileInfo(m_dbPath).absolutePath() + "/backups";
    QDir().mkpath(backupDir);

    QString backupPath = backupDir + "/nexus_" + today + ".db";

    // Skip if today's backup already exists
    if (QFile::exists(backupPath))
        return true;

    // Use SQLite VACUUM INTO for a consistent backup
    QSqlQuery query(m_db);
    if (query.exec(QString("VACUUM INTO '%1'").arg(backupPath))) {
        qInfo() << "Database backed up to:" << backupPath;
        cleanupOldBackups();
        return true;
    }

    // Fallback: simple file copy
    bool ok = QFile::copy(m_dbPath, backupPath);
    if (ok) {
        qInfo() << "Database backed up (copy) to:" << backupPath;
        cleanupOldBackups();
    } else {
        qWarning() << "Database backup failed";
    }
    return ok;
}

void DatabaseManager::cleanupOldBackups(int maxBackups)
{
    QString backupDir = QFileInfo(m_dbPath).absolutePath() + "/backups";
    QDir dir(backupDir);
    QStringList backups = dir.entryList({"nexus_*.db"}, QDir::Files, QDir::Name);

    while (backups.size() > maxBackups) {
        QString oldest = backups.takeFirst();
        QFile::remove(backupDir + "/" + oldest);
        qInfo() << "Removed old backup:" << oldest;
    }
}

// --- Settings ---

QString DatabaseManager::getSetting(const QString &key, const QString &defaultValue)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT value FROM settings WHERE key = ?");
    query.addBindValue(key);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return defaultValue;
}

void DatabaseManager::setSetting(const QString &key, const QString &value)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)");
    query.addBindValue(key);
    query.addBindValue(value);
    query.exec();
}

void DatabaseManager::updateFtsIndex(int taskId, const QString &title, const QString &content)
{
    Q_UNUSED(taskId);
    Q_UNUSED(title);
    Q_UNUSED(content);
    // FTS is kept in sync via triggers
}

void DatabaseManager::removeFtsEntry(int taskId)
{
    Q_UNUSED(taskId);
    // FTS is kept in sync via triggers
}
