#include "ProductListModel.h"
#include "db/DatabaseManager.h"

ProductListModel::ProductListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ProductListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_products.size();
}

QVariant ProductListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_products.size())
        return QVariant();

    const Product &p = m_products[index.row()];

    switch (role) {
        case Qt::DisplayRole:
        case NameRole:
            return p.name;
        case IdRole:
            return p.id;
        case SortOrderRole:
            return p.sortOrder;
        case Qt::EditRole:
            return p.name;
    }
    return QVariant();
}

Qt::ItemFlags ProductListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

bool ProductListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;

    QString newName = value.toString().trimmed();
    if (newName.isEmpty())
        return false;

    Product &p = m_products[index.row()];
    if (DatabaseManager::instance().updateProduct(p.id, newName)) {
        p.name = newName;
        emit dataChanged(index, index, {Qt::DisplayRole, NameRole});
        return true;
    }
    return false;
}

void ProductListModel::loadProducts()
{
    beginResetModel();
    m_products = DatabaseManager::instance().getAllProducts();
    endResetModel();
}

int ProductListModel::productIdAt(int row) const
{
    if (row < 0 || row >= m_products.size()) return -1;
    return m_products[row].id;
}

int ProductListModel::rowForProductId(int productId) const
{
    for (int i = 0; i < m_products.size(); ++i) {
        if (m_products[i].id == productId) return i;
    }
    return -1;
}

void ProductListModel::addProduct(const QString &name)
{
    int id = DatabaseManager::instance().addProduct(name);
    if (id > 0) {
        loadProducts();
    }
}

void ProductListModel::removeProduct(int row)
{
    if (row < 0 || row >= m_products.size()) return;
    int id = m_products[row].id;
    if (DatabaseManager::instance().deleteProduct(id)) {
        loadProducts();
    }
}
