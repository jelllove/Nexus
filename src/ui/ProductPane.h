#pragma once

#include <QWidget>
#include <QListView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include "models/ProductListModel.h"

class ProductPane : public QWidget
{
    Q_OBJECT

public:
    explicit ProductPane(QWidget *parent = nullptr);

    void loadProducts();
    int selectedProductId() const;
    bool selectProductById(int productId, bool emitSelection = true);

signals:
    void productSelected(int productId);

private slots:
    void onAddProduct();
    void onDeleteProduct();
    void onRenameProduct();

private:
    void setupUi();
    void setupContextMenu();
    void onProductClicked(QListView *sourceView, ProductListModel *sourceModel, const QModelIndex &index);
    ProductListModel *modelForView(QListView *view) const;
    void setCurrentListView(QListView *view);

    QListView *m_activeListView;
    QListView *m_archivedListView;
    ProductListModel *m_activeModel;
    ProductListModel *m_archivedModel;
    QListView *m_currentListView = nullptr;
    QWidget *m_archivedSection;
    QPushButton *m_showArchivedButton;
    QPushButton *m_addButton;
    QLabel *m_titleLabel;
};
