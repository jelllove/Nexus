#pragma once

#include <QString>
#include <QDateTime>

struct Product {
    int id = 0;
    QString name;
    int sortOrder = 0;
    QDateTime createdAt;
    QDateTime updatedAt;
};
