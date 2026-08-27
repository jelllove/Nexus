#include "ProductPane.h"
#include "db/DatabaseManager.h"
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

    auto configureList = [this](QListView *listView, ProductListModel *model) {
        listView->setModel(model);
        listView->setEditTriggers(QAbstractItemView::DoubleClicked);
        listView->setDragEnabled(true);
        listView->setAcceptDrops(true);
        listView->setDropIndicatorShown(true);
        listView->setDragDropMode(QAbstractItemView::InternalMove);
        listView->setDefaultDropAction(Qt::MoveAction);
        listView->setStyleSheet(
        "QListView { border: none; background-color: #34495e; color: white; "
        "font-size: 13px; }"
        "QListView::item { padding: 10px 12px; border-bottom: 1px solid #3d566e; }"
        "QListView::item:selected { background-color: #2980b9; }"
        "QListView::item:hover { background-color: #3d566e; }");
    };

    // Active products list
    m_activeModel = new ProductListModel(this);
    m_activeListView = new QListView(this);
    configureList(m_activeListView, m_activeModel);
    layout->addWidget(m_activeListView, 1);

    // Archived section toggle
    m_showArchivedButton = new QPushButton("Show Archived Products", this);
    m_showArchivedButton->setCheckable(true);
    m_showArchivedButton->setStyleSheet(
        "QPushButton { background-color: #3d566e; color: #ecf0f1; border: none; "
        "padding: 7px; font-size: 11px; font-weight: bold; }"
        "QPushButton:hover { background-color: #4a6580; }"
        "QPushButton:checked { background-color: #6c5ce7; color: white; }");
    layout->addWidget(m_showArchivedButton);

    // Archived products section
    m_archivedSection = new QWidget(this);
    auto *archivedLayout = new QVBoxLayout(m_archivedSection);
    archivedLayout->setContentsMargins(0, 0, 0, 0);
    archivedLayout->setSpacing(4);

    auto *archivedLabel = new QLabel("Archived Products", m_archivedSection);
    archivedLabel->setStyleSheet(
        "QLabel { font-size: 12px; font-weight: bold; padding: 6px 8px; "
        "background-color: #2d3436; color: #ffeaa7; }");
    archivedLayout->addWidget(archivedLabel);

    m_archivedModel = new ProductListModel(this);
    m_archivedListView = new QListView(m_archivedSection);
    configureList(m_archivedListView, m_archivedModel);
    archivedLayout->addWidget(m_archivedListView, 1);

    m_archivedSection->setVisible(false);
    layout->addWidget(m_archivedSection, 1);

    // Add button
    m_addButton = new QPushButton("+ Add Product", this);
    m_addButton->setStyleSheet(
        "QPushButton { background-color: #27ae60; color: white; border: none; "
        "padding: 8px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #2ecc71; }");
    layout->addWidget(m_addButton);

    connect(m_activeListView, &QListView::clicked, this, [this](const QModelIndex &index) {
        onProductClicked(m_activeListView, m_activeModel, index);
    });
    connect(m_archivedListView, &QListView::clicked, this, [this](const QModelIndex &index) {
        onProductClicked(m_archivedListView, m_archivedModel, index);
    });
    connect(m_addButton, &QPushButton::clicked, this, &ProductPane::onAddProduct);
    connect(m_showArchivedButton, &QPushButton::toggled, this, [this](bool checked) {
        m_archivedSection->setVisible(checked);
        m_showArchivedButton->setText(checked ? "Hide Archived Products"
                                              : "Show Archived Products");
    });

    setStyleSheet("background-color: #34495e;");
}

void ProductPane::setupContextMenu()
{
    auto installContextMenu = [this](QListView *listView, ProductStatus listStatus) {
        listView->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(listView, &QListView::customContextMenuRequested, this,
                [this, listView, listStatus](const QPoint &pos) {
                    ProductListModel *model = modelForView(listView);
                    if (!model) return;

                    QModelIndex index = listView->indexAt(pos);
                    if (!index.isValid()) return;

                    setCurrentListView(listView);
                    listView->setCurrentIndex(index);

                    QMenu menu(this);
                    menu.setStyleSheet(
                        "QMenu { background-color: #2c3e50; color: white; border: 1px solid #3d566e; }"
                        "QMenu::item:selected { background-color: #2980b9; }");

                    QAction *renameAction = menu.addAction("Rename");
                    QAction *archiveToggleAction = (listStatus == ProductStatus::Active)
                        ? menu.addAction("Archive")
                        : menu.addAction("Reactivate");
                    QAction *deleteAction = menu.addAction("Delete");

                    QAction *selected = menu.exec(listView->viewport()->mapToGlobal(pos));
                    if (!selected) return;

                    if (selected == renameAction) {
                        onRenameProduct();
                        return;
                    }

                    const int productId = model->productIdAt(index.row());
                    if (selected == archiveToggleAction) {
                        const bool ok = (listStatus == ProductStatus::Active)
                            ? DatabaseManager::instance().archiveProduct(productId)
                            : DatabaseManager::instance().reactivateProduct(productId);
                        if (ok) {
                            loadProducts();
                        }
                        return;
                    }

                    if (selected == deleteAction) {
                        onDeleteProduct();
                    }
                });
    };

    installContextMenu(m_activeListView, ProductStatus::Active);
    installContextMenu(m_archivedListView, ProductStatus::Archived);
}

