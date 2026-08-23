# Task Markdown Export Design

Date: 2026-08-23  
Topic: Export Active tasks to Markdown with selectable scope/items, subtask inclusion, and optional AI simple description

## 1. Background

Nexus currently has no user-facing task export feature. The goal is to add a Markdown export flow that:

- only exports **Active** tasks
- allows user selection in two levels:
  - selection of export scope
  - selection of specific main tasks
- always includes selected main tasks' sub tasks
- supports optional `simple description` generation:
  - use AI one-sentence summary when AI config exists
  - fallback to first 120 plain-text chars when AI is not configured

## 2. Goals & Non-goals

### Goals

1. Add a File menu action: `Export Tasks to Markdown...`
2. Add a two-step export wizard UX:
   - Step 1: choose scope + whether to include simple description
   - Step 2: choose specific main tasks
3. Export to one `.md` file with rich emoji and mixed markdown formats
4. Include selected main tasks and all corresponding sub tasks
5. Keep existing AI title/summary behavior intact

### Non-goals

- Export archived/deleted tasks
- Export to formats other than Markdown
- Add persistent export presets
- Rewrite existing task list UI architecture

## 3. User Flow

1. User clicks `File -> Export Tasks to Markdown...`
2. Step 1 dialog:
   - scope:
     - All products (Active only)
     - Single product (Active only)
   - option:
     - Include simple description (default OFF)
3. Step 2 dialog:
   - show grouped main tasks by product
   - allow multi-select (main tasks only)
   - support select all / clear all
4. User picks output `.md` path
5. System gathers selected data, generates Markdown, saves file
6. User receives success/failure notification

## 4. Architecture & Components

### 4.1 MainWindow changes

- Add export QAction in `setupMenuBar()`
- Add slot/method `exportTasksToMarkdown()`
- Add internal helpers for:
  - step dialogs
  - task collection from DB
  - markdown rendering
  - file write result handling

### 4.2 Data access

Reuse existing APIs:

- `DatabaseManager::getAllProducts()`
- `DatabaseManager::getTasksForProduct(productId, TaskStatus::Active)`
- `DatabaseManager::getSubtasks(taskId)`

No new persistence schema changes required.

### 4.3 AI export summary isolation

Current `AIService::summaryGenerated` is tied to editor behavior.  
To avoid side effects, add a dedicated export-oriented API in `AIService`, callback-based, for one-sentence summary generation. This API must not emit `summaryGenerated` used by editor flows.

Behavior:

- if include description = false: skip all summary generation
- if include description = true:
  - AI configured: request one-sentence summary per selected task
  - AI missing config: use plain text fallback (first 120 chars)
  - AI per-task failure: fallback to first 120 chars and continue export

## 5. Markdown Output Spec

The file should use multiple markdown styles plus rich emoji.

## 5.1 Document structure

1. Header
   - `# 📦 Task Export`
   - exported time (`🕒`)
   - statistics (`📊` main tasks count, sub tasks count)
2. Product sections
   - `## 📚 <Product Name>`
3. Task blocks
   - `### 🟢 <Task Title>`
   - metadata table:
     - Status
     - Title
     - Updated time
   - optional description blockquote:
     - `> 🧠 <one-line summary>`
   - sub task checklist:
     - `- [x] ✅ ...`
     - `- [ ] ⬜ ...`
   - horizontal separator `---`

## 5.2 Status mapping

- Active task status icon in title: `🟢` (or equivalent active icon)
- Sub task completed: `✅`
- Sub task pending: `⬜`

## 6. Error Handling

1. No active tasks in selected scope:
   - show warning and stop
2. No main task selected in step 2:
   - show warning and stop
3. Save path canceled:
   - no-op with info status
4. File write failure:
   - show explicit error with path/reason
5. AI request error for any task:
   - fallback to 120-char plain text for that task
   - keep export running
   - show non-blocking warning summary after export

## 7. Validation Plan

1. Build validation: compile project successfully
2. Manual checks:
   - Export all active tasks across all products
   - Export active tasks for one selected product
   - Step 2 select subset of main tasks only
   - Verify selected main tasks include all sub tasks
   - Include description ON with AI configured
   - Include description ON with AI not configured (120-char fallback)
   - Verify markdown readability and emoji-rich formatting

## 8. Rollout & Compatibility

- Backward compatible with existing DB schema and task operations
- No behavior change to current editor summary/title generation paths
- New menu action is additive only
