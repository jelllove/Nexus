# Task Completion Visibility — Design

Status: Implemented and locally validated on 2026-10-20.

## Goal

Let users hide completed work in the existing Tasks pane and show it again
with one checkbox. Done means `TaskWorkStatus::Completed`, independently of
the Active, Archived and Deleted lifecycle states.

## Interaction

- Add a compact, left-aligned **Show completed** checkbox on its own row
  below the Active / Archived / Deleted buttons and above the task cards.
  Use the existing pane colors and native checkbox interaction.
- Checked shows all work statuses. Unchecked hides Completed items and keeps
  Not Started, Ongoing, Paused and Waiting visible.
- Default to checked for users without a saved preference, preserving the
  existing view. Remember the preference across products and application restarts
  using the existing database settings API (`task_list_show_completed`).
- Apply the same preference to Active, Archived, Deleted and Search Results
  views. Clearing search restores the previous view with the same preference.
- Filtering changes visibility only. It does not archive, delete, complete or
  otherwise update a task or subtask. Export/preview and activity-summary
  selection continue to use their existing independent rules.

## Main Tasks and Subtasks

Recommended scope includes both levels:

- A completed main task is hidden together with its entire subtree, including
  any unfinished subtasks. Parent status is authoritative; no automatic
  completion of children or calculation of parent status is introduced.
- Under a visible main task, hide only subtasks whose own work status is
  Completed. Preserve expansion state when toggling the checkbox.
- A main task with only completed subtasks stays visible. Its expansion
  indicator should reflect visible children, avoiding an apparently empty
  expandable group while filtering is enabled.
- With completed items hidden, successfully marking a visible item Completed
  immediately removes it from the list. Checking Show completed reveals it
  again in its normal order. A failed status write leaves the list unchanged
  and reports the failure.

## Selection, Editing and Feedback

- Preserve the selected task/subtask by stable ID when it remains visible;
  avoid reloading its editor or disturbing its cursor.
- When filtering would hide the open note, save pending editor content before
  changing visibility, then clear the list selection and editor. Do not
  automatically open an unrelated note. If saving fails, keep the prior
  checkbox, selection and editor state and report the error. Status changes
  that would hide the note use the same save-before-change path.
- If results exist before filtering but none remain visible, show
  **All matching tasks are completed. Check Show completed to see them.**
  An actually empty product/search retains its existing empty state.
- Search feedback reports visible main-task results and the number hidden by
  the filter, so a nonzero result count cannot accompany an unexplained empty
  list. Subtasks are not counted as independent search hits.
- If saving the preference fails, keep the current visibility for the session
  and report that the preference could not be remembered.

## Implementation Boundaries

- `TaskPane`: owns the checkbox, empty-state feedback and restoring selection
  by task/subtask ID. It supplies the filter preference on every load/refresh
  path through the retained model setting, including search, lifecycle switches
  and reveal-after-move actions.
- `TaskListModel`: keeps the complete loaded task collection and filters when
  rebuilding display rows, including expanded subtask rows. Add one explicit
  `showCompleted` property rather than wrapping the existing model in a proxy;
  current drag/drop and ID lookup paths depend directly on this model.
- `MainWindow` / `EditorPane`: coordinate save-before-hide and editor clearing
  through the existing `saveCurrentContent()` contract.
- `DatabaseManager`: reuse settings storage without a schema migration. Keep
  ordinary task/search queries complete so the UI filter does not change
  exports, summaries or other data consumers.
- Main-task reorder must use the complete priority group, moving visible IDs
  through visible slots while preserving hidden tasks' relative order/slots.
  Never persist a partial group with fresh contiguous sort orders. Subtask
  drag/drop positions must map visible rows to the complete sibling order.
- Reparenting into a hidden destination must reveal it predictably: explicitly
  enable and persist Show completed before following the moved subtask. Apply
  the same rule if the moved subtask itself is completed. If that preference
  cannot persist, still reveal it for the session and report the failure.

## Alternatives

1. **Show completed** checkbox: recommended; the checked state directly names
   what is visible and can default to the existing behavior.
2. **Hide completed** checkbox: equivalent filtering, but checked means
   removing content and adds a negative condition to the interaction.
3. Status dropdown: supports more filters but adds unnecessary scope for this
   binary requirement.

## Verification Plan

- Model regressions: all five work statuses, mixed subtask statuses, hidden
  parent/unfinished child, visible parent/completed children, all-hidden and
  genuinely empty results, expansion preservation, lifecycle/search reloads.
- UI regressions: checkbox default, persisted preference, product/view changes,
  search count and restore, selection by ID, immediate completion filtering,
  pending editor saves and simulated save/status/preference failures.
- Movement regressions: main-task reorder with hidden same-priority tasks,
  subtask reorder across hidden siblings, dragging to another visible parent,
  and destination-picker moves into completed parents/with completed children.
- Verify filtering itself does not write task/subtask records or alter export
  and summary inputs. Use in-memory or temporary databases only.
- The Windows validation helper built all targets, passed all 10 CTest suites
  and passed editor syntax checks. The core suite passed 33 cases; the
  UI/movement suite passed 60 cases, with 2 existing native mouse cases skipped
  under offscreen Qt. Native desktop mouse interaction was not run.

## Decision Record

The user approved proceeding with implementation on 2026-10-20, including the
initial checked state, shared preference, subtask scope, all-view scope and
reveal-after-move behavior described above.
