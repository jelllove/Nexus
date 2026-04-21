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
    // Account for separator space
    if (index.row() > 0) {
        int prevPriority = index.sibling(index.row() - 1, 0).data(TaskListModel::PriorityRole).toInt();
        int curPriority = index.data(TaskListModel::PriorityRole).toInt();
        if (curPriority != prevPriority) {
            rect.adjust(0, 20, 0, 0);  // separator height
        }
    }
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

    QRect fullRect = option.rect.adjusted(4, 2, -4, -2);
    QRect rect = fullRect;

    // Priority separator line between different priority groups
    int curPriority = index.data(TaskListModel::PriorityRole).toInt();
    if (index.row() > 0) {
        int prevPriority = index.sibling(index.row() - 1, 0).data(TaskListModel::PriorityRole).toInt();
        if (curPriority != prevPriority) {
            int sepY = fullRect.top() + 8;
            QColor sepColor("#d5dbdb");

            // Draw separator line
            painter->setPen(QPen(sepColor, 1, Qt::SolidLine));
            painter->drawLine(fullRect.left() + 8, sepY, fullRect.right() - 8, sepY);

            // Draw priority group label
            QString label = Task::priorityToString(static_cast<TaskPriority>(curPriority));
            label = label.left(label.indexOf(" -"));  // Just "P0", "P1", etc.
            QFont labelFont = option.font;
            labelFont.setPointSize(7);
            labelFont.setBold(true);
            painter->setFont(labelFont);
            QFontMetrics fm(labelFont);
            int labelW = fm.horizontalAdvance(label) + 8;
            QRect labelBg(fullRect.left() + 12, sepY - 6, labelW, 12);

            // Background to cover the line
            painter->fillRect(labelBg, QColor("#ecf0f1"));
            painter->setPen(sepColor);
            painter->drawText(labelBg, Qt::AlignCenter, label);

            // Shift card rect below separator
            rect.adjust(0, 20, 0, 0);
        }
    }

    // Get priority info for coordinated colors
    QColor priorityColor = index.data(TaskListModel::PriorityColorRole).value<QColor>();
    TaskPriority prio = static_cast<TaskPriority>(curPriority);
    QColor bgColor = Task::priorityBackgroundColor(prio);
    QColor barFillColor = Task::priorityBarColor(prio);
    QColor barTrackColor = Task::priorityBarTrackColor(prio);

    // Background — priority-tinted
    if (option.state & QStyle::State_Selected) {
        // Selected: slightly more saturated version of priority bg
        QColor selBg = bgColor.darker(110);
        painter->fillRect(rect, selBg);
        painter->setPen(QPen(priorityColor, 1.5));
        painter->drawRoundedRect(rect, 4, 4);
    } else if (option.state & QStyle::State_MouseOver) {
        painter->fillRect(rect, bgColor.darker(103));
    } else {
        painter->fillRect(rect, bgColor);
    }

    // Priority color bar on the left
    QRect colorBar(rect.left(), rect.top(), 4, rect.height());
    painter->fillRect(colorBar, priorityColor);

    // Title — with ✅ prefix if completed
    QString title = index.data(TaskListModel::TitleRole).toString();
    bool isCompleted = index.data(TaskListModel::CompletedRole).toBool();
    if (isCompleted) {
        title = QString::fromUtf8("\xE2\x9C\x85 ") + title;   // ✅
    }
    QFont titleFont = option.font;
    titleFont.setPointSize(11);
    titleFont.setBold(true);
    painter->setFont(titleFont);
    painter->setPen(QColor("#2c3e50"));
    QRect titleRect(rect.left() + 12, rect.top() + 8, rect.width() - 20, 20);
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));

    // Priority badge
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

    // Created & Modified dates
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

        qint64 totalDays = createDay.daysTo(dueDay);
        qint64 elapsedDays = createDay.daysTo(today);
        double progress = (totalDays > 0) ? (static_cast<double>(elapsedDays) / totalDays) : 1.0;
        double clampedProgress = std::clamp(progress, 0.0, 1.0);

        // Progress bar geometry
        int barY = rect.bottom() - 20;
        int barH = 12;
        int barX = rect.left() + 12;
        int barW = rect.width() - 70;
        QRect trackRect(barX, barY, barW, barH);

        // Background track — priority-coordinated
        painter->setBrush(barTrackColor);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(trackRect, 6, 6);

        // Fill bar — priority-coordinated (same color regardless of progress)
        int fillW = static_cast<int>(barW * clampedProgress);
        if (fillW > 0) {
            QRect fillRect(barX, barY, fillW, barH);
            painter->setBrush(barFillColor);
            painter->drawRoundedRect(fillRect, 6, 6);
        }

        // Emoji icon with urgency suffix
        QString icon = index.data(TaskListModel::DueDateIconRole).toString();
        QString urgencySuffix;
        if (progress > 1.0) {
            urgencySuffix = QString::fromUtf8("\xF0\x9F\x94\xA5");   // 🔥 overdue
        } else if (progress > 0.8) {
            urgencySuffix = QString::fromUtf8("\xF0\x9F\x94\xA5");   // 🔥 urgent
        } else if (progress > 0.5) {
            urgencySuffix = QString::fromUtf8("\xF0\x9F\x92\xA8");   // 💨 rushing
        }

        QFont iconFont = option.font;
        iconFont.setPointSize(9);
        painter->setFont(iconFont);
        painter->setPen(QColor("#2c3e50"));
        int iconX = barX + static_cast<int>(barW * clampedProgress) - 6;
        iconX = std::clamp(iconX, barX, barX + barW - 12);
        QString iconStr = icon + urgencySuffix;
        int iconW = urgencySuffix.isEmpty() ? 16 : 28;
        painter->drawText(QRect(iconX, barY - 2, iconW, 16), Qt::AlignLeft | Qt::AlignVCenter, iconStr);

        // Due date text — priority-coordinated color
        QFont dueFont = option.font;
        dueFont.setPointSize(8);
        dueFont.setBold(true);
        painter->setFont(dueFont);
        if (progress > 1.0) {
            painter->setPen(priorityColor);  // Overdue uses the strong priority color
        } else {
            painter->setPen(barFillColor.darker(120));
        }
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
    int baseHeight = dueDate.isValid() ? 108 : 88;

    // Add separator space if priority changes
    if (index.row() > 0) {
        int prevPriority = index.sibling(index.row() - 1, 0).data(TaskListModel::PriorityRole).toInt();
        int curPriority = index.data(TaskListModel::PriorityRole).toInt();
        if (curPriority != prevPriority) {
            baseHeight += 20;
        }
    }

    return QSize(280, baseHeight);
}
