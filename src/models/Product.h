#pragma once

#include <QString>
#include <QDateTime>

enum class ProductStatus {
    Active,
    Archived
};

struct Product {
    int id = 0;
    QString name;
    int sortOrder = 0;
    ProductStatus status = ProductStatus::Active;
    QDateTime archivedAt;
    QDateTime createdAt;
    QDateTime updatedAt;
};
