# Nexus project instructions

Follow [AGENTS.md](../AGENTS.md) for implementation, validation and data safety.

## Mandatory cross-platform release authoring

Before creating a release, publishing a version tag, modifying packaging or
changing release CI, invoke the project skill at
`.github/skills/nexus-release-authoring/SKILL.md`
([release-authoring skill](skills/nexus-release-authoring/SKILL.md)).
If the agent cannot invoke repository skills, read and follow that file directly.

Every release must retain the verified Windows, macOS and Linux package matrix.
Use the existing Build workflow and release-preparation checks; do not publish
only the Windows installer, use `release.bat` as a cross-platform replacement,
disable an OS job or weaken a missing-package/native-evidence gate.

Require successful exact-tag CI, complete packages, native evidence and
checksums before reporting publication complete. Keep documentation aligned
with the actual OS/architecture support; proposals for Intel macOS or AppImage
do not mean those packages already exist.

Select and verify the correct GitHub account for each remote. Synchronize only
requested mirrors, using the same verified commit/tag/assets without force.
Never create a release or push merely because this skill exists: obtain an
explicit user request and preserve unrelated changes.
