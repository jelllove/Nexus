# Native Linux packages

## Scope and assumptions

Release v1.0.10 adds x64 DEB and RPM assets while retaining Windows, macOS
and the portable Linux archive. Existing v1.0.9 assets/tags are not replaced.
The implementation scope was selected autonomously while the user was unavailable.
DEB targets Ubuntu 22.04/24.04; RPM targets Fedora 43/44. Other distributions,
including Debian and Rocky/RHEL, are not claimed without native verification.

## Architecture and installation

`packaging/native/CMakeLists.txt` packages the same deployed Linux runtime used
by the portable archive. It installs private application/Qt files under
`/opt/nexus`, a launcher at `/usr/bin/nexus`, a desktop entry under
`/usr/share/applications` and an icon under `/usr/share/icons`.
Qt is not installed into system library directories. Runtime dependency names
are declared separately for DEB and RPM. The Ubuntu-built binary is only
advertised on Fedora after an actual clean-host runtime check.

There are no package scripts deleting user files, no installation-time launch,
and no automatic replacement of a running app. Users should quit Nexus before
upgrading. Notes and settings remain in Qt's per-user application-data paths.
Linux in-app updates retain the archive/manual-install flow; native-package
users can download a newer DEB/RPM and upgrade with their package manager.

## Build and verification

CI deploys Qt to a staging directory, then configures the separate packaging
project with its path and the application version. CPack creates both formats.
Package files must include the app, Qt plugins/helper/resources, system launcher,
menu/icon and license without claiming ownership of shared system directories.

Clean Ubuntu hosts and isolated Fedora containers install packages with apt/dnf,
reinstall the same version to exercise replacement, launch as an unprivileged
user under Xvfb, verify the real editor/save path, capture screenshots, remove
the package and check that user data and synthetic note evidence still exist.
Reinstallation does not prove a cross-version migration; there is no prior
native Nexus package to upgrade from in this release.

Release publication requires all package lifecycle checks and existing native
checks to pass. Python tests cover release completeness and lifecycle evidence
validation; the ordinary command remains
`ctest --test-dir build -C Release --output-on-failure`.
Published checksums cover all packages and evidence assets.

## Acceptance criteria

- Package managers identify package `nexus`, version 1.0.10, amd64/x86_64.
- Install commands resolve declared runtime dependencies on each advertised OS.
- `/usr/bin/nexus` launches the deployed runtime without development Qt paths.
- The menu entry uses `Exec=nexus`, and normal uninstall removes installed files
  while preserving the user's data.
- Actual installed executable identity, rendered note, saved SQLite content,
  app-window/desktop PNGs, reinstall and removal outcomes are recorded.
- Both repositories publish identical verified package assets; screenshots
  identify the actual OS being tested, not a substituted platform.

## Limitations

Packages are not repository-signed and no apt/yum update repository is created.
Users install local files through apt/dnf. Native Wayland hotkeys remain unsupported.
Fedora container screenshots exercise X11 desktop/runtime behavior, not a physical
GNOME session or SELinux policy on an installed workstation.
