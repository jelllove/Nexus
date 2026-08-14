#include "ProductPane.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QMenu>
#include <QAction>

ProductPane::ProductPane(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setupContextMenu();
}

void ProductPane::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Title
    m_titleLabel = new QLabel("Products", this);
    m_titleLabel->setStyleSheet(
        "QLabel { font-size: 14px; font-weight: bold; padding: 8px; "
        "background-color: #2c3e50; color: white; }");
    layout->addWidget(m_titleLabel);

    // List view
    m_model = new ProductListModel(this);
    m_listView = new QListView(this);
    m_listView->setModel(m_model);
    m_listView->setEditTriggers(QAbstractItemView::DoubleClicked);
    m_listView->setStyleSheet(
        "QListView { border: none; background-color: #34495e; color: white; "
        "font-size: 13px; }"
        "QListView::item { padding: 10px 12px; border-bottom: 1px solid #3d566e; }"
        "QListView::item:selected { background-color: #2980b9; }"
        "QListView::item:hover { background-color: #3d566e; }");
    layout->addWidget(m_listView, 1);

    // Add button
    m_addButton = new QPushButton("+ Add Product", this);
    m_addButton->setStyleSheet(
        "QPushButton { background-color: #27ae60; color: white; border: none; "
        "padding: 8px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #2ecc71; }");
    layout->addWidget(m_addButton);

    connect(m_listView, &QListView::clicked, this, &ProductPane::onProductClicked);
    connect(m_addButton, &QPushButton::clicked, this, &ProductPane::onAddProduct);

    setStyleSheet("background-color: #34495e;");
}

void ProductPane::setupContextMenu()
{
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listView, &QListView::customContextMenuRequested, this, [this](const QPoint &pos) {
        QModelIndex index = m_listView->indexAt(pos);
        if (!index.isValid()) return;

        QMenu menu(this);
        menu.setStyleSheet(
            "QMenu { background-color: #2c3e50; color: white; border: 1px solid #3d566e; }"
            "QMenu::item:selected { background-color: #2980b9; }");

        QAction *renameAction = menu.addAction("Rename");
        QAction *deleteAction = menu.addAction("Delete");

        QAction *selected = menu.exec(m_listView->viewport()->mapToGlobal(pos));
        if (selected == renameAction) {
            m_listView->edit(index);
        } else if (selected == deleteAction) {
            onDeleteProduct();
        }
    });
}

void ProductPane::loadProducts()
{
    m_model->loadProducts();
}

int ProductPane::selectedProductId() const
{
    QModelIndex index = m_listView->currentIndex();
    if (!index.isValid()) return -1;
    return m_model->productIdAt(index.row());
}

void ProductPane::setSelectedProduct(int productId)
{
    if (productId <= 0) {
        m_listView->clearSelection();
        m_listView->setCurrentIndex(QModelIndex());
        return;
    }

    const int row = m_model->rowForProductId(productId);
    if (row >= 0) {
        m_listView->setCurrentIndex(m_model->index(row, 0));
    }
}

void ProductPane::onProductClicked(const QModelIndex &index)
{
    int productId = m_model->productIdAt(index.row());
    emit productSelected(productId);
}

void ProductPane::onAddProduct()
{
    bool ok;
    QString name = QInputDialog::getText(this, "New Product", "Product name:",
                                          QLineEdit::Normal, "", &ok);
    if (ok && !name.trimmed().isEmpty()) {
        m_model->addProduct(name.trimmed());
        // Select the newly added product
        int lastRow = m_model->rowCount() - 1;
        if (lastRow >= 0) {
            QModelIndex idx = m_model->index(lastRow);
            m_listView->setCurrentIndex(idx);
            emit productSelected(m_model->productIdAt(lastRow));
        }
    }
}

void ProductPane::onDeleteProduct()
{
    QModelIndex index = m_listView->currentIndex();
    if (!index.isValid()) return;

    QString name = index.data(Qt::DisplayRole).toString();
    auto result = QMessageBox::question(this, "Delete Product",
        QString("Delete product '%1' and all its tasks?").arg(name),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
        m_model->removeProduct(index.row());
    }
}

void ProductPane::onRenameProduct()
{
    QModelIndex index = m_listView->currentIndex();
    if (index.isValid()) {
        m_listView->edit(index);
    }
}
