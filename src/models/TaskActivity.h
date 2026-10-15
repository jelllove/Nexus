#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

enum class ActivityTimeBasis { Created, Modified, CreatedOrModified };

struct ActivityChange {
    QDateTime occurredAt;
    QString operation;
    QString changedFields;
    QString oldStatus;
    QString newStatus;
    int oldWorkStatus = -1;
    int newWorkStatus = -1;
};

struct TaskActivity {
    QString tableName;
    int id = -1;
    QString productName;
    QString parentTitle;
    QString title;
    QString content;
    QDateTime createdAt;
    QDateTime updatedAt;
    QString lifecycleStatus;
    int workStatus = 0;
    QList<ActivityChange> changes;
};
