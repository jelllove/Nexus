# Nexus v1.0.17

## Download and install

**[Windows x64 installer (EXE)](https://github.com/jelllove/Nexus/releases/download/v1.0.17/Nexus-Setup-v1.0.17-x64.exe)**

| Platform | Download |
| --- | --- |
| Windows x64 portable | [ZIP](https://github.com/jelllove/Nexus/releases/download/v1.0.17/Nexus-1.0.17-Windows-x64.zip) |
| macOS Apple Silicon | [DMG](https://github.com/jelllove/Nexus/releases/download/v1.0.17/Nexus-1.0.17-macOS-arm64.dmg) |
| Ubuntu 22.04/24.04 x64 | [DEB](https://github.com/jelllove/Nexus/releases/download/v1.0.17/Nexus-1.0.17-Linux-x64.deb) |
| Fedora 43/44 x64 | [RPM](https://github.com/jelllove/Nexus/releases/download/v1.0.17/Nexus-1.0.17-Linux-x64.rpm) |
| Linux x64 portable | [tar.gz](https://github.com/jelllove/Nexus/releases/download/v1.0.17/Nexus-1.0.17-Linux-x64.tar.gz) |

The PNGs and installer-evidence ZIPs are verification materials.
[SHA256SUMS](https://github.com/jelllove/Nexus/releases/download/v1.0.17/SHA256SUMS)
provides checksums for every other published file.

## Persistent update button

- Detecting a newer release shows a blue **Update** button. It remains available
  after **Not now**, including while automatic popup reminders are paused.
- Click **Update** to download the detected release without repeating the
  availability prompt. Existing manual/automatic checks and reminder
  preferences remain available.
- The button shows download progress and prevents duplicate downloads.
  Cancelling or a failed download restores the action so it can be retried.
- After downloading, the action becomes **Install now** on Windows or
  **Open update** on macOS/Linux. Choosing **Later** keeps that action available
  without downloading the package again during the same session.
- If the cached file is removed, it is downloaded again. Detecting a newer
  release replaces an older cached update target.
- Installation/opening still requires confirmation and saves the current
  task or subtask first. Save failures keep Nexus open.

## Windows title-bar integration

- On Windows, **Update** is immediately left of the minimize/maximize/close
  controls, on the same title-bar row rather than beside the search field.
- The caption follows the window title and icon. Its controls preserve
  minimize/maximize/restore and the existing close-to-tray/save behavior.
- Native caption and resize hit testing preserves title dragging, double-click
  maximize/restore, system-menu access and the maximize/snap hit target.
  DPI-aware edges and corners remain resizable; maximized windows respect the
  monitor work area and taskbar.
- Native system commands keep Qt window state synchronized, including restoring
  a previously maximized window after minimization.
- macOS/Linux retain their native window frames and the top-right search-header
  update button.

## Upgrading and verification

Save notes and quit Nexus before installing. The database schema remains v10;
existing notes, configuration and task states are preserved.

Publication requires editor validation, all ten CTest suites on Windows, macOS
and Linux, and all six native macOS/Linux installer environments. Update
regressions cover button visibility, paused reminders, direct downloads,
progress, cancellation, retry, cached files, newer releases, concurrent checks
and save failures. Additional Windows native caption tests cover control
placement, minimize/maximize/restore, eight resize edges/corners and taskbar
bounds. Tests and captures use synthetic notes and isolated databases.

All six packages, native screenshots/evidence and checksums are included.
macOS ARM64 remains unsigned/unnotarized. Linux DEBs are verified on Ubuntu
22.04/24.04 and RPMs in Fedora 43/44 containers; these are unsigned packages,
not an apt/yum repository. Intel macOS, Linux ARM64, AppImage and native Wayland
global hotkeys are not added. This publication targets jelllove/Nexus;
private-mirror synchronization is separate.
