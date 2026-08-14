#pragma once

#include <QMetaType>

enum class EditorTargetKind {
    None,
    Task,
    SubTask
};

struct EditorTarget {
    EditorTargetKind kind = EditorTargetKind::None;
    int id = -1;

    static EditorTarget task(int taskId) { return {EditorTargetKind::Task, taskId}; }
    static EditorTarget subtask(int subtaskId) { return {EditorTargetKind::SubTask, subtaskId}; }

    bool isValid() const { return kind != EditorTargetKind::None && id > 0; }

    friend bool operator==(const EditorTarget &left, const EditorTarget &right)
    {
        return left.kind == right.kind && left.id == right.id;
    }

    friend bool operator!=(const EditorTarget &left, const EditorTarget &right)
    {
        return !(left == right);
    }
};

Q_DECLARE_METATYPE(EditorTarget)
