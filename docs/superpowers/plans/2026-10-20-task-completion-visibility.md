# Task Completion Visibility Implementation Plan

**Goal:** Implement the approved Show completed checkbox for main tasks and
subtasks in every Tasks view, with persistent settings and safe editing/movement.

**Architecture:** Filter the existing TaskListModel display rows while retaining
the full loaded collection. TaskPane owns presentation and selection; MainWindow
guards pending editor saves before a note can disappear. Reuse settings storage.

**Tech Stack:** C++17, Qt 6.8.3 Widgets/Test/SQLite, CMake, PowerShell.

Implementation runs in the existing f805 worktree, inline in this session.

## Tasks

- [x] Add regression cases to `tests/NexusCoreTests.cpp` for all five work
  statuses, subtree filtering, expansion, search/lifecycle refresh and reorder
  with hidden tasks. Add UI/movement regressions to `tests/SubtaskMoveTest.cpp`
  for checkbox persistence, saves/failures, selection, search, status updates
  and drag/drop through hidden siblings. Run them and observe missing behavior.
- [x] Extend `src/models/TaskListModel.h/.cpp` with `showCompleted`, visible and
  hidden main-task counts, visible-child flags, search-aware refresh and
  complete-group reorder. Filter only Completed rows; retain expansion IDs.
- [x] Extend `src/ui/TaskPane.h/.cpp` with the checkbox and empty label; retain
  selected IDs on model reset, centralize status updates with a save guard and
  failure reporting, persist preference, report filtered search counts, and
  reveal completed destinations explicitly after moves.
- [x] Update `src/app/MainWindow.cpp` and `src/ui/EditorPane.h` to guard the
  actual edited task/subtask before hiding and clear it only after success.
  Update `src/ui/TaskListView.cpp` to calculate subtask drop positions in the
  complete sibling list; use visible neighbors for context-menu moves.
- [x] Build and run focused core/UI/movement cases, fix observed issues, then
  run `.\scripts\validate.ps1 -QtRoot C:\Qt\6.8.3\msvc2022_64`.
- [x] Review the diff against the approved design; update README and SPEC to
  describe the implemented behavior and record operational gotchas if needed.

## Verification commands

Use Visual Studio 2022 Build Tools CMake (discovered through vswhere) to
configure `build` with `-DBUILD_TESTING=ON` and the Qt prefix above. Build
`NexusCoreTests` and `SubtaskMoveTest` in Release, then use CTest with
`-R "^(nexus-core|SubtaskMoveTest)$"`. Qt DLLs must be on PATH; UI tests
use offscreen Qt and a temporary database. Full validation builds all
targets, runs every CTest suite and checks editor JavaScript syntax.

## Acceptance checks

- Default checked; preference survives new panes/restarts/products/views.
- Completed parents hide all children; only completed children hide under
  unfinished parents; all-hidden empty state and search counts are explicit.
- Pending hidden-note edits save first; failure leaves checkbox/status intact.
- Retained notes keep selection/editor; hidden notes clear after success.
- Hidden items survive all reorder/move operations without incorrect positions.
- Exports and summaries retain their existing scope.

## Verification result (2026-10-20)

- Windows validation helper: all targets built, all 10 CTest suites passed, editor syntax checks passed.
- Core suite: 33 passed. UI/movement suite: 60 passed; 2 existing native mouse cases skipped under offscreen Qt.
- Independent review findings fixed and covered by regressions: mixed-product/status search reorder and clearing selection when the hidden editor differs from the selected row.
- Implementation was first validated locally on feat/task-completion-visibility.
  Release preparation proceeds separately on agents/release-v1.0.16 through
  the required branch and exact-tag Build workflows.
