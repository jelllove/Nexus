# Nexus

A desktop task management application with a 3-pane layout inspired by OneNote, built with C++17 and Qt 6.

![Windows](https://img.shields.io/badge/platform-Windows-blue)
![Qt 6.8](https://img.shields.io/badge/Qt-6.8.3-green)
![C++17](https://img.shields.io/badge/C%2B%2B-17-orange)

## Features

- **3-Pane Layout** — Products (notebooks) → Tasks (pages) → Rich Editor, with a resizable splitter
- **Rich Text Editor** — TipTap-based WYSIWYG editor embedded via QWebEngineView, supporting headings, lists, code blocks, images, and more
- **Task Cards** — Visual task cards with priority badges (P0–P3), color-coded priority bars, created/modified timestamps, and due-date progress bars with cute animal icons
- **Sub Task Work Status** — Sub tasks support the same 5 statuses as main tasks (Not Started/Ongoing/Paused/Completed/Waiting), rendered as smaller status icons with click-to-change popups
- **Sub Task Movement** — Drag subtasks to reorder or move between visible main tasks, with drop indicators and edge scrolling; a right-click menu also offers up/down actions and a searchable destination picker
- **Due Dates & Progress** — Set due dates on tasks; a day-based progress bar shows time elapsed with green/orange/red color coding
- **Full-Text Search** — SQLite FTS5-powered global search across Active/Archived/Deleted task titles and content, rendered directly in the task list with context restore on clear
- **Priority Management** — Click the priority badge or right-click to change task priority; tasks auto-sort by priority then due date
- **Active / Archived Toggle** — Quick-switch between active and archived tasks with toggle buttons
- **Product Ordering & Archive** — Drag to reorder products, archive/reactivate products, and expand archived products below the active list
- **System Tray** — Minimize to tray; restore with a click
- **Global Hotkey** — Win32 `RegisterHotKey` to summon the window from anywhere
- **GitHub Release Auto-Update** — Checks on startup and every 2 hours while running (including in the system tray), optionally downloads updates automatically, and asks before saving the current note and launching the installer
- **AI Integration** — OpenAI-compatible API for AI-powered title generation from task content
- **Image Support** — Paste or drag images into the editor; stored locally in AppData
- **Content History** — Automatic snapshots for undo/redo support
- **Markdown Export** — Export selected Active tasks via Product → Main Task → Sub Task selection, optional AI one-line summaries with extra confirmation, progress dialog with logs/cancel, and a nested pure-Markdown tree output
- **Markdown Preview** — Reuse the same selection/summarization pipeline and open a non-modal preview window with `Copy Markdown`, `Copy HTML`, and `Save As .md`
- **Database Maintenance** — Daily `VACUUM INTO` backups and delayed weekly background compaction (`wal_checkpoint(TRUNCATE)` + `VACUUM`)

## Architecture

```
src/
├── app/           MainWindow (3-pane coordinator)
├── db/            DatabaseManager (SQLite + FTS5, WAL mode)
├── models/        Product & Task structs, Qt list models
├── ui/            ProductPane, TaskPane, TaskCardDelegate,
│                  EditorPane, SearchBar, SettingsDialog, MarkdownPreviewDialog
├── editor/        EditorBridge (C++ ↔ JS via QWebChannel)
├── services/      AIService, ImageManager
└── platform/      GlobalHotkey (Win32)

resources/
└── editor/        TipTap HTML/CSS/JS bundle
```

## Prerequisites

- **Windows 10/11**
- **Visual Studio 2022** (MSVC v14.44+)
- **Qt 6.8.3** (msvc2022_64) — install via [aqtinstall](https://github.com/miurahr/aqtinstall):
  ```
  pip install aqtinstall
  aqt install-qt windows desktop 6.8.3 win64_msvc2022_64
  aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -m qtwebengine qtwebchannel qtpositioning
  ```
- **CMake 3.21+** (bundled with VS2022)

## Build

### Quick Build (Recommended)

Double-click or run from a terminal:

```bat
build.bat
```

This will configure, build, deploy Qt DLLs, and package everything into the `dist/` directory.

### Manual Build

```bash
# Configure
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64

# Build
cmake --build build --config Release

# Deploy Qt runtime
C:/Qt/6.8.3/msvc2022_64/bin/windeployqt6.exe build/Release/Nexus.exe
```

## Run

```
dist\Nexus.exe
```

The database (`nexus.db`) is stored in `%APPDATA%\Nexus\Nexus\` by default.

## Moving subtasks

- Drag a subtask by its title or dotted grip. A blue line above/below another
  subtask indicates the insertion position, including under a different parent.
- Drop on a main-task card to append there (also works with collapsed or empty
  main tasks). The destination is expanded and the moved subtask selected.
  Dropping in the blank space below the list appends to the last main task.
- Hold near the top/bottom of the list to scroll while dragging. **Esc**, release
  outside the list, loss of focus, or a list refresh cancels the drag. Clicking without dragging
  still opens the editor; clicking the status icon still opens its status menu.
- Expand a main task, then right-click a subtask and choose **Move Up** or
  **Move Down** to change its order. The first/last boundary action is disabled.
- Choose **Move to Main Task...**, search by product or main-task title, select
  the destination and click **Move**. Task IDs distinguish duplicate titles.
  Active and archived main tasks in active or archived products are supported.
- Moving via the destination picker appends the subtask at the end, opens that
  destination and selects the moved subtask. Use **Move Up** afterwards to
  adjust its position. Reparenting from search exits search; reordering stays
  in the current search results.
- A move preserves the subtask's identity, title, rich content and work status.
  The current editor note is saved before moving; save failures block the move.
  Database moves are atomic and the order persists across restarts and is used
  by Markdown export/preview.
- Deleted main tasks cannot be sources or destinations until restored.
  Dragging works within the current task list, including search results.
  Use the destination picker for main tasks in other products that are not
  visible in the current list. This is internal movement, not a file export.

### Trying a different local build

Nexus only runs one instance at a time. Launching a different executable while
an older copy is running brings the **older copy** to the front; it does not
switch builds. Closing the window only hides it in the system tray.
Before trying a new local build, allow pending edits to autosave, then use
**File > Quit** (**Ctrl+Q**) or the tray's **Quit**, and launch the new executable.

## Update checks

- **Settings > Updates** controls automatic startup and two-hour checks; changes take effect immediately.
- **Help > Check for Updates** also works when automatic checks are disabled.
- Checks and downloads cannot overlap. Background errors are logged and shown in the status bar, not an error popup; the next scheduled check can retry.
- The same release is not repeatedly prompted during a session. Manual checks can retry a dismissed update.
- Automatic download does not mean automatic shutdown: **Install now** saves the current task or subtask before launching the installer. If saving or launching fails, Nexus stays open.
- Choose **Later** to keep working; use **Help > Check for Updates** to reopen the downloaded update without downloading it again during the same session.
- Checking requires Nexus to be running and Windows awake. This is not a Windows background service, and the installer may still require UAC confirmation.

## Tests

With the Qt Test component installed, configure with `-DBUILD_TESTING=ON`, build,
then run `ctest --test-dir build -C Release --output-on-failure`.
On Windows, add your Qt `bin` directory to `PATH` before running tests.
Test reports are saved in `build/UpdateServiceTest.txt`, `build/UpdateUiTest.txt`
and `build/SubtaskMoveTest.txt`; test executables are kept separately under
`build/tests`. The subtask suite uses an isolated temporary database and covers
ordering, reparenting, field preservation, rollback, the destination picker,
mouse dragging, drop indicators, cancellation, edge scrolling, editor saves
and selection.

On an interactive Windows desktop, the `systemMouseDrag` cases additionally use
Windows `SendInput` and system cursor movement, rather than direct Qt widget
events. They use a temporary database, move the mouse during the test, and are
skipped in the normal offscreen suite. With Qt `bin` on `PATH`, run them with:

```powershell
$env:QT_QPA_PLATFORM = "windows"
$env:QT_PLUGIN_PATH = "C:\Qt\6.8.3\msvc2022_64\plugins"
.\build\tests\Release\SubtaskMoveTest.exe systemMouseDrag -o ".\build\SubtaskSystemMouseTest.txt,txt"
```

## License

MIT License. See [LICENSE](./LICENSE).
