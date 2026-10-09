---
name: nexus-release-authoring
description: Use for Nexus release authoring, push and release requests, version tags, packaging or release CI changes. Requires verified Windows, macOS and Linux packages in every release.
---

# Nexus cross-platform release authoring

## When to use

Use this skill before preparing or publishing a Nexus release, changing the
release workflow or package matrix, or reporting a release as complete.
Follow [AGENTS.md](../../../AGENTS.md) and the
[project build and release commands](../../../README.md).

Do not treat a successful Windows build, a pushed branch or a GitHub release
page alone as a completed cross-platform release.

## Required package matrix

Each release must include these six non-empty packages built from the same
commit and application version:

| Platform | Required release assets |
| --- | --- |
| Windows x64 | `Nexus-Setup-v<version>-x64.exe`, `Nexus-<version>-Windows-x64.zip` |
| macOS Apple Silicon | `Nexus-<version>-macOS-arm64.dmg` |
| Linux x64 | `Nexus-<version>-Linux-x64.tar.gz`, `Nexus-<version>-Linux-x64.deb`, `Nexus-<version>-Linux-x64.rpm` |

DEBs are verified on Ubuntu 22.04/24.04; RPMs are verified in Fedora 43/44
containers. Do not imply support for all Linux distributions. macOS packages
are unsigned/unnotarized. Intel macOS, Linux ARM64 and AppImage are not currently
required release artifacts or verified release targets.

Do not silently remove an OS, architecture or package format to make CI pass.
An explicitly requested matrix change must update CMake packaging, build and
native verification jobs, release preparation, regression tests, updater
package selection and support documentation together.

## Existing enforcement

Use the [Build workflow](../../workflows/build.yml), including its tag-triggered
release job. [prepare_release.py](../../../ci/prepare_release.py) rejects missing
or empty required packages, mismatched versions and failed native evidence.
[ReleasePreparationTest](../../../tests/test_release_preparation.py) covers all
six missing/empty packages and checks this skill's discovery links.

Keep these gates enabled:

- Editor syntax and generated-bundle reproducibility.
- Windows, macOS and Linux builds and all configured CTest suites.
- Installed macOS DMG and Linux tar.gz launch, editor/save and screenshots.
- Ubuntu 22.04/24.04 DEB and Fedora 43/44 RPM installation, reinstallation,
  launch, integration, removal and preservation of synthetic user data.

Never replace the release workflow with `release.bat`: that legacy helper is
Windows-only. Never manually publish a partial release to bypass failed gates.

## Authoring and publication procedure

1. Inspect the target repository/worktree, dirty changes, local branches and
   remote URLs. Preserve existing work; do not reset, force-push or rewrite
   previously published tags/assets.
2. Select the GitHub account for the intended remote and verify its API login
   before fetching, pushing or reading private repository data. Verify the
   mirror's canonical repository name as well. Credential labels may be stale:
   `qinqingxu/Nexus` is the historical URL of `qinqiangxu/Nexus`. Confirm any
   redirect/account mapping; stop on an unresolved identity mismatch.
3. Fetch current main and tags, integrate incoming changes without discarding
   either side, and use an `agents/release-*` branch. Choose a new unused
   version, not a replacement for the current published release.
4. Update `project(Nexus VERSION ...)` in CMake, the installer default version,
   README package names/commands and `docs/release-notes-v<version>.md`.
   Native packaging derives its version from the root CMake project.
5. Run the documented full validation command. On Windows:

   ```powershell
   .\scripts\validate.ps1 -QtRoot C:\Qt\6.8.3\msvc2022_64
   ```

   For focused release-helper checks, use existing CTest:

   ```powershell
   ctest --test-dir build -C Release --output-on-failure --no-tests=error `
     -R "^(ReleasePreparationTest|InstallerVerificationTest|NativePackageTest)$"
   ```

   Use README platform commands on macOS/Linux. Do not claim native installer
   checks passed based only on local Windows tests.
6. Commit only intended source, tests and documentation. Push the release
   branch; wait for all required branch CI jobs to pass at its exact commit.
   Retrieve failure logs and fix/retest failures instead of weakening gates.
7. Re-fetch main and verify it can advance safely to the tested source. If it
   moved, integrate/revalidate; do not overwrite concurrent work. Fast-forward
   main and create a new annotated tag matching the CMake version. For a
   validated version `1.2.3`, the tag is `v1.2.3`, never `v1.2.4`.
8. Wait for the exact-tag workflow to rebuild, run every native gate and publish
   the primary release. Do not use a branch run as evidence that tag publication
   succeeded. Record the successful workflow URL and commit.
9. When mirror synchronization is requested, select/verify the mirror account,
   synchronize the same main/branch/tag without force and copy the primary
   release's already-verified assets. Do not rebuild separately and describe
   different bytes as identical. Preserve the mirror's existing Actions policy.
10. Verify both latest releases are published, not drafts/prereleases; compare
    asset names, sizes and SHA256 digests, main/tag commit IDs and checksums.
    Check every file listed in `SHA256SUMS`. Preserve diagnostic evidence, then
    remove only explicitly identified temporary binaries/tools.

The current matrix yields 25 release assets: six packages, app/desktop PNGs and
an evidence ZIP for each of six native environments, plus `SHA256SUMS`.
If the verified matrix is intentionally expanded, update the preparation and
tests to require the new artifacts rather than silently skipping them.

## Completion checklist

- All required OS packages and architecture names are present and non-empty.
- All packages are from the released commit/version, including the Windows
  installer generated from the CMake-deployed Qt runtime.
- Exact-tag build/test, editor and all native installer jobs passed.
- Native captures use synthetic notes; no personal database or credentials
  were uploaded or committed.
- Checksums and all screenshot/evidence assets are published.
- Requested mirrors have the same source/tag and byte-identical assets.
- README/release notes match the actual packages and verified support scope.
- The handoff includes version, release links, commit and passing CI evidence.

If a gate or requested mirror is blocked, report what remains. Never describe
a pushed branch, Windows-only artifact, partial upload or pending CI as done.

## Updater safety learned during releases

If update code is touched, preserve platform/CPU matching, download consent,
cancellation and save-before-install/open behavior. Destroy the
`QTemporaryFile` writer before emitting `downloadFinished`; `close()` alone
retains its writable handle and prevents immediate Windows execution.
Run `UpdateServiceTest`, `UpdateUiTest` and `UpdatePackageTest`, including the
native launch regression and the official manual-download fallback tests.
