#pragma once

#include "models/EditorTarget.h"
#include "models/Task.h"

struct SearchResult {
    Task parentTask;
    SubTask matchedSubtask;
    bool isSubtaskMatch = false;

    EditorTarget target() const
    {
        return isSubtaskMatch
            ? EditorTarget::subtask(matchedSubtask.id)
            : EditorTarget::task(parentTask.id);
    }
};
