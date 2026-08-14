# Subtask Notes Design

## Goal

Give every subtask its own rich-text note while preserving the distinction between parent tasks and their child subtasks. Selecting either layer opens that item's title and content in the existing right-hand editor.

## User Experience

- Clicking a main task loads the main task in the existing editor.
- Clicking an expanded subtask loads that subtask in the same editor.
- The selected subtask remains visibly selected and its parent remains expanded.
- The editor supports the same capabilities for both layers:
  - rich-text formatting;
  - pasted and inserted images;
  - three-second autosave;
  - title editing;
  - AI title generation; and
  - AI summarization.
- AI title generation renames the selected item at its own layer.
- AI summaries are appended to the selected item's note.
- Subtask titles use 11-point regular text, increased from the current 10-point text while remaining visually subordinate to bold main-task titles.

## Architecture

### Typed editor target

`EditorPane` will use an explicit editing target containing an item kind and ID rather than treating every selected ID as a main-task ID. The supported kinds are:

- main task;
- subtask; and
- no selection.

Loading, title updates, autosave, history snapshots, image insertion, and AI requests will carry this target. This keeps one editor implementation while preventing a main-task and subtask with the same numeric ID from being confused.

`MainWindow` will route editor and AI results according to the target kind. Selecting a product or clearing a selection resets the target.

### Data model and persistence

The `subtasks` table will gain:

- `content TEXT NOT NULL DEFAULT ''`;
- `updated_at DATETIME DEFAULT CURRENT_TIMESTAMP`.

The `SubTask` model and database queries will expose these fields. `DatabaseManager` will provide explicit operations to:

- fetch one subtask;
- update its title;
- update its content; and
- save and retrieve subtask content snapshots.

Subtask history will be stored separately from main-task history so IDs cannot collide and cascading deletion remains straightforward.

### Migration

A new database version will add the subtask columns and history storage without changing existing subtask IDs, titles, completion state, or ordering. Fresh databases will be created with the final schema.

Each migration statement must be checked. Initialization must report a failed migration instead of continuing with a partially upgraded database.

## Selection and Task List Behavior

`TaskPane` will emit a layer-aware selection for both main tasks and subtasks. `TaskListModel` already represents subtasks as distinct display rows; it will expose the data needed to load their editor target and maintain selected-row state.

The existing checkbox interaction remains independent from note selection:

- clicking the checkbox toggles completion;
- clicking the rest of the row selects and opens the subtask;
- context-menu rename and delete continue to work.

Deleting a parent continues to cascade to its subtasks, notes, histories, and search data. Deleting a subtask removes only that subtask and its dependent records.

## Search

Global search will cover:

- main-task titles and content;
- subtask titles and content.

Subtasks will use a dedicated FTS5 index and synchronization triggers, with a `LIKE` fallback when FTS5 is unavailable. Search results will retain item kind, item ID, and parent-task context rather than flattening all IDs into one namespace.

The task pane will display actual search results. A matching subtask will be shown with its parent context; selecting it expands the parent if necessary and opens the subtask in the editor. Clearing search restores the selected product's normal task list.

## AI and Autosave Flow

1. The editor emits its typed target and current content.
2. `MainWindow` starts the AI request and retains that target for the response.
3. A generated title updates the target's title.
4. A generated summary is appended to the target's content.
5. The selected row and editor are refreshed without switching layers.

Autosave follows the same target-aware routing. Switching between items flushes a pending save before replacing the target. Save or migration failures must be surfaced through the existing status or message UI and must not appear successful.

## Compatibility

- Existing main-task behavior and storage remain unchanged.
- Existing databases are migrated in place.
- Existing subtasks receive empty notes initially.
- Completion, ordering, archive, deleted-task, image, tray, and update behavior remain unchanged.
- The feature adds no new runtime dependency.

## Validation

Validation will cover:

- migration of a database containing existing subtasks;
- creation of a fresh database with the final schema;
- independent parent-task and subtask title/content updates, including overlapping numeric IDs;
- autosave flushing when switching between layers;
- subtask history isolation and cascading cleanup;
- AI title and summary routing to the selected layer;
- FTS5 and fallback search for subtask titles and notes;
- opening a subtask from search results;
- checkbox clicks not unintentionally opening or editing the subtask; and
- successful editor-bundle and Release application builds using the repository's existing build commands.

No unrelated UI redesign or hierarchy expansion beyond one subtask layer is included.
