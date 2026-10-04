"""Collect validated package and screenshot assets for a matching version tag."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys
import zipfile

from verify_installer import validate_evidence


def prepare_release(source, output, tag):
    root = Path(__file__).resolve().parents[1]
    match = re.search(r"project\(Nexus VERSION ([0-9.]+)", (root / "CMakeLists.txt").read_text())
    if not match or tag != "v" + match[1]:
        raise ValueError("Release tag must match the CMake application version.")
    version = match[1]
    expected = {
        "Windows": [f"Nexus-Setup-v{version}-x64.exe", f"Nexus-{version}-Windows-x64.zip"],
        "macOS": [f"Nexus-{version}-macOS-arm64.dmg"],
        "Linux": [f"Nexus-{version}-Linux-x64.tar.gz"],
    }
    packages = []
    for system, names in expected.items():
        for name in names:
            package = source / f"Nexus-{system}" / name
            if not package.is_file() or package.stat().st_size == 0:
                raise ValueError(f"Required release package is missing/empty: {name}")
            packages.append(package)
    for system in ("macOS", "Linux"):
        evidence = source / f"Nexus-{system}-installer-evidence"
        report = json.loads((evidence / "verification.json").read_text())
        if report.get("status") != "passed":
            raise ValueError(f"{system} installed-package verification did not pass.")
        validate_evidence(evidence / "screenshots", Path(report["installedExecutable"]))
    output.mkdir(parents=True, exist_ok=False)
    for package in packages:
        shutil.copy2(package, output / package.name)
    for system in ("macOS", "Linux"):
        evidence = source / f"Nexus-{system}-installer-evidence"
        for name in ("app-window.png", "desktop.png"):
            shutil.copy2(evidence / "screenshots" / name, output / f"Nexus-{system}-{name}")
        with zipfile.ZipFile(output / f"Nexus-{system}-installer-evidence.zip", "w",
                             compression=zipfile.ZIP_DEFLATED) as archive:
            for path in evidence.rglob("*"):
                if path.is_file() and path.suffix in (".png", ".json", ".log"):
                    archive.write(path, path.relative_to(evidence))
    checksums = []
    for path in sorted(output.iterdir()):
        with path.open("rb") as file:
            digest = hashlib.file_digest(file, "sha256").hexdigest()
        checksums.append(f"{digest}  {path.name}")
    (output / "SHA256SUMS").write_text("\n".join(checksums) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--tag", required=True)
    args = parser.parse_args()
    try:
        prepare_release(args.input, args.output, args.tag)
    except (OSError, ValueError, KeyError) as error:
        print(f"Release preparation failed: {error}", file=sys.stderr)
        return 1
    print(f"Release assets prepared in {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
