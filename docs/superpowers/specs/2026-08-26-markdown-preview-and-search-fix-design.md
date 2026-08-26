# Markdown Preview Window and Search Behavior Fix Design

Date: 2026-08-26  
Scope: Keep existing Markdown export capability, add a new non-export Markdown preview workflow, and make Search actually render global results (Active + Archived + Deleted) with proper view restoration on clear.

## 1. Problem Statement

Two user-facing gaps remain:

1. Export is file-oriented only. Users also need a fast read/inspect path that reuses the same task-selection flow but opens a window instead of writing a file.
2. Search currently executes a query and shows result counts in status bar, but does not render results into the task list.

Additionally, preview UX requires explicit copy format choices (Markdown and HTML), and preview must be non-modal so the main window stays operable.

## 2. Goals

1. Preserve current Export behavior (menu + file output) unchanged.
2. Add a new File-menu entry to preview selected tasks as Markdown using the same selection pipeline as Export.
3. Keep Preview window independent/non-modal so user can continue using the main window while preview is open.
4. Support copy as Markdown and copy as HTML from preview.
5. Support Save As `.md` from preview.
6. Make Search show global results across Active + Archived + Deleted tasks in the middle task list.
7. On search clear, restore the exact pre-search view context (product + active/archived/deleted mode + selected task where possible).

## 3. Non-goals

- No removal or redesign of existing export-to-file flow.
- No changes to database schema for this feature.
- No search ranking redesign or advanced filter UI in this scope.
- No editable preview content in this scope (preview is read-only).

## 4. Functional Design

### 4.1 New Preview Entry and Shared Pipeline

- Add menu action under File:
  - `Preview Selected Tasks as Markdown...`
- Reuse existing export preparation flow:
  1. Step 1 scope dialog (all products / single product, AI summary option),
  2. Step 2 Product -> Main Task -> Sub Task selection tree,
  3. AI one-line summary resolution (same default-off + confirmation + fallback behavior),
  4. Markdown generation via `TaskExportService::buildMarkdown`.
- Divergence point:
  - Export action writes file.
  - Preview action opens preview dialog with generated markdown text.

### 4.2 Markdown Preview Window

- New `MarkdownPreviewDialog` (non-modal):
  - read-only markdown-rendered view area,
  - buttons:
    - `Copy Markdown`
    - `Copy HTML`
    - `Save As .md`
    - `Close`
- Copy behavior:
  - Markdown copy uses raw generated markdown string.
  - HTML copy uses markdown-rendered HTML output from the same source text.
- Save behavior:
  - Save current markdown content to user-selected `.md` file path.

### 4.3 Search Behavior Correction

- Current issue in `MainWindow::onSearchRequested`: query executes but task pane is not switched to result mode.
- New behavior:
  - search query runs globally through existing DB search path,
  - results are pushed into task list model and shown immediately in Task pane,
  - selecting a result continues to load that task in editor pane.
- Search clear behavior:
  - restore the pre-search browsing context:
    - selected product id,
    - active/archived/deleted filter mode,
    - previously selected task (if still available).

## 5. State and Data Flow

### 5.1 Shared export/preview preparation output

Introduce an internal prepared-export payload (in-memory) containing:

- `QList<ExportTaskItem> selectedItems`
- generated `QString markdown`
- metadata needed for completion messages.

Both Export and Preview consume this payload.

### 5.2 Search session snapshot

Store a lightweight snapshot when entering search mode:

- product id,
- task pane mode (active/archived/deleted),
- selected task id.

On clear, reapply snapshot and reload corresponding list.

## 6. Error Handling

### 6.1 Preview

- If no tasks selected after Step 2, show existing selection warning and do not open preview.
- If HTML conversion/copy fails, surface warning and fallback to Markdown copy.
- If Save As fails, show explicit file-write error dialog and keep dialog open.

### 6.2 Search

- Empty query behaves as clear (restore pre-search state).
- Zero-result query shows empty results list plus status message; no crash and no stale list.
- If DB search fails, preserve previous view and show warning/status error.

## 7. Component Impact

- `src/app/MainWindow.cpp/.h`
  - add preview menu action and handler,
  - split shared export/preview preparation logic,
  - wire search result rendering and search-state restore.
- `src/ui/TaskPane.cpp/.h`
  - expose API for loading search result tasks and restoring normal mode cleanly.
- `src/models/TaskListModel.cpp/.h`
  - ensure search-result loading works with existing display row logic and selection mapping.
- `src/services/TaskExportService.cpp/.h`
  - reuse markdown generation unchanged as shared source for export and preview.
- `src/ui/MarkdownPreviewDialog.cpp/.h` (new)
  - non-modal markdown viewer + copy/save actions.
- `CMakeLists.txt`
  - add new preview dialog source/header.

## 8. Validation

1. Build validation:
   - release build succeeds.
2. Preview workflow:
   - menu opens same Step1/Step2 flow,
   - preview window opens non-modally,
   - markdown content structure matches export output,
   - copy markdown works,
   - copy html works,
   - save as `.md` works.
3. Export regression:
   - existing export-to-file path still works as before.
4. Search behavior:
   - global search shows Active + Archived + Deleted results in task list,
   - clicking result opens editor,
   - clear search restores pre-search product/filter/task context.

