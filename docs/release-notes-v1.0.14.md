# Nexus v1.0.14

## Download and install

**[Windows x64 installer (EXE)](https://github.com/jelllove/Nexus/releases/download/v1.0.14/Nexus-Setup-v1.0.14-x64.exe)**

| Platform | Download |
| --- | --- |
| Windows x64 portable | [ZIP](https://github.com/jelllove/Nexus/releases/download/v1.0.14/Nexus-1.0.14-Windows-x64.zip) |
| macOS Apple Silicon | [DMG](https://github.com/jelllove/Nexus/releases/download/v1.0.14/Nexus-1.0.14-macOS-arm64.dmg) |
| Ubuntu 22.04/24.04 x64 | [DEB](https://github.com/jelllove/Nexus/releases/download/v1.0.14/Nexus-1.0.14-Linux-x64.deb) |
| Fedora 43/44 x64 | [RPM](https://github.com/jelllove/Nexus/releases/download/v1.0.14/Nexus-1.0.14-Linux-x64.rpm) |
| Linux x64 portable | [tar.gz](https://github.com/jelllove/Nexus/releases/download/v1.0.14/Nexus-1.0.14-Linux-x64.tar.gz) |

The PNGs and installer-evidence ZIPs are verification materials, not extra
installers. [SHA256SUMS](https://github.com/jelllove/Nexus/releases/download/v1.0.14/SHA256SUMS)
provides checksums for all published files.

## Every task's last-update time

- AI activity reports now include a fixed timestamp list covering every
  selected main task and subtask, even if the AI omits those tasks or dates.
- Each entry identifies its task type/ID, product, parent task and title,
  followed by its saved last-update time in local ISO 8601 format with UTC
  offset (`Z` for UTC). Missing dates are explicitly marked unknown.
- The list is generated from the same local preview snapshot that was sent
  to AI, not from a later database query. It is included when copying Markdown,
  copying HTML or saving the report.
- The AI prompt also explicitly requests exact timestamps for tasks it mentions.
  These are last-update values, not inferred completion dates or historical
  content snapshots; a last update can be outside the selected report period.

## English or Chinese output

- **File > AI Activity Summary by Time...** now offers
  **Summary language: English / 中文**, with **English selected by default**.
- The chosen language is explicitly passed to the configured AI service,
  independently of the language used in the notes.
- Report headings, scope/limitation text and timestamp-list labels follow the
  selected language. Original task and product names are preserved.
- Changing language invalidates the local preview so the preview and request
  remain consistent. Sending still requires confirmation; cancellation and
  the request deadline are retained.

## Upgrading and verification

Save your notes and quit Nexus before installing. Database schema remains v10;
the persistent operation history introduced in v1.0.13 is preserved.
This release does not modify existing notes or AI configuration.

Publication requires editor validation, all ten CTest suites on Windows,
macOS and Linux, and all six native macOS/Linux installer environments.
New regressions use synthetic data and a local fake AI service to verify
English defaults, Chinese requests, complete timestamp output, snapshot
consistency and copy behavior without using personal notes or real API keys.

All six packages, native screenshots/evidence and checksums remain included.
macOS ARM64 remains unsigned/unnotarized. Linux DEBs are verified on Ubuntu
22.04/24.04 and RPMs in Fedora 43/44 containers; these are unsigned packages,
not an apt/yum repository. Intel macOS, Linux ARM64, AppImage and native Wayland
global hotkeys are not added. This publication targets jelllove/Nexus;
private-mirror synchronization is separate.
