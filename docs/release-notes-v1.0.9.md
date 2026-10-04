# Nexus v1.0.9

## Cross-platform desktop support

- Windows x64 installer and portable ZIP, macOS Apple Silicon DMG, and Linux
  x64 tar.gz include the Qt runtime, plugins and WebEngine resources.
- Native Control+Shift+N hotkeys use Win32, macOS Carbon or Linux X11.
  Wayland global hotkeys are not supported; normal app use remains available.
- Closing saves and exits when no system tray exists. macOS Dock activation
  can restore a hidden window.
- Updates select packages matching the OS and CPU. Windows launches its
  installer; macOS/Linux open downloaded packages for manual installation.

## Verification and screenshots

Release publication requires successful builds/tests and installed-package
checks on macOS and Linux. Those jobs render and save synthetic notes and
capture app-window and desktop screenshots. Release assets include the PNGs,
diagnostic evidence ZIPs and SHA256SUMS.

macOS packages are not Developer ID signed/notarized. Linux packages use
Ubuntu 22.04 as the reference platform, require compatible system libraries
and are not universal AppImages or DEB/RPM installers. Linux screenshots use
an Xvfb desktop. See the README for installation instructions and limitations.
