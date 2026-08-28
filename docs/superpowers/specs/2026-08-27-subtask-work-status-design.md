# Sub Task Multi-Status (Small Icon) Design

## Goal

Enable each sub task to use the same 5 work statuses as main tasks:

- Not Started
- Ongoing
- Paused
- Completed
- Waiting

Show these statuses with a smaller icon on sub task rows, and allow changing status by clicking the sub task status icon.

## Confirmed Product Decisions

1. Sub task status set is exactly the same as main task status set.
2. Interaction is icon-click -> popup menu (same style as main task).
3. Existing sub task checkbox is replaced by a small status icon.
4. Existing data migration rule:
   - `completed = 1` -> `work_status = Completed (3)`
   - `completed = 0` -> `work_status = Not Started (0)`
5. Export/preview completed-state logic for sub tasks uses `work_status == Completed`.

## Current State (Before Change)

- `tasks` already supports `work_status`.
- `subtasks` currently stores `completed` but not `work_status`.
- Sub task UI renders checkbox + title.
- Main task supports status icon click popup and right-click status menu.

## Proposed Architecture

### 1) Data Model + Migration

- Add `work_status INTEGER DEFAULT 0` to `subtasks`.
- Keep legacy `completed` column for compatibility.
- Migration updates `work_status` from existing `completed` values only when schema is upgraded.

Behavior contract:

- Source of truth for sub task status becomes `subtasks.work_status`.
- `completed` is kept aligned where practical to avoid legacy breakage:
  - `work_status == Completed` => `completed = 1`
  - otherwise => `completed = 0`

### 2) Domain Model

- Extend `SubTask` model with `TaskWorkStatus workStatus`.
- Existing `completed` stays in struct for compatibility code paths, but UI/logic shifts to `workStatus`.

### 3) Database API

Add/update sub task APIs:

- Read `work_status` in `getSubtask` / `getSubtasks`.
- Initialize new subtasks with `work_status = NotStarted`.
- Add `updateSubtaskWorkStatus(subtaskId, TaskWorkStatus)` method.
- Keep `toggleSubtask` but make it map to work status:
  - checked/true -> Completed
  - unchecked/false -> NotStarted

### 4) View Model (TaskListModel)

- Add sub task roles:
  - `SubTaskWorkStatusRole`
  - `SubTaskWorkStatusIconRole`
- Existing `SubTaskCompletedRole` should derive from `work_status == Completed`.

### 5) UI Rendering + Interaction

#### TaskCardDelegate

- Replace sub task checkbox drawing with small status icon drawing.
- Add sub task status hit area helper (icon rectangle) for click detection.

#### TaskPane Click Behavior

- If clicked row is sub task:
  - clicking sub task status icon opens sub task status popup menu
  - clicking other sub task area selects/open editor for sub task content (keep current behavior)

#### TaskPane Context Menu

- Replace sub task `Mark Complete/Incomplete` action with `Set Status` submenu (5 statuses).
- Keep rename/delete actions unchanged.

### 6) Export/Preview Alignment

- Sub task completion visualization in markdown/export flow should use `work_status == Completed`.
- No structural format change needed beyond completion-state source.

## Error Handling

- Migration failure must fail initialization with explicit log, matching existing migration behavior.
- DB update failures return false and keep existing warning/error patterns; no silent fallback.

## Validation Plan

1. Open DB with old subtasks:
   - verify migrated statuses match completed mapping.
2. Add new sub task:
   - default status is Not Started.
3. In task list:
   - small icon is visible on each sub task row.
   - clicking icon opens status menu and persists selection.
4. Click sub task title area:
   - editor loads sub task content (no regression).
5. Main task status interactions still behave unchanged.
6. Export/preview:
   - completed marker for sub task follows `work_status == Completed`.

## Non-Goals

- No new custom status set for subtasks.
- No redesign of main task status icons.
- No bulk status edit workflow in this iteration.
