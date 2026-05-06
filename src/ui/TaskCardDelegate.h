#pragma once

#include <QStyledItemDelegate>
#include <QPainter>

class TaskCardDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit TaskCardDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    static QRect priorityBadgeRect(const QStyleOptionViewItem &option,
                                   const QModelIndex &index);
    static QRect workStatusIconRect(const QStyleOptionViewItem &option,
                                    const QModelIndex &index);
};
