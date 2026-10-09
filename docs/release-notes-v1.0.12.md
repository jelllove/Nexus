# Nexus v1.0.12

## Windows update launch fix

- Releases the downloaded installer's writable file handle before notifying
  the UI, so choosing Install now immediately after downloading no longer fails
  because Nexus itself still has the executable open.
- Keeps unique temporary filenames, cached downloads, cancellation and
  save-before-install behavior.
- Adds a Windows regression that downloads and launches a harmless test
  executable synchronously from the completion notification, without running
  a real installer or accessing personal notes.

## Manual download fallback

- Automatically opens the [official Nexus download website](https://www.jelllove.com/products/nexus.html)
  when a requested update download cannot complete/save or the downloaded
  installer/package cannot be launched/opened.
- Keeps Nexus running and retains the original error. The warning displays a
  selectable download URL; failure to open the browser is also reported.
- Does not open a browser when users cancel or defer updates, a background
  update check fails, or unsaved notes block installation.
- This handles download and launch failures, not failures inside an external
  installer after it has successfully started.

## Upgrading from earlier versions

The fix is in the new application, so an older updater may still hit its
file-lock issue while installing v1.0.12. Choose Later after downloading,
then Help > Check for Updates > Install now, or save your notes, quit Nexus
and run the trusted downloaded installer manually. Renaming the file or
running the whole application as administrator is not needed for this issue.

## Packages and verification

Windows x64 installer/portable ZIP, macOS Apple Silicon DMG and Linux x64
tar.gz/DEB/RPM remain available. DEBs are verified on Ubuntu 22.04/24.04;
RPMs are verified in Fedora 43/44 containers. Quit Nexus before upgrading.

Publication requires editor validation, all nine CTest suites on each platform
and all six macOS/Linux native installer environments. Native screenshots,
evidence archives and SHA256SUMS accompany the release. Both repositories
receive the same verified commit/tag/assets; the private mirror is now named
qinqiangxu/Nexus, with the old qinqingxu/Nexus URL redirecting to it.

Existing limitations remain: macOS is unsigned/unnotarized; Linux packages are
not signed or hosted in an apt/yum repository; Wayland global hotkeys, Intel
macOS release packages and AppImage are not part of this release.
