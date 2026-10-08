# Nexus v1.0.10

## Native Linux installation

- Adds x64 DEB and RPM packages alongside the portable tar.gz.
- DEB lifecycle verification runs on Ubuntu 22.04 and 24.04.
- RPM lifecycle verification runs in isolated Fedora 43 and 44 environments.
- Installs the private Qt runtime under `/opt/nexus`, exposes `nexus` on PATH,
  and adds a system desktop entry and icon.
- Runtime dependencies are installed through apt/dnf. Reinstallation and
  uninstallation are verified; user notes/settings are never package-owned.
- Each native-package job launches as an unprivileged user, renders/saves a
  synthetic note, and captures app-window and desktop screenshots.

Install with `sudo apt install ./Nexus-1.0.10-Linux-x64.deb` or
`sudo dnf install ./Nexus-1.0.10-Linux-x64.rpm`. Quit Nexus before installing an
upgrade. Remove with `sudo apt remove nexus` or `sudo dnf remove nexus`.

Windows x64 and macOS Apple Silicon packages remain available. Checksums and
installer evidence are included in the release. No apt/yum repository or package
signature is provided; only install trusted downloads. Rocky/RHEL, Debian and
other unverified distributions are not claimed compatible. Fedora captures use
an X11 virtual desktop in a container, not a physical desktop session.
macOS remains unsigned/unnotarized; Wayland global hotkeys remain unsupported.
