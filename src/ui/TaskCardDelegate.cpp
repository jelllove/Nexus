#include "TaskCardDelegate.h"
#include "models/TaskListModel.h"
#include <QApplication>
#include <QRegularExpression>
#include <QDateTime>
#include <algorithm>

TaskCardDelegate::TaskCardDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QRect TaskCardDelegate::priorityBadgeRect(const QStyleOptionViewItem &option,
                                           const QModelIndex &index)
{
    QRect rect = option.rect.adjusted(4, 2, -4, -2);
    QString priorityText = index.data(TaskListModel::PriorityTextRole).toString();
    QFont badgeFont = option.font;
    badgeFont.setPointSize(8);
    QFontMetrics badgeFm(badgeFont);
    int badgeWidth = badgeFm.horizontalAdvance(priorityText) + 12;
    return QRect(rect.right() - badgeWidth - 8, rect.top() + 8, badgeWidth, 18);
}

void TaskCardDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QRect rect = option.rect.adjusted(4, 2, -4, -2);

    // Background
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(rect, QColor("#d5e8f0"));
        painter->setPen(QPen(QColor("#2980b9"), 1));
        painter->drawRoundedRect(rect, 4, 4);
    } else if (option.state & QStyle::State_MouseOver) {
        painter->fillRect(rect, QColor("#eaf2f8"));
    } else {
        painter->fillRect(rect, Qt::white);
    }

    // Priority color bar on the left
    QColor priorityColor = index.data(TaskListModel::PriorityColorRole).value<QColor>();
    QRect colorBar(rect.left(), rect.top(), 4, rect.height());
    painter->fillRect(colorBar, priorityColor);

    // Title
    QString title = index.data(TaskListModel::TitleRole).toString();
    QFont titleFont = option.font;
    titleFont.setPointSize(11);
    titleFont.setBold(true);
    painter->setFont(titleFont);
    painter->setPen(QColor("#2c3e50"));
    QRect titleRect(rect.left() + 12, rect.top() + 8, rect.width() - 20, 20);
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));

    // Priority badge (using shared rect calculation)
    QString priorityText = index.data(TaskListModel::PriorityTextRole).toString();
    QRect badgeRect = priorityBadgeRect(option, index);
    QFont badgeFont = option.font;
    badgeFont.setPointSize(8);
    painter->setFont(badgeFont);
    painter->setBrush(priorityColor);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(badgeRect, 9, 9);
    painter->setPen(Qt::white);
    painter->drawText(badgeRect, Qt::AlignCenter, priorityText);

    // Content preview (first 2 lines)
    QString content = index.data(TaskListModel::ContentRole).toString();
    // Strip HTML tags for preview
    content.remove(QRegularExpression("<[^>]*>"));
    content = content.trimmed();
    if (!content.isEmpty()) {
        QFont contentFont = option.font;
        contentFont.setPointSize(9);
        painter->setFont(contentFont);
        painter->setPen(QColor("#7f8c8d"));
        QRect contentRect(rect.left() + 12, rect.top() + 32, rect.width() - 20, 36);
        painter->drawText(contentRect, Qt::AlignLeft | Qt::TextWordWrap,
                          painter->fontMetrics().elidedText(content, Qt::ElideRight,
                                                            contentRect.width() * 2));
    }

    // Created & Modified dates (single line)
    QDateTime createdAt = index.data(TaskListModel::CreatedAtRole).toDateTime();
    QDateTime updatedAt = index.data(TaskListModel::UpdatedAtRole).toDateTime();
    QFont dateFont = option.font;
    dateFont.setPointSize(8);
    painter->setFont(dateFont);
    painter->setPen(QColor("#bdc3c7"));
    QDateTime dueDateCheck = index.data(TaskListModel::DueDateRole).toDateTime();
    int dateY = dueDateCheck.isValid() ? rect.bottom() - 38 : rect.bottom() - 18;
    QString dateStr;
    if (createdAt.isValid())
        dateStr += QString::fromUtf8("\xF0\x9F\x93\x85 ") + createdAt.toString("MM-dd hh:mm");
    if (updatedAt.isValid())
        dateStr += QString::fromUtf8("  \xE2\x9C\x8F\xEF\xB8\x8F ") + updatedAt.toString("MM-dd hh:mm");
    if (!dateStr.isEmpty()) {
        QRect dateRect(rect.left() + 12, dateY, rect.width() - 20, 14);
        painter->drawText(dateRect, Qt::AlignLeft | Qt::AlignVCenter, dateStr);
    }

    // Due date progress bar
    QDateTime dueDate = index.data(TaskListModel::DueDateRole).toDateTime();
    if (dueDate.isValid()) {
        QDateTime createdAt2 = index.data(TaskListModel::CreatedAtRole).toDateTime();
        QDate createDay = createdAt2.date();
        QDate dueDay = dueDate.date();
        QDate today = QDate::currentDate();

        // Day-based progress: e.g. created 14th, due 18th = 4 days total, today 15th = 1 day elapsed
        qint64 totalDays = createDay.daysTo(dueDay);
        qint64 elapsedDays = createDay.daysTo(today);
        double progress = (totalDays > 0) ? (static_cast<double>(elapsedDays) / totalDays) : 1.0;
        double clampedProgress = std::clamp(progress, 0.0, 1.0);

        // Progress bar geometry
        int barY = rect.bottom() - 20;
        int barH = 12;
        int barX = rect.left() + 12;
        int barW = rect.width() - 70;  // Leave room for text
        QRect trackRect(barX, barY, barW, barH);

        // Background track
        painter->setBrush(QColor("#ecf0f1"));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(trackRect, 6, 6);

        // Fill bar with color based on progress
        QColor barColor;
        if (progress > 1.0) {
            barColor = QColor("#e74c3c");  // Red - overdue
        } else if (progress > 0.8) {
            barColor = QColor("#e74c3c");  // Red
        } else if (progress > 0.5) {
            barColor = QColor("#e67e22");  // Orange
        } else {
            barColor = QColor("#27ae60");  // Green
        }

        int fillW = static_cast<int>(barW * clampedProgress);
        if (fillW > 0) {
            QRect fillRect(barX, barY, fillW, barH);
            painter->setBrush(barColor);
            painter->drawRoundedRect(fillRect, 6, 6);
        }

        // Emoji icon at progress position
        QString icon = index.data(TaskListModel::DueDateIconRole).toString();
        QFont iconFont = option.font;
        iconFont.setPointSize(9);
        painter->setFont(iconFont);
        painter->setPen(QColor("#2c3e50"));
        int iconX = barX + static_cast<int>(barW * clampedProgress) - 6;
        iconX = std::clamp(iconX, barX, barX + barW - 12);
        painter->drawText(QRect(iconX, barY - 2, 16, 16), Qt::AlignCenter, icon);

        // Due date text
        QFont dueFont = option.font;
        dueFont.setPointSize(8);
        dueFont.setBold(true);
        painter->setFont(dueFont);
        painter->setPen(barColor);
        QRect dueTextRect(barX + barW + 4, barY, 54, barH);
        QString dueText = (progress > 1.0) ? "Overdue" : dueDate.toString("MM/dd");
        painter->drawText(dueTextRect, Qt::AlignLeft | Qt::AlignVCenter, dueText);
    }

    painter->restore();
}

QSize TaskCardDelegate::sizeHint(const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    Q_UNUSED(option);
    QDateTime dueDate = index.data(TaskListModel::DueDateRole).toDateTime();
    return QSize(280, dueDate.isValid() ? 108 : 88);
}
