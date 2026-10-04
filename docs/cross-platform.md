# Cross-platform support

## Goals and scope

Nexus retains one Qt 6/C++17 UI and SQLite data model on Windows, macOS and
Linux. This extends an existing project; it does not create a new application.
The selected scope is an implementation assumption made while the user was
unavailable, not an explicit approval of feature parity.

## Platform behavior

- Windows uses MSVC, the existing executable installer, and Win32 global hotkeys.
- macOS uses Clang, an application bundle/disk image, and Carbon global hotkeys.
- Linux uses GCC or Clang, an installed archive, and X11 global hotkeys.
- The default global shortcut is Control+Shift+N on all three platforms.
  Native Wayland global shortcuts are outside this scope. Their unavailability
  must be visible in Settings and logged, without preventing normal app use.
- Closing hides the window only when a system tray is available. Otherwise
  closing saves the active note and exits. Save failure leaves the window open.
- Updates select packages matching the running OS and CPU architecture.
  Windows preserves its save/launch/quit installer flow. macOS opens the disk
  image; Linux opens the containing folder for manual extraction/installation.
  Neither non-Windows flow overwrites a running app or quits automatically.
- Missing packages, invalid URLs, registration errors and failed package opens
  must be reported, not treated as success.

## Architecture and data flow

`src/platform/GlobalHotkey.*` owns native shortcut registration and teardown.
Platform-specific dependencies are linked consistently to the application and
UI test executables. `src/platform/UpdatePackage.*` provides deterministic
package naming/selection by OS and architecture, independent of the host used
to run unit tests. `UpdateService` retains its timer, request exclusion,
download cache and notifications, delegating package selection to this helper.
`MainWindow` owns user confirmation, saving notes and opening an update.
`SettingsDialog` describes the actual platform capabilities.

Application data continues to use Qt's standard writable application-data
directory, and existing custom database paths remain supported.

## Build and deployment

CMake installs the application and deploys its Qt runtime using Qt's deployment
API. CPack creates a macOS DMG, Linux tar.gz or Windows ZIP. Linux includes a
launcher that resolves the installed Qt libraries relative to the executable.
Linux packages still depend on host system libraries; they are not universal
AppImages. Windows release installers remain the existing release mechanism.
macOS signing/notarization and Linux package-manager integration are not added.

The CI workflow builds, tests and packages on Windows, macOS and Ubuntu.
No successful native macOS/Linux run is assumed from Windows-only validation.

Release tags matching the CMake version additionally require all build/test
and installed-package jobs to pass before publication. Windows installers are
compiled from the CMake-deployed runtime, preserving the existing root-executable
layout so upgrades replace the previous executable and keep shortcuts valid.
Release assets include checksums, native PNGs and diagnostic ZIPs.
The primary repository publishes verified packages; a private qinqingxu
mirror receives the same code/tag and assets without duplicate CI execution.

## Validation evidence

Local Windows validation used MSVC and Qt 6.8.3:

- The application and all four Qt test suites built successfully.
- `ctest --test-dir build -C Release --output-on-failure` passed all four suites.
- Package-selection tests passed 18 cases, including cross-platform and CPU
  mismatch cases; the UI suite passed 11 cases with one non-Windows-only skip.
- CPack produced `Nexus-1.0.8-Windows-x64.zip`. Its contents were checked for the
  application, Qt/MSVC libraries, SQLite/TLS/platform plugins, WebEngine process,
  locale files and resources.
- The UI suite also passed using the deployed package libraries, with the
  development Qt directory removed from `PATH`.
- The Linux launcher passed a shell syntax check.

macOS/Linux native builds, native hotkey behavior and desktop smoke tests have
not been run locally. There is no native Linux development environment or macOS
host available here. The configured CI matrix is the next verification gate;
it has not been triggered or reported as successful.

## Acceptance criteria and verification

1. The application and existing tests build with Qt 6.8.3 on the three runners.
2. Package-selection tests cover Windows, macOS, Linux, CPU mismatches,
   universal macOS packages and unsupported architectures.
3. Downloaded files keep their native package extension. The updater rejects
   packages for other systems before starting a network request.
4. UI tests verify existing deferred-update/save-failure behavior and closing
   without a tray.
5. The ordinary validation command is
   `ctest --test-dir build -C Release --output-on-failure`, after configuration
   and `cmake --build build --config Release`.
6. Packaging uses `cpack --config build/CPackConfig.cmake -C Release`.
   Platform-native runtime/hotkey checks require a real desktop session.

## Installed-package CI and screenshots

The selected design extends CI with fresh macOS/Linux jobs that download the
generated packages, rather than launching an executable from the build tree.
This scope was selected autonomously while the user was unavailable.

macOS mounts the DMG read-only, copies Nexus.app into a temporary installation
directory, unmounts the image, and launches the copied executable. Linux extracts
the tar.gz into a temporary installation directory and uses its launcher under
Xvfb with a window manager. The verification environment excludes development
Qt paths and loader/plugin overrides.

An explicit `--installer-smoke <new-output-directory>` mode isolates settings,
WebEngine data and the database, disables update checks, creates synthetic
products/tasks/subtasks, and selects an example note. It waits for actual editor
DOM content, performs and verifies a save, then captures application-window and
desktop PNGs and a JSON report. A bounded timeout, failed render/save, missing
screenshots or invalid images fail the job. Normal startup is unchanged.
Existing output directories are refused to avoid overwriting user data.

CI uploads screenshots, JSON reports, install details and process logs even
when verification fails. These artifacts are retained for 14 days and linked
from the workflow summary. This tests DMG-copy and archive-extraction workflows,
not Gatekeeper dialogs, notarization, DEB/RPM installation or native hotkeys.
Native screenshot availability is itself required; blank captures must fail
rather than be presented as evidence of a successful launch.

Local validation of this pipeline extension succeeded on Windows: the newly
packaged application rendered the synthetic note, saved/read it back, produced
both PNGs, and exited successfully with only its deployed runtime available.
The app-window screenshot was inspected and shows the three-pane UI and editor
content. CLI checks reject missing/extra arguments and existing output directories
without changing their database. Native macOS/Linux screenshot jobs are configured
but have not yet run; local Windows screenshots are not evidence of those platforms.

## Non-goals and limitations

There is no new UI design, database migration, automated replacement of a
running non-Windows executable, native Wayland portal shortcut implementation,
or claim of compatibility with every Linux distribution. Release assets must
be published with the documented package names for in-app updates to find them.
