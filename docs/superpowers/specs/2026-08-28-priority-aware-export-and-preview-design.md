# Priority-Aware Export and Preview Design

## Goal

Improve task export/preview readability by aligning sub task and priority presentation with the current task list behavior.

## Confirmed Product Decisions

1. Sub task markdown rows must no longer include `[x]` / `[ ]`.
2. Each exported sub task row must start with its own work-status emoji.
3. Main task headings in export/preview must include P0/P1/P2/P3 labels and use priority-matched background color.
4. Export selection dialog main-task nodes must also show priority label and priority color distinction.
5. In exported markdown, tasks are grouped by priority (P0 -> P3), with horizontal separators between different priority groups.
6. Using small inline HTML in markdown is allowed for priority background rendering.

## Current State (Before Change)

- Export selection tree shows main task status icon + title, but no priority label/color.
- Export markdown includes sub task checkbox-like markers (`[x]` / `[ ]`) plus completion emoji.
- Export markdown output is product-grouped, but not priority-grouped with separators.
- `Task` already has priority text and colors via shared helpers.

## Proposed Design

### 1) Data Contract Updates (Export DTOs)

- Extend `ExportTaskItem` with:
  - `TaskPriority priority`
  - `QString priorityText`
  - `QString priorityBadge`
  - `QString priorityBgColorHex`
  - `QString priorityTextColorHex`
- Extend `ExportSubTaskItem` with:
  - `TaskWorkStatus workStatus`
  - `QString workStatusIcon`

These values are resolved once in export selection extraction so preview/export share identical source data.

### 2) Export Selection Tree (Step 2/2)

- For each main task node:
  - Prefix title with priority badge text (`[P0]`, `[P1]`, `[P2]`, `[P3]`).
  - Keep existing work-status emoji prefix.
  - Apply background color on the node using existing priority background color helper.
- Keep current check-state cascading behavior unchanged.

### 3) Markdown Output Rules (Shared by Preview + Export)

- Keep product-level grouping (`## 📚 Product Name`).
- Inside each product, group selected tasks by priority in order:
  - P0 Critical
  - P1 High
  - P2 Medium
  - P3 Low
- Emit a priority section header before each non-empty group, e.g.:
  - `### 🔴 P0 - Critical`
- Insert `---` between rendered priority groups.
- Render main task heading using inline HTML for background color, e.g.:
  - `<span style="background:#fdf0ef; color:#e74c3c; ...">P0</span> ✅ **Task Title**`
- Render sub tasks as:
  - `- ✅ Sub task title`
  - no checkbox syntax.

### 4) Compatibility and Fallbacks

- If priority metadata is missing, default to `P2 - Medium` style.
- If sub task status metadata is missing, default to `Not Started` emoji.
- No schema change and no data migration required.

## Error Handling

- Keep existing export cancellation behavior unchanged.
- Keep existing file write failure handling unchanged (explicit message box warning).
- If markdown color decoration cannot be resolved, continue export with default P2 decoration instead of failing export.

## Validation Plan

1. Open export selection dialog:
   - main task rows show status + `[Px]` + title.
   - main task background colors match task-list priority tones.
2. Select mixed-priority tasks and open markdown preview:
   - tasks grouped P0 -> P3.
   - `---` appears between non-empty priority groups.
   - sub task rows show status emoji only (no `[x]` / `[ ]`).
3. Save markdown and inspect in external markdown viewer:
   - structure remains readable even if viewer partially ignores inline styles.
4. Regression check:
   - existing export options (all products / selected product, with or without description) still function.

## Non-Goals

- No change to task list sorting behavior outside export/preview output.
- No redesign of task-card visual system.
- No new priority levels beyond existing P0-P3 model.
