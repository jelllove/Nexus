# Task Status Markdown Export Design

## Goal

Allow users to export selected tasks into a Markdown summary file so they can share or archive task progress outside the app.

The export must include:

- task work status;
- task title; and
- a short summary extracted from each task note.

## User Experience

### Entry point

- Add a new menu action in **File**:
  - `Export Task Summary (.md)...`

### Export flow

1. User selects a product in the left pane.
2. User opens **File → Export Task Summary (.md)...**.
3. App opens an export dialog that lists **active main tasks only** for the current product.
4. Each task has a checkbox; all are checked by default.
5. User keeps/removes selections and clicks **Export**.
6. App opens a **Save As** dialog for Markdown files.
7. App writes a `.md` file and confirms success.

### Output format

The generated Markdown includes:

1. Header metadata:
   - product name;
   - export timestamp.
2. A table with columns:
   - `Status` (work status text);
   - `Title`;
   - `Summary`.

## Scope and Constraints

### In scope

- Main tasks only (no subtask rows in export).
- Active tasks only as selectable candidates.
- Local summary generation from existing note content (no AI call).
- User-selected save path and filename.

### Out of scope

- Exporting archived/deleted tasks.
- Exporting subtasks.
- AI-generated summaries during export.
- New database schema/migrations.

## Architecture

### MainWindow integration

- Add a new `QAction` in `MainWindow::setupMenuBar()`.
- Add a corresponding slot (for example `onExportTaskSummaryRequested()`).
- Slot responsibilities:
  - validate selected product context;
  - fetch export candidates from database;
  - launch export selection dialog;
  - show Save As dialog;
  - write Markdown file;
  - show success/failure feedback.

### Export dialog

Add a dedicated dialog component (for example `ExportTaskSummaryDialog`) that:

- receives active tasks for current product;
- renders a checkable list;
- preselects all tasks;
- returns selected task IDs on accept.

This keeps selection behavior isolated and avoids coupling export behavior to TaskPane interaction state.

### Markdown generation helper

Add a focused helper (pure Qt/C++) to build Markdown from selected tasks.

Responsibilities:

- convert `TaskWorkStatus` to display text via existing `Task::workStatusToString`;
- strip HTML tags from task note content;
- normalize whitespace;
- truncate to ~120 characters;
- escape Markdown table-sensitive characters as needed;
- produce deterministic Markdown table output.

No persistence or UI dependencies should live in this helper.

## Data Flow

1. Trigger export action from File menu.
2. Validate selected product:
   - if not selected, show info message and stop.
3. Read active tasks:
   - `DatabaseManager::getTasksForProduct(productId, TaskStatus::Active)`.
   - if empty, show info message and stop.
4. Show export dialog with checkboxes (all checked by default).
5. If canceled or no tasks selected:
   - do not write file;
   - show concise info message.
6. Build Markdown with selected tasks.
7. Open Save As dialog (`*.md`).
8. Write UTF-8 file via `QFile` + `QTextStream`.
9. On success, show status confirmation.

## Error Handling

- No product selected → information dialog.
- No active tasks available → information dialog.
- Export dialog accepted with zero selected tasks → information dialog.
- Save canceled → no file write and show a short status-bar message.
- File open/write failure → warning dialog with path and write error.

Do not silently swallow write failures.

## Testing Strategy (TDD)

Add focused tests before implementation:

1. Markdown helper tests
   - HTML is stripped from notes.
   - Whitespace is normalized.
   - Summary truncates to configured short length (~120 chars).
   - Work status text is correct.
   - Table escaping handles problematic characters.

2. Export flow logic tests (targeted)
   - no selected product path;
   - empty active-task list path;
   - canceled/empty selection path;
   - successful output format with header + table rows.

Use the repository’s existing Qt Test setup and keep tests narrow to changed behavior.

## Compatibility

- No database schema change.
- No changes to existing task editing, search, AI, or subtask flows.
- Existing installer/build workflow remains unchanged.
