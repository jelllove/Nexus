#include "SearchBar.h"

SearchBar::SearchBar(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);

    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText("Search tasks...");
    m_searchInput->setClearButtonEnabled(true);
    m_searchInput->setStyleSheet(
        "QLineEdit { padding: 6px 10px; border: 1px solid #bdc3c7; "
        "border-radius: 16px; font-size: 12px; background-color: white; }");
    layout->addWidget(m_searchInput, 1);

    m_clearButton = new QPushButton("Clear", this);
    m_clearButton->setStyleSheet(
        "QPushButton { background-color: #95a5a6; color: white; border: none; "
        "padding: 6px 12px; border-radius: 12px; font-size: 11px; }"
        "QPushButton:hover { background-color: #7f8c8d; }");
    m_clearButton->setVisible(false);
    layout->addWidget(m_clearButton);

    connect(m_searchInput, &QLineEdit::returnPressed, this, [this]() {
        QString text = m_searchInput->text().trimmed();
        if (!text.isEmpty()) {
            m_clearButton->setVisible(true);
            emit searchRequested(text);
        }
    });

    connect(m_clearButton, &QPushButton::clicked, this, [this]() {
        m_searchInput->clear();
        m_clearButton->setVisible(false);
        emit searchCleared();
    });

    connect(m_searchInput, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (text.isEmpty()) {
            m_clearButton->setVisible(false);
            emit searchCleared();
        }
    });
}

QString SearchBar::searchText() const
{
    return m_searchInput->text().trimmed();
}

void SearchBar::clear()
{
    m_searchInput->clear();
    m_clearButton->setVisible(false);
}
