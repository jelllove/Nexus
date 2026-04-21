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
    Archived
};

struct Task {
    int id = 0;
    int productId = 0;
    QString title;
    QString content;
    TaskPriority priority = TaskPriority::Medium;
    TaskStatus status = TaskStatus::Active;
    int sortOrder = 0;
    QDateTime createdAt;
    QDateTime updatedAt;
    QDateTime archivedAt;
    QDateTime dueDate;
    bool completed = false;
    QDateTime completedAt;

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
            case TaskPriority::Critical: return QColor("#fdf0ef");  // Very light red
            case TaskPriority::High:     return QColor("#fef5ec");  // Very light orange
            case TaskPriority::Medium:   return QColor("#edf5fc");  // Very light blue
            case TaskPriority::Low:      return QColor("#f4f5f5");  // Very light gray
        }
        return QColor("#edf5fc");
    }

    static QColor priorityBarColor(TaskPriority p) {
        switch (p) {
            case TaskPriority::Critical: return QColor("#f1948a");  // Muted red
            case TaskPriority::High:     return QColor("#f0b27a");  // Muted orange
            case TaskPriority::Medium:   return QColor("#85c1e9");  // Muted blue
            case TaskPriority::Low:      return QColor("#c5cccd");  // Muted gray
        }
        return QColor("#85c1e9");
    }

    static QColor priorityBarTrackColor(TaskPriority p) {
        switch (p) {
            case TaskPriority::Critical: return QColor("#f9dbd8");  // Lighter red
            case TaskPriority::High:     return QColor("#fce4cc");  // Lighter orange
            case TaskPriority::Medium:   return QColor("#d4e6f6");  // Lighter blue
            case TaskPriority::Low:      return QColor("#e8eaeb");  // Lighter gray
        }
        return QColor("#d4e6f6");
    }
};
