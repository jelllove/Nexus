# Nexus v1.0.13

## Download and install

**[Windows x64 installer (EXE)](https://github.com/jelllove/Nexus/releases/download/v1.0.13/Nexus-Setup-v1.0.13-x64.exe)**

| Platform | Download |
| --- | --- |
| Windows x64 portable | [ZIP](https://github.com/jelllove/Nexus/releases/download/v1.0.13/Nexus-1.0.13-Windows-x64.zip) |
| macOS Apple Silicon | [DMG](https://github.com/jelllove/Nexus/releases/download/v1.0.13/Nexus-1.0.13-macOS-arm64.dmg) |
| Ubuntu 22.04/24.04 x64 | [DEB](https://github.com/jelllove/Nexus/releases/download/v1.0.13/Nexus-1.0.13-Linux-x64.deb) |
| Fedora 43/44 x64 | [RPM](https://github.com/jelllove/Nexus/releases/download/v1.0.13/Nexus-1.0.13-Linux-x64.rpm) |
| Linux x64 portable | [tar.gz](https://github.com/jelllove/Nexus/releases/download/v1.0.13/Nexus-1.0.13-Linux-x64.tar.gz) |

Ordinary users only need the package for their operating system. The PNGs and
installer-evidence ZIPs are verification materials, not additional software
installers. [SHA256SUMS](https://github.com/jelllove/Nexus/releases/download/v1.0.13/SHA256SUMS)
contains the published file checksums.

## Persistent database write history

- Records row-level creation, modification and deletion for products, tasks,
  subtasks, content snapshots and settings, including state transitions,
  ordering, subtask movement, restoration and cascading deletes.
- Stores UTC timestamps with millisecond precision, record IDs, changed field
  names and previous/current statuses. Later edits do not overwrite earlier
  events; rollback also rolls back the associated history.
- Does not copy titles, note bodies or setting values such as API keys into
  the log. No query/search trace or maintenance-operation audit is added.
- Existing databases upgrade to schema v10 on opening. Past operations cannot
  be reconstructed. History has no automatic expiry and grows with writes.

## AI activity summaries by time

- Open **File > AI Activity Summary by Time...** and choose today, one week,
  two weeks, one month, three months or custom inclusive local calendar dates.
- Filter by creation, modification or either. Includes all products' main
  tasks and subtasks, including archived and recycled records. Retained
  in-range modification events still match after a later edit.
- Preview the material locally first. Sending requires explicit confirmation
  of your configured AI host/model; declining is the default.
- Reuses your existing AI endpoint/key/model. The input includes matched
  task text and activity metadata, not settings values or attachments.
- Results open as Markdown with copy and save actions, without overwriting
  tasks. Requests support cancellation and a 60-second deadline.
- Content reflects current saved notes, not historical wording. Missing
  pre-upgrade history and permanently deleted content are explicitly
  acknowledged. Large inputs are rejected rather than silently truncated.

## Upgrading and verification

Save your work, quit Nexus and install the matching package. Back up important
notes before upgrading; the existing backup mechanism includes operation history.
The Windows updater reliability fixes from v1.0.12 are retained.

Publication requires editor validation, all ten CTest suites on Windows,
macOS and Linux, and all six macOS/Linux native installer environments.
Tests use synthetic data and a local fake AI service, not personal notes or
real AI credentials. Native screenshots, evidence archives and checksums
remain part of this release.

The macOS ARM64 package remains unsigned/unnotarized. Linux packages are
unsigned and are not an apt/yum repository. Intel macOS, Linux ARM64,
AppImage and native Wayland global hotkeys are not added in this release.
This publication targets the primary jelllove/Nexus repository; mirror
synchronization is a separate operation.
