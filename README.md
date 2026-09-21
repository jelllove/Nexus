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
The two test reports are saved in `build/UpdateServiceTest.txt` and
`build/UpdateUiTest.txt`; test executables are kept separately under `build/tests`.

## License

MIT License. See [LICENSE](./LICENSE).
