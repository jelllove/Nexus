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
    Qt::ItemFlags baseFlags = QAbstractListModel::flags(index);
    if (!index.isValid()) {
        return baseFlags | Qt::ItemIsDropEnabled;
    }
    return baseFlags | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
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

Qt::DropActions ProductListModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

bool ProductListModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                                const QModelIndex &destinationParent, int destinationRow)
{
    Q_UNUSED(sourceParent);
    Q_UNUSED(destinationParent);

    if (count != 1) return false;
    if (sourceRow < 0 || sourceRow >= m_products.size()) return false;
    if (destinationRow < 0 || destinationRow > m_products.size()) return false;
    if (destinationRow == sourceRow || destinationRow == sourceRow + 1) return false;

    const int targetRow = destinationRow > sourceRow ? destinationRow - 1 : destinationRow;
    beginMoveRows(QModelIndex(), sourceRow, sourceRow, QModelIndex(), destinationRow);
    m_products.move(sourceRow, targetRow);
    endMoveRows();

    QList<int> productIds;
    productIds.reserve(m_products.size());
    for (const Product &p : m_products) {
        productIds.append(p.id);
    }

    if (!DatabaseManager::instance().reorderProductsByStatus(productIds, m_status)) {
        loadProducts(m_status);
        return false;
    }

    return true;
}

void ProductListModel::loadProducts(ProductStatus status)
{
    m_status = status;
    beginResetModel();
    m_products = DatabaseManager::instance().getProductsByStatus(status);
    endResetModel();
}

void ProductListModel::setStatus(ProductStatus status)
{
    if (m_status == status) {
        return;
    }
    loadProducts(status);
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
