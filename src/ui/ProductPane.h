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

signals:
    void productSelected(int productId);

private slots:
    void onProductClicked(const QModelIndex &index);
    void onAddProduct();
    void onDeleteProduct();
    void onRenameProduct();

private:
    void setupUi();
    void setupContextMenu();

    QListView *m_listView;
    ProductListModel *m_model;
    QPushButton *m_addButton;
    QLabel *m_titleLabel;
};
