# Nexus v1.0.15

## Download and install

**[Windows x64 installer (EXE)](https://github.com/jelllove/Nexus/releases/download/v1.0.15/Nexus-Setup-v1.0.15-x64.exe)**

| Platform | Download |
| --- | --- |
| Windows x64 portable | [ZIP](https://github.com/jelllove/Nexus/releases/download/v1.0.15/Nexus-1.0.15-Windows-x64.zip) |
| macOS Apple Silicon | [DMG](https://github.com/jelllove/Nexus/releases/download/v1.0.15/Nexus-1.0.15-macOS-arm64.dmg) |
| Ubuntu 22.04/24.04 x64 | [DEB](https://github.com/jelllove/Nexus/releases/download/v1.0.15/Nexus-1.0.15-Linux-x64.deb) |
| Fedora 43/44 x64 | [RPM](https://github.com/jelllove/Nexus/releases/download/v1.0.15/Nexus-1.0.15-Linux-x64.rpm) |
| Linux x64 portable | [tar.gz](https://github.com/jelllove/Nexus/releases/download/v1.0.15/Nexus-1.0.15-Linux-x64.tar.gz) |

The PNGs and installer-evidence ZIPs are verification materials.
[SHA256SUMS](https://github.com/jelllove/Nexus/releases/download/v1.0.15/SHA256SUMS)
provides checksums for every other published file.

## Preview hyperlink destinations

- Hover over a hyperlink in task or subtask content to see its destination in
  the bottom status bar, including links containing bold or other formatted text.
- Moving away or switching notes clears the preview. A newer operational
  status message is preserved when the previous hover ends.
- Hovering uses editable-content DOM events through the editor bridge; it does
  not fetch the destination or open a browser.

## Insert and edit hyperlinks

- Select text or place the cursor, then choose **Insert > Link** in the editor
  ribbon or press **Ctrl+K** (**Command+K** on macOS).
- Set the address and display text in the dialog. Place the cursor within an
  existing link to edit its address or label, or choose **Remove Link** to keep
  its text without the hyperlink. Cancel and Escape leave content unchanged.
- Applying or changing only an address preserves the selected text formatting.
  Link changes use the existing note autosave and persistence.
- Web, email, phone and FTP(S) addresses are supported. A leading `www.` becomes
  HTTPS; invalid, executable and data addresses are rejected without changing
  the note. Ctrl/Command-click continues to open links externally.

## Upgrading and verification

Save your notes and quit Nexus before installing. The database schema remains
v10, and existing notes and AI configuration are preserved.

Publication requires editor validation, all ten CTest suites on Windows, macOS
and Linux, and all six native macOS/Linux installer environments. Editor tests
exercise rendered DOM and mouse hover, insertion, editing, removal, shortcuts,
cancellation, URL validation, formatting and saving using synthetic notes.
Offscreen rendered-editor tests use software rendering.

All six packages, native screenshots/evidence and checksums are included.
macOS ARM64 remains unsigned/unnotarized. Linux DEBs are verified on Ubuntu
22.04/24.04 and RPMs in Fedora 43/44 containers; these are unsigned packages,
not an apt/yum repository. Intel macOS, Linux ARM64, AppImage and native Wayland
global hotkeys are not added. This publication targets jelllove/Nexus;
private-mirror synchronization is separate.