void ProductPane::loadProducts()
{
    const int currentProduct = selectedProductId();

    m_activeModel->loadProducts(ProductStatus::Active);
    m_archivedModel->loadProducts(ProductStatus::Archived);

    if (currentProduct <= 0) {
        return;
    }
    selectProductById(currentProduct, true);
}

int ProductPane::selectedProductId() const
{
    QListView *view = m_currentListView ? m_currentListView : m_activeListView;
    ProductListModel *model = modelForView(view);
    if (!model) return -1;

    QModelIndex index = view->currentIndex();
    if (!index.isValid()) return -1;
    return model->productIdAt(index.row());
}

bool ProductPane::selectProductById(int productId, bool emitSelection)
{
    if (productId <= 0) {
        return false;
    }

    const int activeRow = m_activeModel->rowForProductId(productId);
    if (activeRow >= 0) {
        QModelIndex idx = m_activeModel->index(activeRow);
        setCurrentListView(m_activeListView);
        m_activeListView->setCurrentIndex(idx);
        m_activeListView->scrollTo(idx);
        if (emitSelection) {
            emit productSelected(productId);
        }
        return true;
    }

    const int archivedRow = m_archivedModel->rowForProductId(productId);
    if (archivedRow >= 0) {
        if (!m_showArchivedButton->isChecked()) {
            m_showArchivedButton->setChecked(true);
        }
        QModelIndex idx = m_archivedModel->index(archivedRow);
        setCurrentListView(m_archivedListView);
        m_archivedListView->setCurrentIndex(idx);
        m_archivedListView->scrollTo(idx);
        if (emitSelection) {
            emit productSelected(productId);
        }
        return true;
    }

    return false;
}

void ProductPane::onProductClicked(QListView *sourceView, ProductListModel *sourceModel,
                                   const QModelIndex &index)
{
    if (!index.isValid() || !sourceModel) {
        return;
    }

    setCurrentListView(sourceView);
    sourceView->setCurrentIndex(index);

    int productId = sourceModel->productIdAt(index.row());
    emit productSelected(productId);
}

void ProductPane::onAddProduct()
{
    bool ok;
    QString name = QInputDialog::getText(this, "New Product", "Product name:",
                                          QLineEdit::Normal, "", &ok);
    if (ok && !name.trimmed().isEmpty()) {
        m_activeModel->addProduct(name.trimmed());
        loadProducts();

        // Select the newly added product
        int row = m_activeModel->rowCount() - 1;
        if (row >= 0) {
            QModelIndex idx = m_activeModel->index(row);
            onProductClicked(m_activeListView, m_activeModel, idx);
        }
    }
}

void ProductPane::onDeleteProduct()
{
    QListView *view = m_currentListView ? m_currentListView : m_activeListView;
    ProductListModel *model = modelForView(view);
    if (!model) return;

    QModelIndex index = view->currentIndex();
    if (!index.isValid()) return;

    QString name = index.data(Qt::DisplayRole).toString();
    auto result = QMessageBox::question(this, "Delete Product",
        QString("Delete product '%1' and all its tasks?").arg(name),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
        model->removeProduct(index.row());
        loadProducts();
    }
}

void ProductPane::onRenameProduct()
{
    QListView *view = m_currentListView ? m_currentListView : m_activeListView;
    QModelIndex index = view->currentIndex();
    if (index.isValid()) {
        view->edit(index);
    }
}

ProductListModel *ProductPane::modelForView(QListView *view) const
{
    if (view == m_activeListView) return m_activeModel;
    if (view == m_archivedListView) return m_archivedModel;
    return nullptr;
}

void ProductPane::setCurrentListView(QListView *view)
{
    m_currentListView = view;
    if (view == m_activeListView) {
        m_archivedListView->clearSelection();
        m_archivedListView->setCurrentIndex(QModelIndex());
    } else if (view == m_archivedListView) {
        m_activeListView->clearSelection();
        m_activeListView->setCurrentIndex(QModelIndex());
    }
}
