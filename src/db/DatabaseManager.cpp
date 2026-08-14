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

namespace {
bool tableHasColumn(QSqlDatabase &database, const QString &tableName, const QString &columnName)
{
    QSqlQuery query(database);
    if (!query.exec(QString("PRAGMA table_info(%1)").arg(tableName))) {
        qCritical() << "Failed to inspect table" << tableName << ":" << query.lastError().text();
        return false;
    }

    while (query.next()) {
        if (query.value(1).toString() == columnName) {
            return true;
        }
    }

    return false;
}

bool tableExists(QSqlDatabase &database, const QString &tableName)
{
    QSqlQuery query(database);
    query.prepare("SELECT 1 FROM sqlite_master WHERE type IN ('table', 'view') AND name = ?");
    query.addBindValue(tableName);
    if (!query.exec()) {
        qCritical() << "Failed to inspect sqlite_master for" << tableName << ":" << query.lastError().text();
        return false;
    }

    return query.next();
}

QStringList searchTokens(const QString &rawQuery)
{
    const QString simplified = rawQuery.simplified();
    if (simplified.isEmpty()) {
        return {};
    }

    return simplified.split(' ', Qt::SkipEmptyParts);
}

QString buildFtsMatchQuery(const QStringList &tokens)
{
    if (tokens.isEmpty()) {
        return QString();
    }

    QStringList escapedTokens;
    escapedTokens.reserve(tokens.size());
    for (QString token : tokens) {
        token.replace(QStringLiteral("\""), QStringLiteral("\"\""));
        escapedTokens.append(QStringLiteral("\"%1\"").arg(token));
    }

    return escapedTokens.join(QStringLiteral(" AND "));
}

QString buildLikeSearchClause(const QString &titleColumn, const QString &contentColumn, int tokenCount)
{
    QStringList clauses;
    clauses.reserve(tokenCount);
    for (int i = 0; i < tokenCount; ++i) {
        clauses.append(QStringLiteral("(%1 LIKE ? OR %2 LIKE ?)")
                           .arg(titleColumn, contentColumn));
    }

    return clauses.join(QStringLiteral(" AND "));
}

void bindLikeSearchTokens(QSqlQuery &query, const QStringList &tokens)
{
    for (const QString &token : tokens) {
        const QString pattern = QStringLiteral("%") + token + QStringLiteral("%");
        query.addBindValue(pattern);
        query.addBindValue(pattern);
    }
}

bool isMissingFtsTableError(const QSqlError &error)
{
    const QString errorText = error.text();
    return errorText.contains(QStringLiteral("no such table"), Qt::CaseInsensitive)
        && errorText.contains(QStringLiteral("_fts"), Qt::CaseInsensitive);
}

Task taskFromSearchQuery(const QSqlQuery &query, int startColumn = 0)
{
    Task task;
    task.id = query.value(startColumn + 0).toInt();
    task.productId = query.value(startColumn + 1).toInt();
    task.title = query.value(startColumn + 2).toString();
    task.content = query.value(startColumn + 3).toString();
    task.priority = static_cast<TaskPriority>(query.value(startColumn + 4).toInt());

    const QString status = query.value(startColumn + 5).toString();
    if (status == "deleted") {
        task.status = TaskStatus::Deleted;
    } else if (status == "archived") {
        task.status = TaskStatus::Archived;
    } else {
        task.status = TaskStatus::Active;
    }

    task.sortOrder = query.value(startColumn + 6).toInt();
    task.createdAt = query.value(startColumn + 7).toDateTime();
    task.updatedAt = query.value(startColumn + 8).toDateTime();
    task.archivedAt = query.value(startColumn + 9).toDateTime();
    task.dueDate = query.value(startColumn + 10).toDateTime();
    task.workStatus = static_cast<TaskWorkStatus>(query.value(startColumn + 11).toInt());
    return task;
}

SubTask subtaskFromSearchQuery(const QSqlQuery &query, int startColumn)
{
    SubTask subtask;
    subtask.id = query.value(startColumn + 0).toInt();
    subtask.taskId = query.value(startColumn + 1).toInt();
    subtask.title = query.value(startColumn + 2).toString();
    subtask.content = query.value(startColumn + 3).toString();
    subtask.completed = query.value(startColumn + 4).toBool();
    subtask.sortOrder = query.value(startColumn + 5).toInt();
    subtask.createdAt = query.value(startColumn + 6).toDateTime();
    subtask.updatedAt = query.value(startColumn + 7).toDateTime();
    return subtask;
}
}

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

    if (!createTables()) {
        qCritical() << "Failed to create database tables.";
        return false;
    }

    if (!migrateDatabase()) {
        qCritical() << "Failed to migrate database schema.";
        return false;
    }

    if (!createFtsTables()) {
        qCritical() << "Failed to create database search tables.";
        return false;
    }

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

    if (!query.exec(
        "CREATE TABLE IF NOT EXISTS subtasks ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  task_id INTEGER NOT NULL,"
        "  title TEXT NOT NULL DEFAULT '',"
        "  content TEXT NOT NULL DEFAULT '',"
        "  completed INTEGER DEFAULT 0,"
        "  sort_order INTEGER DEFAULT 0,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  FOREIGN KEY (task_id) REFERENCES tasks(id) ON DELETE CASCADE"
        ")")) {
        qCritical() << "Failed to create subtasks table:" << query.lastError().text();
        return false;
    }

    if (!query.exec(
        "CREATE TABLE IF NOT EXISTS subtask_content_history ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  subtask_id INTEGER NOT NULL,"
        "  content TEXT NOT NULL,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  FOREIGN KEY (subtask_id) REFERENCES subtasks(id) ON DELETE CASCADE"
        ")")) {
        qCritical() << "Failed to create subtask_content_history table:" << query.lastError().text();
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
    const bool hadTasksFts = tableExists(m_db, "tasks_fts");
    const bool hadSubtasksFts = tableExists(m_db, "subtasks_fts");

    if (!query.exec(
        "CREATE VIRTUAL TABLE IF NOT EXISTS tasks_fts USING fts5("
        "  title, content, content=tasks, content_rowid=id"
        ")")) {
        qWarning() << "Failed to create FTS table (FTS5 may not be available):" << query.lastError().text();
        return true;
    }

    if (!query.exec(
        "CREATE VIRTUAL TABLE IF NOT EXISTS subtasks_fts USING fts5("
        "  title, content, content=subtasks, content_rowid=id"
        ")")) {
        qWarning() << "Failed to create subtask FTS table (FTS5 may not be available):" << query.lastError().text();
        return true;
    }

    if (!query.exec(
        "CREATE TRIGGER IF NOT EXISTS tasks_ai AFTER INSERT ON tasks BEGIN "
        "  INSERT INTO tasks_fts(rowid, title, content) VALUES (new.id, new.title, new.content); "
        "END")) {
        qCritical() << "Failed to create tasks_ai trigger:" << query.lastError().text();
        return false;
    }
    if (!query.exec(
        "CREATE TRIGGER IF NOT EXISTS tasks_ad AFTER DELETE ON tasks BEGIN "
        "  INSERT INTO tasks_fts(tasks_fts, rowid, title, content) VALUES ('delete', old.id, old.title, old.content); "
        "END")) {
        qCritical() << "Failed to create tasks_ad trigger:" << query.lastError().text();
        return false;
    }
    if (!query.exec(
        "CREATE TRIGGER IF NOT EXISTS tasks_au AFTER UPDATE ON tasks BEGIN "
        "  INSERT INTO tasks_fts(tasks_fts, rowid, title, content) VALUES ('delete', old.id, old.title, old.content); "
        "  INSERT INTO tasks_fts(rowid, title, content) VALUES (new.id, new.title, new.content); "
        "END")) {
        qCritical() << "Failed to create tasks_au trigger:" << query.lastError().text();
        return false;
    }
    if (!query.exec(
        "CREATE TRIGGER IF NOT EXISTS subtasks_ai AFTER INSERT ON subtasks BEGIN "
        "  INSERT INTO subtasks_fts(rowid, title, content) VALUES (new.id, new.title, new.content); "
        "END")) {
        qCritical() << "Failed to create subtasks_ai trigger:" << query.lastError().text();
        return false;
    }
    if (!query.exec(
        "CREATE TRIGGER IF NOT EXISTS subtasks_ad AFTER DELETE ON subtasks BEGIN "
        "  INSERT INTO subtasks_fts(subtasks_fts, rowid, title, content) VALUES ('delete', old.id, old.title, old.content); "
        "END")) {
        qCritical() << "Failed to create subtasks_ad trigger:" << query.lastError().text();
        return false;
    }
    if (!query.exec(
        "CREATE TRIGGER IF NOT EXISTS subtasks_au AFTER UPDATE ON subtasks BEGIN "
        "  INSERT INTO subtasks_fts(subtasks_fts, rowid, title, content) VALUES ('delete', old.id, old.title, old.content); "
        "  INSERT INTO subtasks_fts(rowid, title, content) VALUES (new.id, new.title, new.content); "
        "END")) {
        qCritical() << "Failed to create subtasks_au trigger:" << query.lastError().text();
        return false;
    }

    if (!hadTasksFts && !query.exec("INSERT INTO tasks_fts(tasks_fts) VALUES ('rebuild')")) {
        qCritical() << "Failed to rebuild tasks FTS index:" << query.lastError().text();
        return false;
    }

    if (!hadSubtasksFts && !query.exec("INSERT INTO subtasks_fts(subtasks_fts) VALUES ('rebuild')")) {
        qCritical() << "Failed to rebuild subtasks FTS index:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::recoverFtsTablesIfNeeded(const QSqlError &error)
{
    const bool tasksFtsMissing = !tableExists(m_db, "tasks_fts");
    const bool subtasksFtsMissing = !tableExists(m_db, "subtasks_fts");
    if (!isMissingFtsTableError(error) && !tasksFtsMissing && !subtasksFtsMissing) {
        return false;
    }

    qWarning() << "Recovering missing FTS table:" << error.text();
    return createFtsTables();
}

bool DatabaseManager::executeWithFtsRecovery(
    const std::function<void(QSqlQuery &)> &prepareAndBind)
{
    QSqlQuery query(m_db);
    prepareAndBind(query);
    if (query.exec()) {
        return true;
    }

    if (!recoverFtsTablesIfNeeded(query.lastError())) {
        return false;
    }

    QSqlQuery retry(m_db);
    prepareAndBind(retry);
    if (retry.exec()) {
        return true;
    }

    qWarning() << "Query failed after FTS recovery:" << retry.lastError().text();
    return false;
}

int DatabaseManager::executeInsertWithFtsRecovery(
    const std::function<void(QSqlQuery &)> &prepareAndBind)
{
    QSqlQuery query(m_db);
    prepareAndBind(query);
    if (query.exec()) {
        return query.lastInsertId().toInt();
    }

    if (!recoverFtsTablesIfNeeded(query.lastError())) {
        return -1;
    }

    QSqlQuery retry(m_db);
    prepareAndBind(retry);
    if (retry.exec()) {
        return retry.lastInsertId().toInt();
    }

    qWarning() << "Insert failed after FTS recovery:" << retry.lastError().text();
    return -1;
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

    if (dbVersion < 7) {
        const bool hasSubtaskContent = tableHasColumn(m_db, "subtasks", "content");
        const bool hasSubtaskUpdatedAt = tableHasColumn(m_db, "subtasks", "updated_at");

        if (!hasSubtaskContent || !hasSubtaskUpdatedAt) {
            if (!m_db.transaction()) {
                qCritical() << "Failed to start v7 migration transaction:" << m_db.lastError().text();
                return false;
            }

            auto rollbackWithError = [this, &query](const QString &message) {
                qCritical() << message << query.lastError().text();
                m_db.rollback();
                return false;
            };

            if (!query.exec(
                "CREATE TABLE subtasks_v7 ("
                "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "  task_id INTEGER NOT NULL,"
                "  title TEXT NOT NULL DEFAULT '',"
                "  content TEXT NOT NULL DEFAULT '',"
                "  completed INTEGER DEFAULT 0,"
                "  sort_order INTEGER DEFAULT 0,"
                "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
                "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
                "  FOREIGN KEY (task_id) REFERENCES tasks(id) ON DELETE CASCADE"
                ")")) {
                return rollbackWithError("Failed to create subtasks_v7 table:");
            }

            if (!query.exec(
                "INSERT INTO subtasks_v7 "
                "    (id, task_id, title, content, completed, sort_order, created_at, updated_at) "
                "SELECT id, task_id, title, '', completed, sort_order, created_at, created_at "
                "FROM subtasks")) {
                return rollbackWithError("Failed to copy subtasks into v7 schema:");
            }

            if (!query.exec("DROP TABLE subtasks")) {
                return rollbackWithError("Failed to drop legacy subtasks table:");
            }

            if (!query.exec("ALTER TABLE subtasks_v7 RENAME TO subtasks")) {
                return rollbackWithError("Failed to rename subtasks_v7 table:");
            }

            if (!query.exec(
                "CREATE TABLE IF NOT EXISTS subtask_content_history ("
                "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "  subtask_id INTEGER NOT NULL,"
                "  content TEXT NOT NULL,"
                "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
                "  FOREIGN KEY (subtask_id) REFERENCES subtasks(id) ON DELETE CASCADE"
                ")")) {
                return rollbackWithError("Failed to create subtask_content_history table:");
            }

            query.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES ('db_version', ?)");
            query.addBindValue(QStringLiteral("7"));
            if (!query.exec()) {
                return rollbackWithError("Failed to update db_version to 7:");
            }

            if (!m_db.commit()) {
                qCritical() << "Failed to commit v7 migration:" << m_db.lastError().text();
                m_db.rollback();
                return false;
            }
        } else {
            if (!query.exec(
                "CREATE TABLE IF NOT EXISTS subtask_content_history ("
                "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "  subtask_id INTEGER NOT NULL,"
                "  content TEXT NOT NULL,"
                "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
                "  FOREIGN KEY (subtask_id) REFERENCES subtasks(id) ON DELETE CASCADE"
                ")")) {
                qCritical() << "Failed to create subtask_content_history table:" << query.lastError().text();
                return false;
            }

            query.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES ('db_version', ?)");
            query.addBindValue(QStringLiteral("7"));
            if (!query.exec()) {
                qCritical() << "Failed to update db_version to 7:" << query.lastError().text();
                return false;
            }
        }

        qInfo() << "Migration v7: enabled subtask note storage";
        dbVersion = 7;
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
    const int id = executeInsertWithFtsRecovery(
        [&](QSqlQuery &query) {
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
        });
    if (id > 0) {
        emit taskAdded(id);
        return id;
    }
    qWarning() << "Failed to add task.";
    return -1;
}

bool DatabaseManager::updateTask(const Task &task)
{
    if (executeWithFtsRecovery([&](QSqlQuery &query) {
            query.prepare(
                "UPDATE tasks SET title = ?, content = ?, priority = ?, status = ?, "
                "sort_order = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
            query.addBindValue(task.title);
            query.addBindValue(task.content);
            query.addBindValue(static_cast<int>(task.priority));
            query.addBindValue(task.status == TaskStatus::Active ? "active" : "archived");
            query.addBindValue(task.sortOrder);
            query.addBindValue(task.id);
        })) {
        emit taskUpdated(task.id);
        return true;
    }
    return false;
}

bool DatabaseManager::updateTaskTitle(int taskId, const QString &title)
{
    if (executeWithFtsRecovery([&](QSqlQuery &query) {
            query.prepare("UPDATE tasks SET title = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
            query.addBindValue(title);
            query.addBindValue(taskId);
        })) {
        emit taskUpdated(taskId);
        return true;
    }
    return false;
}

bool DatabaseManager::updateTaskContent(int taskId, const QString &content)
{
    if (executeWithFtsRecovery([&](QSqlQuery &query) {
            query.prepare("UPDATE tasks SET content = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
            query.addBindValue(content);
            query.addBindValue(taskId);
        })) {
        emit taskUpdated(taskId);
        return true;
    }
    return false;
}

bool DatabaseManager::updateTaskPriority(int taskId, TaskPriority priority)
{
    if (executeWithFtsRecovery([&](QSqlQuery &query) {
            query.prepare("UPDATE tasks SET priority = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
            query.addBindValue(static_cast<int>(priority));
            query.addBindValue(taskId);
        })) {
        emit taskUpdated(taskId);
        return true;
    }
    return false;
}

bool DatabaseManager::updateTaskDueDate(int taskId, const QDateTime &dueDate)
{
    if (executeWithFtsRecovery([&](QSqlQuery &query) {
            query.prepare("UPDATE tasks SET due_date = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
            if (dueDate.isValid()) {
                query.addBindValue(dueDate.toString(Qt::ISODate));
            } else {
                query.addBindValue(QVariant());
            }
            query.addBindValue(taskId);
        })) {
        emit taskUpdated(taskId);
        return true;
    }
    return false;
}

bool DatabaseManager::archiveTask(int taskId)
{
    if (executeWithFtsRecovery([&](QSqlQuery &query) {
            query.prepare("UPDATE tasks SET status = 'archived', archived_at = CURRENT_TIMESTAMP, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
            query.addBindValue(taskId);
        })) {
        emit taskArchived(taskId);
        return true;
    }
    return false;
}

bool DatabaseManager::updateTaskWorkStatus(int taskId, TaskWorkStatus workStatus)
{
    if (executeWithFtsRecovery([&](QSqlQuery &query) {
            query.prepare("UPDATE tasks SET work_status = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
            query.addBindValue(static_cast<int>(workStatus));
            query.addBindValue(taskId);
        })) {
        emit taskUpdated(taskId);
        return true;
    }
    return false;
}

bool DatabaseManager::reactivateTask(int taskId)
{
    if (executeWithFtsRecovery([&](QSqlQuery &query) {
            query.prepare("UPDATE tasks SET status = 'active', archived_at = NULL, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
            query.addBindValue(taskId);
        })) {
        emit taskReactivated(taskId);
        return true;
    }
    return false;
}

bool DatabaseManager::deleteTask(int taskId)
{
    if (executeWithFtsRecovery([&](QSqlQuery &query) {
            query.prepare("UPDATE tasks SET status = 'deleted', deleted_at = CURRENT_TIMESTAMP WHERE id = ?");
            query.addBindValue(taskId);
        })) {
        emit taskDeleted(taskId);
        return true;
    }
    return false;
}

bool DatabaseManager::restoreTask(int taskId)
{
    return executeWithFtsRecovery([&](QSqlQuery &query) {
        query.prepare("UPDATE tasks SET status = 'active', deleted_at = NULL WHERE id = ?");
        query.addBindValue(taskId);
    });
}

bool DatabaseManager::permanentlyDeleteTask(int taskId)
{
    return executeWithFtsRecovery([&](QSqlQuery &query) {
        query.prepare("DELETE FROM tasks WHERE id = ?");
        query.addBindValue(taskId);
    });
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
    query.prepare("SELECT id, task_id, title, content, completed, sort_order, created_at, updated_at FROM subtasks "
                  "WHERE task_id = ? ORDER BY sort_order ASC, id ASC");
    query.addBindValue(taskId);
    query.exec();
    while (query.next()) {
        SubTask st;
        st.id = query.value(0).toInt();
        st.taskId = query.value(1).toInt();
        st.title = query.value(2).toString();
        st.content = query.value(3).toString();
        st.completed = query.value(4).toBool();
        st.sortOrder = query.value(5).toInt();
        st.createdAt = query.value(6).toDateTime();
        st.updatedAt = query.value(7).toDateTime();
        subtasks.append(st);
    }
    return subtasks;
}

SubTask DatabaseManager::getSubtask(int subtaskId)
{
    SubTask subtask;
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT id, task_id, title, content, completed, sort_order, created_at, updated_at "
        "FROM subtasks WHERE id = ?");
    query.addBindValue(subtaskId);
    if (query.exec() && query.next()) {
        subtask.id = query.value(0).toInt();
        subtask.taskId = query.value(1).toInt();
        subtask.title = query.value(2).toString();
        subtask.content = query.value(3).toString();
        subtask.completed = query.value(4).toBool();
        subtask.sortOrder = query.value(5).toInt();
        subtask.createdAt = query.value(6).toDateTime();
        subtask.updatedAt = query.value(7).toDateTime();
    }
    return subtask;
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
    return executeInsertWithFtsRecovery([&](QSqlQuery &query) {
        query.prepare("INSERT INTO subtasks (task_id, title, sort_order) "
                      "VALUES (?, ?, (SELECT COALESCE(MAX(sort_order), 0) + 1 FROM subtasks WHERE task_id = ?))");
        query.addBindValue(taskId);
        query.addBindValue(title);
        query.addBindValue(taskId);
    });
}

bool DatabaseManager::toggleSubtask(int subtaskId, bool completed)
{
    return executeWithFtsRecovery([&](QSqlQuery &query) {
        query.prepare("UPDATE subtasks SET completed = ? WHERE id = ?");
        query.addBindValue(completed ? 1 : 0);
        query.addBindValue(subtaskId);
    });
}

bool DatabaseManager::deleteSubtask(int subtaskId)
{
    return executeWithFtsRecovery([&](QSqlQuery &query) {
        query.prepare("DELETE FROM subtasks WHERE id = ?");
        query.addBindValue(subtaskId);
    });
}

bool DatabaseManager::renameSubtask(int subtaskId, const QString &title)
{
    return updateSubtaskTitle(subtaskId, title);
}

bool DatabaseManager::updateSubtaskTitle(int subtaskId, const QString &title)
{
    return executeWithFtsRecovery([&](QSqlQuery &query) {
        query.prepare("UPDATE subtasks SET title = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
        query.addBindValue(title);
        query.addBindValue(subtaskId);
    });
}

bool DatabaseManager::updateSubtaskContent(int subtaskId, const QString &content)
{
    return executeWithFtsRecovery([&](QSqlQuery &query) {
        query.prepare("UPDATE subtasks SET content = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
        query.addBindValue(content);
        query.addBindValue(subtaskId);
    });
}

// --- Content History (Undo/Redo) ---

bool DatabaseManager::saveContentSnapshot(int taskId, const QString &content)
{
    // Clean up old snapshots first
    cleanupOldHistory(taskId);

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO task_content_history (task_id, content) VALUES (?, ?)");
    query.addBindValue(taskId);
    query.addBindValue(content);
    return query.exec();
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

bool DatabaseManager::saveSubtaskContentSnapshot(int subtaskId, const QString &content)
{
    cleanupOldSubtaskHistory(subtaskId);

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO subtask_content_history (subtask_id, content) VALUES (?, ?)");
    query.addBindValue(subtaskId);
    query.addBindValue(content);
    return query.exec();
}

QList<QString> DatabaseManager::getSubtaskContentHistory(int subtaskId)
{
    QList<QString> history;
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT content FROM subtask_content_history "
        "WHERE subtask_id = ? AND created_at >= datetime('now', '-60 minutes') "
        "ORDER BY id ASC");
    query.addBindValue(subtaskId);
    query.exec();

    while (query.next()) {
        history.append(query.value(0).toString());
    }
    return history;
}

void DatabaseManager::cleanupOldSubtaskHistory(int subtaskId, int maxAgeMinutes)
{
    QSqlQuery query(m_db);
    query.prepare(
        "DELETE FROM subtask_content_history "
        "WHERE subtask_id = ? AND created_at < datetime('now', '-' || ? || ' minutes')");
    query.addBindValue(subtaskId);
    query.addBindValue(maxAgeMinutes);
    query.exec();
}

// --- Search ---

QList<SearchResult> DatabaseManager::searchItems(const QString &query, int productId)
{
    QList<SearchResult> results;
    const QString trimmedQuery = query.trimmed();
    if (trimmedQuery.isEmpty()) {
        return results;
    }

    const QStringList tokens = searchTokens(trimmedQuery);
    const QString ftsQuery = buildFtsMatchQuery(tokens);

    auto appendTaskMatches = [&results](QSqlQuery &searchQuery) {
        while (searchQuery.next()) {
            SearchResult result;
            result.parentTask = taskFromSearchQuery(searchQuery);
            results.append(result);
        }
    };

    auto appendSubtaskMatches = [&results](QSqlQuery &searchQuery) {
        while (searchQuery.next()) {
            SearchResult result;
            result.parentTask = taskFromSearchQuery(searchQuery, 0);
            result.matchedSubtask = subtaskFromSearchQuery(searchQuery, 12);
            result.isSubtaskMatch = true;
            results.append(result);
        }
    };

    QString taskFtsSql =
        "SELECT t.id, t.product_id, t.title, t.content, t.priority, t.status, "
        "t.sort_order, t.created_at, t.updated_at, t.archived_at, t.due_date, "
        "t.work_status "
        "FROM tasks t INNER JOIN tasks_fts f ON t.id = f.rowid "
        "WHERE tasks_fts MATCH ?";
    if (productId > 0) {
        taskFtsSql += " AND t.product_id = ?";
    }
    taskFtsSql += " ORDER BY rank";

    auto runTaskLikeFallback = [&]() {
        QSqlQuery fallbackQuery(m_db);
        QString taskLikeSql =
            "SELECT id, product_id, title, content, priority, status, sort_order, "
            "created_at, updated_at, archived_at, due_date, work_status "
            "FROM tasks WHERE " + buildLikeSearchClause("title", "content", tokens.size());
        if (productId > 0) {
            taskLikeSql += " AND product_id = ?";
        }

        fallbackQuery.prepare(taskLikeSql);
        bindLikeSearchTokens(fallbackQuery, tokens);
        if (productId > 0) {
            fallbackQuery.addBindValue(productId);
        }
        if (!fallbackQuery.exec()) {
            qWarning() << "Task LIKE search failed:" << fallbackQuery.lastError().text();
            return;
        }
        appendTaskMatches(fallbackQuery);
    };

    {
        QSqlQuery taskQuery(m_db);
        if (!taskQuery.prepare(taskFtsSql)) {
            qWarning() << "Task FTS search unavailable, using LIKE fallback:" << taskQuery.lastError().text();
            runTaskLikeFallback();
        } else {
            taskQuery.addBindValue(ftsQuery);
            if (productId > 0) {
                taskQuery.addBindValue(productId);
            }

            if (!taskQuery.exec()) {
                qWarning() << "Task FTS search failed, using LIKE fallback:" << taskQuery.lastError().text();
                runTaskLikeFallback();
            } else {
                appendTaskMatches(taskQuery);
            }
        }
    }

    QString subtaskFtsSql =
        "SELECT "
        "  t.id, t.product_id, t.title, t.content, t.priority, t.status, "
        "  t.sort_order, t.created_at, t.updated_at, t.archived_at, t.due_date, t.work_status, "
        "  s.id, s.task_id, s.title, s.content, s.completed, s.sort_order, s.created_at, s.updated_at "
        "FROM subtasks s "
        "INNER JOIN subtasks_fts f ON s.id = f.rowid "
        "INNER JOIN tasks t ON t.id = s.task_id "
        "WHERE subtasks_fts MATCH ?";
    if (productId > 0) {
        subtaskFtsSql += " AND t.product_id = ?";
    }
    subtaskFtsSql += " ORDER BY rank";

    auto runSubtaskLikeFallback = [&]() {
        QSqlQuery fallbackQuery(m_db);
        QString subtaskLikeSql =
            "SELECT "
            "  t.id, t.product_id, t.title, t.content, t.priority, t.status, "
            "  t.sort_order, t.created_at, t.updated_at, t.archived_at, t.due_date, t.work_status, "
            "  s.id, s.task_id, s.title, s.content, s.completed, s.sort_order, s.created_at, s.updated_at "
            "FROM subtasks s "
            "INNER JOIN tasks t ON t.id = s.task_id "
            "WHERE " + buildLikeSearchClause("s.title", "s.content", tokens.size());
        if (productId > 0) {
            subtaskLikeSql += " AND t.product_id = ?";
        }

        fallbackQuery.prepare(subtaskLikeSql);
        bindLikeSearchTokens(fallbackQuery, tokens);
        if (productId > 0) {
            fallbackQuery.addBindValue(productId);
        }
        if (!fallbackQuery.exec()) {
            qWarning() << "Subtask LIKE search failed:" << fallbackQuery.lastError().text();
            return;
        }
        appendSubtaskMatches(fallbackQuery);
    };

    {
        QSqlQuery subtaskQuery(m_db);
        if (!subtaskQuery.prepare(subtaskFtsSql)) {
            qWarning() << "Subtask FTS search unavailable, using LIKE fallback:" << subtaskQuery.lastError().text();
            runSubtaskLikeFallback();
        } else {
            subtaskQuery.addBindValue(ftsQuery);
            if (productId > 0) {
                subtaskQuery.addBindValue(productId);
            }

            if (!subtaskQuery.exec()) {
                qWarning() << "Subtask FTS search failed, using LIKE fallback:" << subtaskQuery.lastError().text();
                runSubtaskLikeFallback();
            } else {
                appendSubtaskMatches(subtaskQuery);
            }
        }
    }

    return results;
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
