# Working on Nexus

Nexus is a C++17/Qt 6.8.3 desktop application with an embedded TipTap editor.

## Boundaries

- `src/db/DatabaseManager.*` owns SQLite persistence, migrations and search.
- `src/models/` contains domain types and Qt list models.
- `src/ui/` and `src/app/` coordinate widgets and application behavior.
- `editor-bundle/entry.js` and `editor-bundle/build.mjs` produce
  `resources/editor/tiptap-bundle.js`. Do not manually edit that generated bundle.
- `resources/editor/editor.js` is hand-written editor integration code.

## Validation

- On Windows, run `.\scripts\validate.ps1 -QtRoot C:\Qt\6.8.3\msvc2022_64`.
  The helper detects Visual Studio 2022 CMake tools without requiring a specific
  Visual Studio edition. Node.js 22+ is required for the editor syntax checks.
- For targeted C++ changes, configure with `-DBUILD_TESTING=ON`, build
  `NexusCoreTests`, then run
  `ctest --test-dir build -C Release --output-on-failure --no-tests=error -R "^nexus-core$"`.
  Build all targets and run unfiltered CTest before release integration is complete.
- For editor changes, run `npm --prefix editor-bundle run check`. After entry
  point or dependency changes, also run `npm --prefix editor-bundle ci` and
  `npm --prefix editor-bundle run build`, and review the generated bundle diff.
- Use the commands in README.md for manual, macOS and Linux builds.
- Release tags must match the CMake version. Require editor validation, all
  three platform builds/tests and all six native installer environments before
  publishing. Mirror exactly the verified tag and assets; do not rewrite releases.

## Safety and maintenance

- Keep tests on in-memory SQLite or temporary files. Never use the default
  application database under AppData for automated tests.
- Preserve transaction ownership: a helper must not commit or roll back a
  transaction it did not start. Propagate SQL failures to callers.
- Keep active, archived and deleted task states distinct across database and
  model APIs. Add regression tests when changing persistence or model behavior.
- Do not commit API keys, local settings, personal databases or installer output.
- Do not create issues, publish releases, push branches or change GitHub policies
  without an explicit user request.
- Update README.md when changing build/test commands. Release-skill frontmatter
  and guide links are checked by `ReleasePreparationTest`; general documentation
  drift is not automatically checked.

## Release authoring

- Before preparing/publishing a release or changing packaging/release CI,
  invoke [nexus-release-authoring](.github/skills/nexus-release-authoring/SKILL.md).
  If skill invocation is unavailable, read and follow the file directly.
- Each release must include Windows x64 EXE/ZIP, macOS ARM64 DMG and Linux x64
  tar.gz/DEB/RPM from one commit/version. Keep all required native evidence and
  checksum gates; a Windows-only build is not a complete Nexus release.
- Use the tag-triggered Build workflow, not the Windows-only `release.bat`.
  Never weaken package requirements or omit an OS to get a green release.
- Explicit matrix expansions must update build/deployment, native verification,
  release preparation/tests, updater selection and support docs together.
- Report release completion only after exact-tag CI and requested mirror
  source/tag/asset parity are verified. If blocked, state the missing step.
