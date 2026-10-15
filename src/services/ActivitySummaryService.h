#pragma once

#include <QDate>
#include "models/TaskActivity.h"

enum class ActivitySummaryLanguage { English, Chinese };

class ActivitySummaryService
{
public:
    static QDate presetStart(int preset, const QDate &today);
    static bool prepare(const QDate &start, const QDate &endInclusive, ActivityTimeBasis basis,
                        QString &payload, QString &error,
                        ActivitySummaryLanguage language = ActivitySummaryLanguage::English,
                        QString *timestampAppendix = nullptr);
    static constexpr int MaximumPayloadCharacters = 60000;
};
