#pragma once

#include "models/TaskCreation.h"

struct TaskCreationSuggestion {
    TaskCreationDraft draft;
    QString reason;
    bool confident = false;
};

class TaskCreationService
{
public:
    static constexpr int MaximumInputCharacters = 8000;
    static constexpr int MaximumPayloadCharacters = 64000;
    static constexpr int MaximumNoteExcerptCharacters = 480;

    static bool buildPayload(const QString &input, const TaskCreationCatalog &catalog,
                             QString &payload, QString &error);
    static bool parseSuggestion(const QString &response, const TaskCreationCatalog &catalog,
                                TaskCreationSuggestion &suggestion, QString &error);
};
