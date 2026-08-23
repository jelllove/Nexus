# Task Export Tree + Markdown Polish Design

Date: 2026-08-23  
Scope: Refine existing Markdown export with true Product/Main/Sub tree selection and richer visual output.

## 1. Goal

Improve export UX and output quality:

1) Step-2 selection must be a real 3-level selectable tree: Product -> Main Task -> Sub Task.  
2) Exported Markdown must be structured by Product -> Main Task -> Sub Task.  
3) Markdown should be significantly more visual via colorful badges + emoji, especially for status and summary readability.

## 2. UX Design

### 2.1 Tree selection behavior

- Product, Main Task, Sub Task nodes are all checkable.
- Parent-child linkage:
  - Checking Product checks all descendant Main/Sub.
  - Checking Main checks all descendant Sub.
  - Child changes recalculate ancestor states (Checked / PartiallyChecked / Unchecked).
- Export rule:
  - Export a Main Task when either:
    - Main Task itself is checked, or
    - at least one of its Sub Tasks is checked.
  - If only some Sub Tasks are checked, export only those selected Sub Tasks.

### 2.2 Dialog structure

- Keep existing Step-1 scope dialog.
- Replace Step-2 task picker internals with tri-state tree interactions.
- Keep Select All / Clear All shortcuts.

## 3. Markdown Output Design

### 3.1 Required hierarchy

Output must be:

- Product section
  - Main Task entries
    - Selected Sub Task checklist items

### 3.2 Visual polish

- Add colorful Shields badges (GitHub-rendered) + emoji:
  - document summary badges (tasks/subtasks/time)
  - per-main-task status badge
  - work status badge
  - subtask completion badge
- Keep emoji-enhanced headings and checklist icons.
- Make summary block clearer:
  - `> 🧠 AI Summary` style callout with stronger visual separation.

## 4. Data Flow / Component Changes

### 4.1 `MainWindow`

- Update `promptTaskSelectionDialog(...)`:
  - build three-level tree with subtask nodes
  - implement check-state propagation + ancestor recompute
  - collect selected main/sub combinations into export DTOs

### 4.2 `TaskExportService`

- Extend markdown renderer for:
  - per-product grouping
  - colorful badge rendering
  - clearer status/summary/subtask visual blocks

## 5. Error Handling

- No selected exportable tasks -> explicit warning.
- File write failure -> explicit error dialog.
- AI summary failure -> fallback summary and continue export.

## 6. Validation

1) Build passes.  
2) Manual:
- Product-level full select works.
- Main-level partial select works.
- Sub-only selection exports parent Main with selected subs.
- Markdown output shows product-grouped hierarchy and colorful badges/emoji.
