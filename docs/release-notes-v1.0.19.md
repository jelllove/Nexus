# Nexus v1.0.19

## Download and install

**[Windows x64 installer (EXE)](https://github.com/jelllove/Nexus/releases/download/v1.0.19/Nexus-Setup-v1.0.19-x64.exe)**

| Platform | Download |
| --- | --- |
| Windows x64 portable | [ZIP](https://github.com/jelllove/Nexus/releases/download/v1.0.19/Nexus-1.0.19-Windows-x64.zip) |
| macOS Apple Silicon | [DMG](https://github.com/jelllove/Nexus/releases/download/v1.0.19/Nexus-1.0.19-macOS-arm64.dmg) |
| Ubuntu 22.04/24.04 x64 | [DEB](https://github.com/jelllove/Nexus/releases/download/v1.0.19/Nexus-1.0.19-Linux-x64.deb) |
| Fedora 43/44 x64 | [RPM](https://github.com/jelllove/Nexus/releases/download/v1.0.19/Nexus-1.0.19-Linux-x64.rpm) |
| Linux x64 portable | [tar.gz](https://github.com/jelllove/Nexus/releases/download/v1.0.19/Nexus-1.0.19-Linux-x64.tar.gz) |

Use the installer or the entire portable archive, not just its executable.
Qt libraries, plugins and WebEngine resources are required.
[SHA256SUMS](https://github.com/jelllove/Nexus/releases/download/v1.0.19/SHA256SUMS)
lists every other published package, screenshot and evidence ZIP.

## AI quick task creation

- Click **AI Quick Task**, use **File > AI Quick Task**, or press **Ctrl+Alt+N**
  (**Command+Option+N** on macOS) to open a plain-text capture box without
  selecting a project first. The existing global Control+Shift+N is unchanged.
- Describe the work and analyze it with the configured OpenAI-compatible AI.
  Review an existing project/parent recommendation, or create a new main task
  or a new project with a task. Title and context are optimized in the input's
  language without treating existing notes as instructions.
- All destinations and text remain editable. Uncertain matches require manual
  selection; searchable dropdowns let you override even confident matches.
  **Manual setup** works without AI configuration and after AI failures.
- New main tasks default to **P1 High**, with an editable priority dropdown.
  Subtasks keep the existing model without independent priority and do not
  change their parent's priority, content or work status.

## Consent and safe persistence

- Explicit consent is required before sending input, active project names,
  eligible main-task titles and bounded note excerpts. **Show Details** shows
  the exact JSON; settings and attachments are excluded.
- Archived/deleted tasks and completed parent tasks are excluded. All eligible
  destination IDs/titles are retained; excerpts can be explicitly omitted to
  fit the source limit, with manual setup available if titles alone are too large.
- AI output cannot create records. Invented IDs and inconsistent parents are
  rejected. Cancellation aborts requests and ignores late responses.
- Final creation rechecks destinations and atomically saves every related
  record. Errors preserve the draft and cannot leave an empty new project.
  Context is escaped note text, not executable generated HTML.
- Pending editor changes must save before analysis or creation. Successful
  creation opens the new note from active, archived, deleted or search views.

## Native macOS verification without another runner allocation

GitHub retired macOS 14 runners on November 2, 2026. macOS work uses the
supported current-GA `macos-26` ARM64 image. The deployment target remains
macOS 12.0, matching Qt 6.8's baseline instead of raising the minimum to the
build host's OS version. To avoid a second allocation from the scarce
ARM64 runner pool, the build job also mounts and verifies its actual DMG.

During that verification the developer Qt SDK is physically unavailable at
its original path and Qt-related runtime overrides are removed. A fresh
installed application must render/save its synthetic note and produce native
screenshots using the packaged runtime. The SDK is restored on success or
failure; collisions preserve both directories and fail the job. Publication
additionally requires `developerQtExcluded: true` in the macOS evidence.

No platform, package or native verification environment is removed. Linux
archive, both Ubuntu DEB and both Fedora RPM checks remain independent jobs.
All existing missing-package, screenshot, lifecycle and checksum gates remain.

## Upgrading and verification

Save notes and use **File > Quit** before installing; closing the main window
can leave Nexus in the tray. The database schema remains v10.

Publication requires editor reproducibility, all eleven CTest suites on all
three platforms, and all six native installer environments. Fifty assistant
cases cover matching, editing, manual setup, consent, cancellation, limits,
navigation, stale destinations and atomic rollback. SDK-isolation tests use
temporary synthetic directories, never a developer's real Qt installation.
AI tests use a loopback fake service; real-provider matching quality is not certified.

This version supersedes the tagged but unpublished v1.0.18, whose tag is
preserved unchanged. The six packages, six environments' screenshots/evidence
and checksums total 25 release assets. macOS ARM64 remains unsigned/unnotarized;
DEBs are verified on Ubuntu 22.04/24.04 and RPMs in Fedora 43/44 containers.
Intel macOS, Linux ARM64, AppImage and native Wayland hotkeys are not added.
Publication targets jelllove/Nexus; mirror synchronization remains separate.
