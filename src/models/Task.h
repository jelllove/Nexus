#pragma once

#include <QString>
#include <QDateTime>
#include <QColor>

enum class TaskPriority {
    Critical = 0,  // P0
    High = 1,      // P1
    Medium = 2,    // P2
    Low = 3        // P3
};

enum class TaskStatus {
    Active,
    Archived,
    Deleted
};

enum class TaskWorkStatus {
    NotStarted = 0,
    Ongoing = 1,
    Paused = 2,
    Completed = 3,
    Waiting = 4
};

struct SubTask {
    int id = -1;
    int taskId = -1;
    QString title;
    QString content;
    bool completed = false;
    TaskWorkStatus workStatus = TaskWorkStatus::NotStarted;
    int sortOrder = 0;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct Task {
    int id = 0;
    int productId = 0;
    QString title;
    QString content;
    TaskPriority priority = TaskPriority::Medium;
    TaskStatus status = TaskStatus::Active;
    TaskWorkStatus workStatus = TaskWorkStatus::NotStarted;
    int sortOrder = 0;
    QDateTime createdAt;
    QDateTime updatedAt;
    QDateTime archivedAt;
    QDateTime dueDate;

    static QString dueDateIcon(int taskId) {
        static const QString icons[] = {
            "\xF0\x9F\x91\xA4",  // 👤 person
            "\xF0\x9F\x90\xB6",  // 🐶 dog
            "\xF0\x9F\x90\xB1",  // 🐱 cat
            "\xF0\x9F\x90\xBB",  // 🐻 bear
            "\xF0\x9F\xA6\x8A",  // 🦊 fox
            "\xF0\x9F\x90\xA2",  // 🐢 turtle
            "\xF0\x9F\xA6\x89",  // 🦉 owl
            "\xF0\x9F\x90\xAC"   // 🐬 dolphin
        };
        return icons[taskId % 8];
    }

    static QString priorityToString(TaskPriority p) {
        switch (p) {
            case TaskPriority::Critical: return "P0 - Critical";
            case TaskPriority::High:     return "P1 - High";
            case TaskPriority::Medium:   return "P2 - Medium";
            case TaskPriority::Low:      return "P3 - Low";
        }
        return "P2 - Medium";
    }

    static QColor priorityColor(TaskPriority p) {
        switch (p) {
            case TaskPriority::Critical: return QColor("#e74c3c");  // Red
            case TaskPriority::High:     return QColor("#e67e22");  // Orange
            case TaskPriority::Medium:   return QColor("#3498db");  // Blue
            case TaskPriority::Low:      return QColor("#95a5a6");  // Gray
        }
        return QColor("#3498db");
    }

    static QColor priorityBackgroundColor(TaskPriority p) {
        switch (p) {
            case TaskPriority::Critical: return QColor("#fdf0ef");
            case TaskPriority::High:     return QColor("#fef5ec");
            case TaskPriority::Medium:   return QColor("#edf5fc");
            case TaskPriority::Low:      return QColor("#f4f5f5");
        }
        return QColor("#edf5fc");
    }

    static QColor priorityBarColor(TaskPriority p) {
        switch (p) {
            case TaskPriority::Critical: return QColor("#f1948a");
            case TaskPriority::High:     return QColor("#f0b27a");
            case TaskPriority::Medium:   return QColor("#85c1e9");
            case TaskPriority::Low:      return QColor("#c5cccd");
        }
        return QColor("#85c1e9");
    }

    static QColor priorityBarTrackColor(TaskPriority p) {
        switch (p) {
            case TaskPriority::Critical: return QColor("#f9dbd8");
            case TaskPriority::High:     return QColor("#fce4cc");
            case TaskPriority::Medium:   return QColor("#d4e6f6");
            case TaskPriority::Low:      return QColor("#e8eaeb");
        }
        return QColor("#d4e6f6");
    }

    // Work status icon — clean geometric icons inspired by Linear/Notion
    static QString workStatusIcon(TaskWorkStatus s) {
        switch (s) {
            case TaskWorkStatus::NotStarted: return QString::fromUtf8("\xE2\x8F\xAF\xEF\xB8\x8F");      // ⏯️
            case TaskWorkStatus::Ongoing:    return QString::fromUtf8("\xF0\x9F\x8F\x83");      // 🏃
            case TaskWorkStatus::Paused:     return QString::fromUtf8("\xE2\x8F\xB8\xEF\xB8\x8F"); // ⏸️
            case TaskWorkStatus::Completed:  return QString::fromUtf8("\xE2\x9C\x85");          // ✅
            case TaskWorkStatus::Waiting:    return QString::fromUtf8("\xE2\x8F\xB3");          // ⏳
        }
        return QString::fromUtf8("\xF0\x9F\x94\x98");
    }

    static QString workStatusToString(TaskWorkStatus s) {
        switch (s) {
            case TaskWorkStatus::NotStarted: return "Not Started";
            case TaskWorkStatus::Ongoing:    return "Ongoing";
            case TaskWorkStatus::Paused:     return "Paused";
            case TaskWorkStatus::Completed:  return "Completed";
            case TaskWorkStatus::Waiting:    return "Waiting";
        }
        return "Not Started";
    }

    static QColor workStatusColor(TaskWorkStatus s) {
        switch (s) {
            case TaskWorkStatus::NotStarted: return QColor("#95a5a6");  // Gray
            case TaskWorkStatus::Ongoing:    return QColor("#2ecc71");  // Green
            case TaskWorkStatus::Paused:     return QColor("#f39c12");  // Amber
            case TaskWorkStatus::Completed:  return QColor("#27ae60");  // Dark green
            case TaskWorkStatus::Waiting:    return QColor("#8e44ad");  // Purple
        }
        return QColor("#95a5a6");
    }
};
