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
    if (index.row() > 0) {
        int prevPriority = index.sibling(index.row() - 1, 0).data(TaskListModel::PriorityRole).toInt();
        int curPriority = index.data(TaskListModel::PriorityRole).toInt();
        if (curPriority != prevPriority) {
            rect.adjust(0, 20, 0, 0);
        }
    }
    QString priorityText = index.data(TaskListModel::PriorityTextRole).toString();
    QFont badgeFont = option.font;
    badgeFont.setPointSize(8);
    QFontMetrics badgeFm(badgeFont);
    int badgeWidth = badgeFm.horizontalAdvance(priorityText) + 12;
    return QRect(rect.right() - badgeWidth - 8, rect.top() + 8, badgeWidth, 18);
}

QRect TaskCardDelegate::workStatusIconRect(const QStyleOptionViewItem &option,
                                            const QModelIndex &index)
{
    QRect rect = option.rect.adjusted(4, 2, -4, -2);
    if (index.row() > 0) {
        bool prevIsSubTask = index.sibling(index.row() - 1, 0).data(TaskListModel::IsSubTaskRole).toBool();
        if (!prevIsSubTask) {
            int prevPriority = index.sibling(index.row() - 1, 0).data(TaskListModel::PriorityRole).toInt();
            int curPriority = index.data(TaskListModel::PriorityRole).toInt();
            if (curPriority != prevPriority) {
                rect.adjust(0, 20, 0, 0);
            }
        }
    }
    return QRect(rect.left() + 4, rect.top() + 4, 28, 28);
}

QRect TaskCardDelegate::expandIconRect(const QStyleOptionViewItem &option,
                                        const QModelIndex &index)
{
    QRect rect = option.rect.adjusted(4, 2, -4, -2);
    if (index.row() > 0) {
        bool prevIsSubTask = index.sibling(index.row() - 1, 0).data(TaskListModel::IsSubTaskRole).toBool();
        if (!prevIsSubTask) {
            int prevPriority = index.sibling(index.row() - 1, 0).data(TaskListModel::PriorityRole).toInt();
            int curPriority = index.data(TaskListModel::PriorityRole).toInt();
            if (curPriority != prevPriority) {
                rect.adjust(0, 20, 0, 0);
            }
        }
    }
    return QRect(rect.left() - 17, rect.top() + rect.height() / 2 - 8, 16, 16);
}

QRect TaskCardDelegate::subtaskCheckboxRect(const QStyleOptionViewItem &option)
{
    QRect rect = option.rect.adjusted(4, 1, -4, -1);
    return QRect(rect.left() + 30, rect.top() + 6, 18, 18);
}

// Draw status icon (left side, no background)
static void drawStatusBadge(QPainter *painter, const QRect &cardRect,
                            TaskWorkStatus ws, const QFont &baseFont)
{
    QString icon = Task::workStatusIcon(ws);

    QFont statusFont = baseFont;
    statusFont.setPointSize(11);
    painter->setFont(statusFont);

    int badgeX = cardRect.left() + 8;
    int badgeY = cardRect.top() + 8;
    QRect iconRect(badgeX, badgeY, 20, 20);

    painter->setPen(QColor("#2c3e50"));
    painter->drawText(iconRect, Qt::AlignCenter, icon);
}

void TaskCardDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // --- SubTask row rendering ---
    bool isSubTask = index.data(TaskListModel::IsSubTaskRole).toBool();
    if (isSubTask) {
        QRect rect = option.rect.adjusted(4, 1, -4, -1);
        bool completed = index.data(TaskListModel::SubTaskCompletedRole).toBool();

        // Background
        if (option.state & QStyle::State_Selected) {
            painter->fillRect(rect, QColor("#dfe6e9"));
        } else if (option.state & QStyle::State_MouseOver) {
            painter->fillRect(rect, QColor("#f0f3f4"));
        } else {
            painter->fillRect(rect, completed ? QColor("#f8f9f9") : QColor("#ffffff"));
        }

        // Indent + checkbox
        int indent = 30;
        QRect checkRect(rect.left() + indent, rect.top() + 6, 18, 18);
        painter->setPen(QPen(completed ? QColor("#27ae60") : QColor("#95a5a6"), 1.5));
        painter->setBrush(completed ? QColor("#27ae60") : Qt::NoBrush);
        painter->drawRoundedRect(checkRect, 3, 3);
        if (completed) {
            painter->setPen(QPen(Qt::white, 2));
            painter->drawLine(checkRect.left() + 4, checkRect.center().y(),
                              checkRect.center().x(), checkRect.bottom() - 4);
            painter->drawLine(checkRect.center().x(), checkRect.bottom() - 4,
                              checkRect.right() - 3, checkRect.top() + 4);
        }

        // Title
        QString title = index.data(TaskListModel::TitleRole).toString();
        QFont titleFont = option.font;
        titleFont.setPointSize(10);
        painter->setFont(titleFont);
        painter->setPen(completed ? QColor("#b0b8bc") : QColor("#2c3e50"));
        QRect titleRect(checkRect.right() + 8, rect.top() + 4, rect.width() - indent - 34, 22);
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                          painter->fontMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));

        painter->restore();
        return;
    }

    // --- Main Task rendering (existing code below) ---
    QRect fullRect = option.rect.adjusted(4, 2, -4, -2);
    QRect rect = fullRect;

    // Priority separator
    int curPriority = index.data(TaskListModel::PriorityRole).toInt();
    if (index.row() > 0) {
        bool prevIsSubTask = index.sibling(index.row() - 1, 0).data(TaskListModel::IsSubTaskRole).toBool();
        if (!prevIsSubTask) {
            int prevPriority = index.sibling(index.row() - 1, 0).data(TaskListModel::PriorityRole).toInt();
            if (curPriority != prevPriority) {
            int sepY = fullRect.top() + 8;
            QColor sepColor("#d5dbdb");
            painter->setPen(QPen(sepColor, 1, Qt::SolidLine));
            painter->drawLine(fullRect.left() + 8, sepY, fullRect.right() - 8, sepY);

            QString label = Task::priorityToString(static_cast<TaskPriority>(curPriority));
            label = label.left(label.indexOf(" -"));
            QFont labelFont = option.font;
            labelFont.setPointSize(7);
            labelFont.setBold(true);
            painter->setFont(labelFont);
            QFontMetrics fm(labelFont);
            int labelW = fm.horizontalAdvance(label) + 8;
            QRect labelBg(fullRect.left() + 12, sepY - 6, labelW, 12);
            painter->fillRect(labelBg, QColor("#ecf0f1"));
            painter->setPen(sepColor);
            painter->drawText(labelBg, Qt::AlignCenter, label);

            rect.adjust(0, 20, 0, 0);
            }
        }
    }

    QColor priorityColor = index.data(TaskListModel::PriorityColorRole).value<QColor>();
    TaskPriority prio = static_cast<TaskPriority>(curPriority);
    QColor bgColor = Task::priorityBackgroundColor(prio);
    QColor barFillColor = Task::priorityBarColor(prio);
    QColor barTrackColor = Task::priorityBarTrackColor(prio);

    TaskWorkStatus workStatus = static_cast<TaskWorkStatus>(
        index.data(TaskListModel::WorkStatusRole).toInt());
    bool isCompleted = (workStatus == TaskWorkStatus::Completed);

    // --- Completed tasks: dimmed style, no progress bar ---
    if (isCompleted) {
        QColor mutedBg;
        if (option.state & QStyle::State_Selected) {
            mutedBg = QColor("#e0e3e5");
            painter->fillRect(rect, mutedBg);
            painter->setPen(QPen(QColor("#b0b8bc"), 1));
            painter->drawRoundedRect(rect, 4, 4);
        } else if (option.state & QStyle::State_MouseOver) {
            painter->fillRect(rect, QColor("#e8eaec"));
        } else {
            painter->fillRect(rect, QColor("#f0f1f2"));
        }

        // Muted left color bar
        QColor mutedBar = priorityColor;
        mutedBar.setAlpha(90);
        painter->fillRect(QRect(rect.left(), rect.top(), 4, rect.height()), mutedBar);

        // Expand/collapse icon for tasks with subtasks (outside card, left margin)
        bool hasSubTasks2 = index.data(TaskListModel::HasSubTasksRole).toBool();
        if (hasSubTasks2) {
            bool expanded2 = index.data(TaskListModel::IsExpandedRole).toBool();
            QFont expandFont2 = option.font;
            expandFont2.setPointSize(9);
            painter->setFont(expandFont2);
            painter->setPen(QColor("#999999"));
            int iconY2 = rect.top() + rect.height() / 2 - 8;
            QRect expandRect2(rect.left() - 17, iconY2, 16, 16);
            painter->drawText(expandRect2, Qt::AlignCenter, expanded2 ? QString::fromUtf8("\xe2\x9e\x96") : QString::fromUtf8("\xe2\x9e\x95"));
        }

        // Status badge
        drawStatusBadge(painter, rect, workStatus, option.font);

        // Title (word wrap, avoid priority badge)
        QString title = index.data(TaskListModel::TitleRole).toString();
        QFont titleFont = option.font;
        titleFont.setPointSize(11);
        titleFont.setBold(true);
        painter->setFont(titleFont);
        painter->setPen(QColor("#95a5a6"));
        // Leave space for status icon (28px) and priority badge on right
        QRect prioBadge = priorityBadgeRect(option, index);
        int titleRight = prioBadge.left() - 4;
        QRect titleRect(rect.left() + 28, rect.top() + 8, titleRight - rect.left() - 28, 20);
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                          painter->fontMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));

        // Priority badge — muted
        QString priorityText = index.data(TaskListModel::PriorityTextRole).toString();
        QRect badgeRect = prioBadge;
        QFont badgeFont = option.font;
        badgeFont.setPointSize(8);
        painter->setFont(badgeFont);
        QColor mutedBadge = priorityColor;
        mutedBadge.setAlpha(100);
        painter->setBrush(mutedBadge);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(badgeRect, 9, 9);
        painter->setPen(QColor(255, 255, 255, 200));
        painter->drawText(badgeRect, Qt::AlignCenter, priorityText);

        // Content preview
        QString content = index.data(TaskListModel::ContentRole).toString();
        content.remove(QRegularExpression("<[^>]*>"));
        content = content.trimmed();
        if (!content.isEmpty()) {
            QFont contentFont = option.font;
            contentFont.setPointSize(9);
            painter->setFont(contentFont);
            painter->setPen(QColor("#b0b8bc"));
            QRect contentRect(rect.left() + 12, rect.top() + 32, rect.width() - 20, 36);
            painter->drawText(contentRect, Qt::AlignLeft | Qt::TextWordWrap,
                              painter->fontMetrics().elidedText(content, Qt::ElideRight,
                                                                contentRect.width() * 2));
        }

        // Dates
        QDateTime createdAt = index.data(TaskListModel::CreatedAtRole).toDateTime();
        QDateTime updatedAt = index.data(TaskListModel::UpdatedAtRole).toDateTime();
        QFont dateFont = option.font;
        dateFont.setPointSize(8);
        painter->setFont(dateFont);
        painter->setPen(QColor("#bdc3c7"));
        QString dateStr;
        if (createdAt.isValid())
            dateStr += QString::fromUtf8("\xF0\x9F\x93\x85 ") + createdAt.toString("MM-dd hh:mm");
        if (updatedAt.isValid())
            dateStr += QString::fromUtf8("  \xE2\x9C\x8F\xEF\xB8\x8F ") + updatedAt.toString("MM-dd hh:mm");
        if (!dateStr.isEmpty()) {
            QRect dateRect(rect.left() + 12, rect.bottom() - 18, rect.width() - 20, 14);
            painter->drawText(dateRect, Qt::AlignLeft | Qt::AlignVCenter, dateStr);
        }

        // Selected indicator
        if (index.data(TaskListModel::IsActiveTaskRole).toBool()) {
            QFont indFont = option.font;
            indFont.setPointSize(12);
            painter->setFont(indFont);
            painter->setPen(QColor("#7f8c8d"));
            QRect indRect(rect.right() - 24, rect.top() + rect.height() / 2 - 10, 20, 20);
            painter->drawText(indRect, Qt::AlignCenter, QString::fromUtf8("\xF0\x9F\x91\x88"));
        }

        painter->restore();
        return;
    }

    // --- Active (non-completed) task rendering ---

    // Background
    if (option.state & QStyle::State_Selected) {
        QColor selBg = bgColor.darker(110);
        painter->fillRect(rect, selBg);
        painter->setPen(QPen(priorityColor, 1.5));
        painter->drawRoundedRect(rect, 4, 4);
    } else if (option.state & QStyle::State_MouseOver) {
        painter->fillRect(rect, bgColor.darker(103));
    } else {
        painter->fillRect(rect, bgColor);
    }

    // Left color bar
    painter->fillRect(QRect(rect.left(), rect.top(), 4, rect.height()), priorityColor);

    // Expand/collapse icon for tasks with subtasks (outside card, left margin)
    bool hasSubTasks = index.data(TaskListModel::HasSubTasksRole).toBool();
    if (hasSubTasks) {
        bool expanded = index.data(TaskListModel::IsExpandedRole).toBool();
        QFont expandFont = option.font;
        expandFont.setPointSize(9);
        painter->setFont(expandFont);
        painter->setPen(QColor("#555555"));
        int iconY = rect.top() + rect.height() / 2 - 8;
        QRect expandRect(rect.left() - 17, iconY, 16, 16);
        painter->drawText(expandRect, Qt::AlignCenter, expanded ? QString::fromUtf8("\xe2\x9e\x96") : QString::fromUtf8("\xe2\x9e\x95"));
    }

    // Status badge
    drawStatusBadge(painter, rect, workStatus, option.font);

    // Title (single line, elided)
    QString title = index.data(TaskListModel::TitleRole).toString();
    QFont titleFont = option.font;
    titleFont.setPointSize(11);
    titleFont.setBold(true);
    painter->setFont(titleFont);
    painter->setPen(QColor("#2c3e50"));
    QRect prioBadge2 = priorityBadgeRect(option, index);
    int titleRight = prioBadge2.left() - 4;
    QRect titleRect(rect.left() + 28, rect.top() + 8, titleRight - rect.left() - 28, 20);
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));

    // Priority badge
    QString priorityText = index.data(TaskListModel::PriorityTextRole).toString();
    QRect badgeRect = prioBadge2;
    QFont badgeFont = option.font;
    badgeFont.setPointSize(8);
    painter->setFont(badgeFont);
    painter->setBrush(priorityColor);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(badgeRect, 9, 9);
    painter->setPen(Qt::white);
    painter->drawText(badgeRect, Qt::AlignCenter, priorityText);

    // Content preview
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

    // Dates
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

        int barY = rect.bottom() - 20;
        int barH = 12;
        int barX = rect.left() + 12;
        int barW = rect.width() - 70;
        QRect trackRect(barX, barY, barW, barH);

        painter->setBrush(barTrackColor);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(trackRect, 6, 6);

        int fillW = static_cast<int>(barW * clampedProgress);
        if (fillW > 0) {
            QRect fillRect(barX, barY, fillW, barH);
            painter->setBrush(barFillColor);
            painter->drawRoundedRect(fillRect, 6, 6);
        }

        // Emoji icon
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
        if (progress > 1.0) {
            painter->setPen(priorityColor);
        } else {
            painter->setPen(barFillColor.darker(120));
        }
        QRect dueTextRect(barX + barW + 4, barY, 54, barH);
        QString dueText = (progress > 1.0) ? "Overdue" : dueDate.toString("MM/dd");
        painter->drawText(dueTextRect, Qt::AlignLeft | Qt::AlignVCenter, dueText);
    }

    // Selected indicator
    if (index.data(TaskListModel::IsActiveTaskRole).toBool()) {
        QFont indFont = option.font;
        indFont.setPointSize(12);
        painter->setFont(indFont);
        painter->setPen(QColor("#2c3e50"));
        QRect indRect(rect.right() - 24, rect.top() + rect.height() / 2 - 10, 20, 20);
        painter->drawText(indRect, Qt::AlignCenter, QString::fromUtf8("\xF0\x9F\x91\x88"));
    }

    painter->restore();
}

QSize TaskCardDelegate::sizeHint(const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    Q_UNUSED(option);

    // SubTask rows are compact
    if (index.data(TaskListModel::IsSubTaskRole).toBool()) {
        return QSize(280, 30);
    }

    TaskWorkStatus ws = static_cast<TaskWorkStatus>(
        index.data(TaskListModel::WorkStatusRole).toInt());
    QDateTime dueDate = index.data(TaskListModel::DueDateRole).toDateTime();

    int baseHeight;
    if (ws == TaskWorkStatus::Completed) {
        baseHeight = 88;
    } else {
        baseHeight = dueDate.isValid() ? 108 : 88;
    }

    if (index.row() > 0) {
        // Only count priority separators between main tasks
        bool prevIsSubTask = index.sibling(index.row() - 1, 0).data(TaskListModel::IsSubTaskRole).toBool();
        if (!prevIsSubTask) {
            int prevPriority = index.sibling(index.row() - 1, 0).data(TaskListModel::PriorityRole).toInt();
            int curPriority = index.data(TaskListModel::PriorityRole).toInt();
            if (curPriority != prevPriority) {
                baseHeight += 20;
            }
        }
    }

    return QSize(280, baseHeight);
}
