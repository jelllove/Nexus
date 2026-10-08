# Nexus

A desktop task management application with a 3-pane layout inspired by OneNote, built with C++17 and Qt 6.

![Windows](https://img.shields.io/badge/platform-Windows-blue)
![Qt 6.8](https://img.shields.io/badge/Qt-6.8.3-green)
![C++17](https://img.shields.io/badge/C%2B%2B-17-orange)

## Features

- **3-Pane Layout** — Products (notebooks) → Tasks (pages) → Rich Editor, with a resizable splitter
- **Rich Text Editor** — TipTap-based WYSIWYG editor embedded via QWebEngineView, supporting headings, lists, code blocks, images, and more
- **Task Cards** — Visual task cards with priority badges (P0–P3), color-coded priority bars, created/modified timestamps, and due-date progress bars with cute animal icons
- **Due Dates & Progress** — Set due dates on tasks; a day-based progress bar shows time elapsed with green/orange/red color coding
- **Full-Text Search** — SQLite FTS5-powered instant search across all task titles and content, with LIKE fallback
- **Priority Management** — Click the priority badge or right-click to change task priority; tasks auto-sort by priority then due date
- **Active / Archived Toggle** — Quick-switch between active and archived tasks with toggle buttons
- **Product Ordering & Archive** — Drag to reorder products, archive/reactivate products, and expand archived products below the active list
- **System Tray** — Minimize to tray; restore with a click
- **Global Hotkey** — Win32 `RegisterHotKey` to summon the window from anywhere
- **AI Integration** — OpenAI-compatible API for AI-powered title generation from task content
- **Image Support** — Paste or drag images into the editor; stored locally in AppData
- **Content History** — Automatic snapshots for undo/redo support
- **Markdown Export** — Export selected Active tasks via Product → Main Task → Sub Task selection, optional AI one-line summaries with extra confirmation, progress dialog with logs/cancel, and collapsible tree-style Markdown output without color badges
- **Database Maintenance** — Daily `VACUUM INTO` backups and delayed weekly background compaction (`wal_checkpoint(TRUNCATE)` + `VACUUM`)

## Architecture

```
src/
├── app/           MainWindow (3-pane coordinator)
├── db/            DatabaseManager (SQLite + FTS5, WAL mode)
├── models/        Product & Task structs, Qt list models
├── ui/            ProductPane, TaskPane, TaskCardDelegate,
│                  EditorPane, SearchBar, SettingsDialog
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
- **Node.js 22+** and npm for editor validation and rebuilding the TipTap bundle

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

## Development checks

On Windows, run the validation helper from PowerShell:

```powershell
.\scripts\validate.ps1 -QtRoot C:\Qt\6.8.3\msvc2022_64
```

The helper finds CMake on PATH or in Visual Studio 2022 (including Build Tools),
builds the application and tests, runs CTest, and checks editor JavaScript syntax.
It does not deploy the application or copy your database. `-QtRoot` defaults to
`QT_ROOT`, or `C:\Qt\6.8.3\msvc2022_64` when that environment variable is unset.
Use `-BuildDirectory` and `-Configuration Debug` for a separate debug build.

For manual validation, configure using the instructions above with
`-DBUILD_TESTING=ON`, then run:

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure --no-tests=error
npm --prefix editor-bundle run check
```

On macOS, configure with `cmake -S . -B build -DCMAKE_PREFIX_PATH="<Qt directory>" -DBUILD_TESTING=ON`
and use the same build/test commands. The tests use Qt Test and an in-memory
SQLite database; they do not open the application UI or access your personal data.
CTest makes the matching Qt DLLs available on Windows. Set `-DBUILD_TESTING=OFF`
when configuring a packaging-only build that does not need Qt Test.

The regression suite covers product/task lifecycle, due-date ordering, subtasks,
search index updates, settings, model roles and reorder transaction failures.
JUnit results are written to `build/tests/nexus-core.junit.xml`.

To rebuild the editor after changing its dependencies or entry point:

```powershell
npm --prefix editor-bundle ci
npm --prefix editor-bundle run check
npm --prefix editor-bundle run build
```

Commit intentional changes to both the lockfile and generated editor bundle.
The existing PR workflow now runs Windows/macOS tests and a separate editor
syntax/build job that also detects a stale generated bundle, retaining JUnit
results even when tests fail. Syntax validation
is not a full JavaScript linter, and these checks are not GUI end-to-end tests.
Repository administrators must make the workflow checks required in branch
protection; workflow configuration alone does not prevent merging.

## License

MIT License. See [LICENSE](./LICENSE).
