# Nexus v1.0.18

## Download and install

**[Windows x64 installer (EXE)](https://github.com/jelllove/Nexus/releases/download/v1.0.18/Nexus-Setup-v1.0.18-x64.exe)**

| Platform | Download |
| --- | --- |
| Windows x64 portable | [ZIP](https://github.com/jelllove/Nexus/releases/download/v1.0.18/Nexus-1.0.18-Windows-x64.zip) |
| macOS Apple Silicon | [DMG](https://github.com/jelllove/Nexus/releases/download/v1.0.18/Nexus-1.0.18-macOS-arm64.dmg) |
| Ubuntu 22.04/24.04 x64 | [DEB](https://github.com/jelllove/Nexus/releases/download/v1.0.18/Nexus-1.0.18-Linux-x64.deb) |
| Fedora 43/44 x64 | [RPM](https://github.com/jelllove/Nexus/releases/download/v1.0.18/Nexus-1.0.18-Linux-x64.rpm) |
| Linux x64 portable | [tar.gz](https://github.com/jelllove/Nexus/releases/download/v1.0.18/Nexus-1.0.18-Linux-x64.tar.gz) |

Use the installer or the entire portable archive, not just its executable: Qt
libraries, plugins and WebEngine resources are required. The PNGs and
installer-evidence ZIPs are verification materials.
[SHA256SUMS](https://github.com/jelllove/Nexus/releases/download/v1.0.18/SHA256SUMS)
provides checksums for every other published file.

## AI quick task creation

- Click **AI Quick Task** beside the search bar, choose **File > AI Quick Task**,
  or press **Ctrl+Alt+N** (**Command+Option+N** on macOS) to open a non-modal,
  plain-text capture box. No project needs to be selected first.
- Describe the work and choose **Analyze with AI...**. The configured
  OpenAI-compatible service recommends an existing project and related parent
  task, a new main task, or a new project with a task.
- Review the optimized title and plain-text context. Every destination and text
  field can be changed before choosing **Create**. Uncertain matches leave the
  destination unselected, and project/parent dropdowns support typing to search.
- New main tasks default to **P1 High**, with an editable priority dropdown.
  Subtasks keep the existing model without independent priority and never
  change their parent's priority, content or work status.
- **Manual setup** works without AI configuration and remains available after
  an AI error. Reopening the capture window keeps its unfinished input while
  that window remains open. The existing global Control+Shift+N is unchanged.

## Consent and safe persistence

- Analysis requires explicit consent before sending the input, active project
  names, eligible main-task titles and bounded plain-text note excerpts.
  **Show Details** displays the exact JSON source; settings and attachments are
  not included. Archived/deleted tasks and completed parent tasks are excluded.
- All eligible destination IDs and titles are retained. For a large source,
  `notesIncluded: false` explicitly omits excerpts instead of destinations; if
  titles alone exceed the limit, use manual setup.
- AI output is a recommendation, not a write. Invented IDs, inconsistent
  project/parent combinations and malformed output are rejected. Cancellation
  aborts requests and ignores late responses.
- Final creation rechecks the current destination and atomically saves all
  related records. Failures preserve the editable draft and cannot leave an
  empty new project. Context is escaped note text, not executable generated HTML.
- Pending editor changes must save before analysis or creation. Successful
  creation opens the new note, including from archived, deleted or search views.

## Supported macOS release runners

macOS builds and installed-DMG verification now use `macos-15` ARM64 runners
with explicit native-architecture checks. GitHub
[retired all macOS 14 runners](https://github.com/actions/runner-images/issues/15226)
on November 2, 2026. Package architectures and all native verification gates
remain unchanged.

## Upgrading and verification

Save notes and use **File > Quit** before installing; closing the main window
alone may leave Nexus in the tray. The database schema remains v10, preserving
existing notes, settings and task states.

Publication requires editor syntax/bundle validation, all eleven CTest suites
on Windows, macOS and Linux, and all six native macOS/Linux installer
environments. Fifty focused assistant cases cover actual shortcut activation,
all creation kinds, editable recommendations, manual/uncertain destinations,
exact input limits, consent, timeout/cancellation, save failures, stale
destinations and transaction ownership/rollback. AI tests use a loopback fake
service and synthetic databases; real-provider matching quality is not certified.

The package matrix remains Windows x64 EXE/ZIP, macOS ARM64 DMG and Linux x64
tar.gz/DEB/RPM, with native screenshots/evidence and checksums. macOS remains
unsigned/unnotarized. DEBs are verified on Ubuntu 22.04/24.04 and RPMs in Fedora
43/44 containers; these are unsigned packages, not an apt/yum repository.
Intel macOS, Linux ARM64, AppImage and native Wayland global hotkeys are not
added. This publication targets jelllove/Nexus; mirror synchronization is separate.
