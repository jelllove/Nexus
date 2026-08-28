#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QTimer>

class SearchBar : public QWidget
{
    Q_OBJECT

public:
    explicit SearchBar(QWidget *parent = nullptr);

    QString searchText() const;
    void clear();

signals:
    void searchRequested(const QString &query);
    void searchCleared();

private:
    QLineEdit *m_searchInput;
    QPushButton *m_clearButton;
    QTimer *m_searchDebounceTimer;
};
