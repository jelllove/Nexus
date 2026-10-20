# Nexus v1.0.16

## Download and install

**[Windows x64 installer (EXE)](https://github.com/jelllove/Nexus/releases/download/v1.0.16/Nexus-Setup-v1.0.16-x64.exe)**

| Platform | Download |
| --- | --- |
| Windows x64 portable | [ZIP](https://github.com/jelllove/Nexus/releases/download/v1.0.16/Nexus-1.0.16-Windows-x64.zip) |
| macOS Apple Silicon | [DMG](https://github.com/jelllove/Nexus/releases/download/v1.0.16/Nexus-1.0.16-macOS-arm64.dmg) |
| Ubuntu 22.04/24.04 x64 | [DEB](https://github.com/jelllove/Nexus/releases/download/v1.0.16/Nexus-1.0.16-Linux-x64.deb) |
| Fedora 43/44 x64 | [RPM](https://github.com/jelllove/Nexus/releases/download/v1.0.16/Nexus-1.0.16-Linux-x64.rpm) |
| Linux x64 portable | [tar.gz](https://github.com/jelllove/Nexus/releases/download/v1.0.16/Nexus-1.0.16-Linux-x64.tar.gz) |

The PNGs and installer-evidence ZIPs are verification materials.
[SHA256SUMS](https://github.com/jelllove/Nexus/releases/download/v1.0.16/SHA256SUMS)
provides checksums for every other published file.

## Show or hide completed tasks

- Use **Show completed** below the Active / Archived / Deleted buttons in the
  Tasks pane. It starts checked; uncheck it to hide Completed work and check
  it again to show everything. Your choice is remembered across products,
  lifecycle views, search results and restarts.
- A completed main task hides its entire subtree, including unfinished
  children. Under unfinished main tasks, completed subtasks hide individually.
  Expansion state and visible selections are preserved.
- Marking a task Completed while completed items are hidden immediately removes
  it from the list. Pending note edits are saved before an affected editor is
  cleared. Save or status-write failures keep the note visible.
- Search counts distinguish visible hits from hidden completed tasks. If all
  matching tasks are completed, the list explains how to show them again.
- Drag/drop and menu moves account for hidden siblings. Moving into a
  completed parent enables Show completed so the destination stays visible.
- Hiding does not archive or delete tasks. Export/preview and AI-summary
  selection keep their existing independent scope.

## Additional fixes

- Keep hidden tasks in their proper positions when reordering visible tasks.
- Preserve unrelated product/lifecycle groups while reordering search results,
  and immediately show the resulting order.
- Clear deferred editor content when hiding a note before the editor is ready,
  preventing that note from reappearing after initialization.

## Upgrading and verification

Save notes and quit Nexus before installing. The database schema remains v10;
existing notes, configuration and previous completion statuses are preserved.

Publication requires editor validation, all ten CTest suites on Windows, macOS
and Linux, and all six native macOS/Linux installer environments. Regression
coverage includes five work statuses, subtree visibility, saved preferences,
search feedback, selection, pending saves and write failures, and filtered
movement/reordering. Tests use synthetic notes and isolated databases.

All six packages, native screenshots/evidence and checksums are included.
macOS ARM64 remains unsigned/unnotarized. Linux DEBs are verified on Ubuntu
22.04/24.04 and RPMs in Fedora 43/44 containers; these are unsigned packages,
not an apt/yum repository. Intel macOS, Linux ARM64, AppImage and native Wayland
global hotkeys are not added. This publication targets jelllove/Nexus;
private-mirror synchronization is separate.
