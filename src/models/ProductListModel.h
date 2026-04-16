#pragma once

#include <QAbstractListModel>
#include <QList>
#include "models/Product.h"

class ProductListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        SortOrderRole
    };

    explicit ProductListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    void loadProducts();
    int productIdAt(int row) const;
    int rowForProductId(int productId) const;
    void addProduct(const QString &name);
    void removeProduct(int row);

private:
    QList<Product> m_products;
};
