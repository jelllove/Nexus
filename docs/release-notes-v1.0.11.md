# Nexus v1.0.11

## Core persistence and development validation

- Integrates the AI-readiness branch's code-quality improvements; this does not
  add a new AI model or chat feature.
- Task reordering checks transaction start/commit failures, rolls back its own
  failed transaction and reports failures instead of claiming a successful move.
- Reuses one task-status serializer while preserving the existing normalized
  active/archived/deleted mapping and search behavior.
- Adds core SQLite/model regression tests, a Windows validation helper and
  editor syntax/bundle reproducibility checks.

## Consent-based update reminders

- Checks on startup and every six hours while Nexus is running.
- Requires confirmation before downloading, even if automatic downloading was
  enabled in an earlier version.
- Supports postponing reminders for six hours or one, two or three calendar
  months. The deadline survives restarts; manual update checks bypass it.
- Shows download size/progress and supports cancellation and retry. Download
  and reminder-save failures are explicitly reported.
- Preserves platform-specific packages and save-before-install/open behavior:
  Windows launches its installer after confirmation; macOS opens the DMG and
  Linux opens the downloaded archive's folder without quitting automatically.
- Keeps existing Markdown export/preview behavior and includes previously
  uncommitted export implementation documents; no new export engine is added.

## Packages and release verification

Windows x64 installer/portable ZIP, macOS Apple Silicon DMG and Linux x64
tar.gz/DEB/RPM remain available. Quit Nexus before upgrading. For native Linux
installation, use `sudo apt install ./Nexus-1.0.11-Linux-x64.deb` on Ubuntu
22.04/24.04 or `sudo dnf install ./Nexus-1.0.11-Linux-x64.rpm` on Fedora 43/44.

Publication requires the editor check, nine CTest suites on each platform,
macOS/Linux archive installation checks and all four DEB/RPM package lifecycle
jobs. Version-specific screenshots, evidence ZIPs and SHA256SUMS accompany the
release. Both repositories receive the same verified commit, tag and assets;
v1.0.10 is not replaced.

The existing support limits remain: macOS is unsigned/unnotarized, Linux packages
are not signed or distributed through an apt/yum repository, Fedora captures
use a container X11 desktop, and Wayland global hotkeys are unsupported. Other
distributions/architectures are not claimed without native verification.
