#pragma once

#include <QDate>
#include "models/TaskActivity.h"

class ActivitySummaryService
{
public:
    static QDate presetStart(int preset, const QDate &today);
    static bool prepare(const QDate &start, const QDate &endInclusive, ActivityTimeBasis basis,
                        QString &payload, QString &error);
    static constexpr int MaximumPayloadCharacters = 60000;
};
