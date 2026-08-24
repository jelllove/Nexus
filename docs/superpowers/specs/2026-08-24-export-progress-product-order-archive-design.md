# Export Progress, Product Ordering, and Product Archive Design

Date: 2026-08-24  
Scope: Enhance export UX and Product management in one release, including AI confirmation, export progress/logging, Product-level selection behavior, markdown structure updates, manual Product ordering, and Product archive management.

## 1. Problem Statement

Current behavior has five gaps for the requested workflow:

1. Export AI summarization control is not safe enough for accidental usage.
2. Export process has no visible progress/log experience.
3. Product-level selection behavior in export must always include all child Main/Sub tasks.
4. Product list cannot be manually reordered.
5. Product-level archive management does not exist.

In addition, markdown export formatting needs to become a collapsible tree and simplify metadata fields.

## 2. Goals

1. Keep AI summarization optional and off by default, with explicit second confirmation only when enabled.
2. Provide a cancelable export progress dialog with a progress bar and live logs.
3. Ensure selecting a Product in export tree selects all descendant Main/Sub tasks.
4. Add manual drag-and-drop ordering for Products.
5. Add Archived Product capability in Product pane with a dedicated expandable section.
6. Change markdown output to a collapsible Product -> Main Task -> Sub Task tree.
7. Remove all colorful badges from markdown output and keep emoji/text readability.
8. Keep Product archive/reactivate operations independent from Task status.

## 3. Non-goals

- No redesign of the full application layout.
- No change to Task archive/delete semantics.
- No export log file persistence (`.log`) after completion.
- No batch schema refactor beyond Product archive support.

## 4. Functional Design

### 4.1 Export Settings and AI Confirmation

- In Export Step-1:
  - `Include simple description (AI one-line summary)` remains optional.
  - Default state is unchecked.
- If user enables AI summary and clicks confirm:
  - show a second confirmation dialog before export starts;
  - only proceed when user confirms.
- If AI is not enabled, do not show second confirmation.

### 4.2 Export Progress, Logging, and Cancellation

- Replace silent wait with a dedicated modal export progress dialog containing:
  - progress bar (0-100%),
  - read-only scrolling log area,
  - cancel button.
- Log phases:
  1. collecting export selection,
  2. generating AI summaries (if enabled),
  3. building markdown,
  4. writing output file.
- Cancellation behavior:
  - before write phase: stop export and do not create output file;
  - during write phase: treat as export failure and show explicit error feedback.
- Errors are surfaced in both log and message dialog; no silent fallback for fatal steps.

### 4.3 Export Tree Selection Behavior

- Export Step-2 keeps Product -> Main Task -> Sub Task hierarchy.
- Product checked state must cascade to all real descendants:
  - all Main tasks checked;
  - all real Sub tasks checked.
- Parent partial-state logic remains intact when child items are manually changed.
- Disabled placeholder subtask nodes remain non-checkable and non-exportable.

### 4.4 Markdown Output Structure and Content Rules

#### 4.4.1 Structure

- Output is a collapsible tree using nested `<details>/<summary>`:
  - Product layer (`details`)
    - Main Task layer (`details`)
      - Sub Task checklist
- Tree is expandable layer-by-layer in markdown viewers that support details blocks.

#### 4.4.2 Styling and field simplification

- Remove all colorful badges:
  - top summary badges,
  - per-task badges.
- Keep emoji-based readability for statuses/checklists.
- Main Task metadata block is simplified to a single field:
  - `Status` (mapped from previous `Work` icon + text).
- Remove these previous fields from task metadata:
  - `Updated`,
  - previous `Status` field (task lifecycle status),
  - `Title` row (title is already in heading/summary).

#### 4.4.3 Retained content

- Keep AI Summary section (when enabled and generated).
- Keep content preview section (existing behavior).
- Keep selected subtask checklist rendering.

## 5. Product Ordering Design

- Product list supports drag-and-drop reordering in UI.
- Persist order through existing `sort_order` in `products`.
- Implement independent ordering for each Product status group:
  - Active Products reorder independently,
  - Archived Products reorder independently.

## 6. Archived Product Design

### 6.1 Data model

- Extend `products` with:
  - `status` (`active` / `archived`, default `active`),
  - `archived_at` datetime nullable.
- Add migration step (next db_version) to create these columns for existing databases.

### 6.2 Behavior

- Product pane default:
  - show Active Products only.
- Add `Show Archived Products` control:
  - when expanded, show Archived Products in a separate section below Active list.
- Context actions:
  - Active Product: `Archive`,
  - Archived Product: `Reactivate`.
- Product archive/reactivate does **not** mutate any child Task status.

## 7. Component Impact

- `src/app/MainWindow.cpp`
  - Export Step-1 defaults and second confirmation.
  - Export progress dialog orchestration and cancellation wiring.
  - Product-level selection cascade verification in Step-2.
- `src/services/TaskExportService.cpp`
  - Markdown output conversion to collapsible tree.
  - Remove badge generation from output path.
  - Simplify task metadata to single `Status` field.
- `src/ui/ProductPane.cpp/.h`
  - Archived section toggle and dual-list presentation.
  - Archive/reactivate actions and drag-drop UI integration.
- `src/models/ProductListModel.cpp/.h`
  - Drag/drop reorder support and status-scoped loading.
- `src/db/DatabaseManager.cpp/.h`
  - Product status migration and CRUD/query/reorder methods for archive-aware behavior.

## 8. Validation

1. Build validation:
   - Release build passes.
2. Export UX validation:
   - AI checkbox defaults off;
   - AI second confirm appears only when AI is enabled;
   - progress dialog shows progress + logs and supports cancel.
3. Export correctness validation:
   - Product full-select cascades to all real descendants;
   - markdown output is collapsible Product -> Main -> Sub tree;
   - no colorful badges in output;
   - metadata only has `Status` line.
4. Product management validation:
   - Active and Archived sections render correctly;
   - each section supports independent drag-drop reorder;
   - archive/reactivate Product does not change any Task status.

