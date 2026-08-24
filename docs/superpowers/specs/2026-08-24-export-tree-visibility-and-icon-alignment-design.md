# Export Tree Visibility and Icon Alignment Design

Date: 2026-08-24  
Scope: Fix Step-2 export picker so users always see a clear 3-level hierarchy (Product -> Main Task -> Sub Task), and align status emoji usage with existing task markers.

## 1. Problem Statement

Current export behavior still feels like a product-level bulk export for some users because third-level subtasks are not consistently visible in Step-2. The UI must clearly present a multi-level tree-list and make status icons consistent with existing task markers.

## 2. Goals

1. Step-2 must always appear as a 3-level tree-list:
   - Product
   - Main Task
   - Sub Task
2. Main Task icon must use the same `Task::workStatusIcon(...)` style currently used in the app.
3. Sub Task level must be visible even when a Main Task has no actual subtasks (via a non-selectable placeholder).
4. Markdown export structure must remain Product -> Main Task -> Sub Task.

## 3. Non-goals

- No change to Step-1 scope settings.
- No change to AI summary generation flow.
- No schema changes in database tables.

## 4. UI Design

### 4.1 Step-2 Tree Construction

- Product node:
  - Text prefix: `📚`
  - Checkable
- Main Task node:
  - Text prefix: `Task::workStatusIcon(task.workStatus)`
  - Checkable
- Sub Task node:
  - Text prefix: `✅` / `⬜` based on `completed`
  - Checkable

### 4.2 Mandatory Third Level Visibility

For each Main Task:

- If database subtasks exist:
  - render each subtask as a third-level child.
- If no database subtasks exist:
  - render one disabled placeholder child:
    - text: `🫥 (No Sub Task)`
    - not checkable
    - not exported

This guarantees visual three-level structure regardless of task content.

### 4.3 Selection Behavior

- Keep existing tri-state propagation:
  - parent check toggles descendants
  - descendant changes recalculate parent checked/partial state
- Placeholder child does not affect selection and export counts.

## 5. Export Data / Markdown Behavior

### 5.1 Export Item Construction

- Export a Main Task if:
  - Main Task is checked, or
  - any real Sub Task is checked.
- Placeholder child is ignored during export-item collection.

### 5.2 Markdown Rendering

- Keep existing visual polish (badges + emoji + summary callout).
- Keep hierarchy:
  - `## Product`
  - `### Main Task` (with work-status icon prefix)
  - `#### Selected Sub Tasks`
- When no real subtasks selected/exist:
  - render `- 💤 No sub tasks`.

## 6. Validation

1. Build succeeds.
2. Manual UI checks:
   - Product with subtasks: third level visible and selectable.
   - Product without subtasks: placeholder third level visible.
   - Main Task icon matches existing task marker style.
3. Export checks:
   - Placeholder never appears in markdown.
   - Markdown remains grouped by Product -> Main Task -> Sub Task.
