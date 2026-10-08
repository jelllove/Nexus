import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "ci"))
SPEC = importlib.util.spec_from_file_location("prepare_release", ROOT / "ci" / "prepare_release.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
VERSION = re.search(r"project\(Nexus VERSION ([0-9.]+)", (ROOT / "CMakeLists.txt").read_text())[1]


def sample_png():
    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data
                + struct.pack(">I", zlib.crc32(kind + data)))
    header = struct.pack(">IIBBBBB", 800, 500, 8, 2, 0, 0, 0)
    pixels = (b"\0" + b"\xff\xff\xff" * 400 + b"\0\0\0" * 400) * 500
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
            + chunk(b"IDAT", zlib.compress(pixels)) + chunk(b"IEND", b""))


class ReleasePreparationTest(unittest.TestCase):
    def create_input(self, root):
        names = {
            "Windows": [f"Nexus-Setup-v{VERSION}-x64.exe", f"Nexus-{VERSION}-Windows-x64.zip"],
            "macOS": [f"Nexus-{VERSION}-macOS-arm64.dmg"],
            "Linux": [f"Nexus-{VERSION}-Linux-x64.tar.gz",
                      f"Nexus-{VERSION}-Linux-x64.deb", f"Nexus-{VERSION}-Linux-x64.rpm"],
        }
        for system, packages in names.items():
            directory = root / f"Nexus-{system}"
            directory.mkdir()
            for name in packages:
                (directory / name).write_bytes(b"synthetic package fixture")
        for system in ("macOS", "Linux", "Ubuntu-22.04-DEB", "Ubuntu-24.04-DEB",
                       "Fedora-43-RPM", "Fedora-44-RPM"):
            evidence = root / f"Nexus-{system}-installer-evidence"
            screenshots = evidence / "screenshots"
            screenshots.mkdir(parents=True)
            executable = str(root / "installed" / system / "Nexus")
            (evidence / "verification.json").write_text(json.dumps({
                "status": "passed", "installedExecutable": executable,
                "packageInstalled": True, "packageReinstalled": True,
                "packageRemoved": True, "userDataPreserved": True,
                "systemIntegration": True,
            }))
            (screenshots / "report.json").write_text(json.dumps({
                "status": "passed", "executable": executable,
                "editorRendered": True, "databaseSaved": True,
            }))
            for name in ("app-window.png", "desktop.png"):
                (screenshots / name).write_bytes(sample_png())

    def test_complete_release_has_packages_evidence_and_checksums(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.create_input(root)
            output = root / "release"
            MODULE.prepare_release(root, output, "v" + VERSION)
            self.assertEqual(len(list(output.iterdir())), 25)
            checksums = (output / "SHA256SUMS").read_text().splitlines()
            self.assertEqual(len(checksums), 24)
            for line in checksums:
                digest, name = line.split("  ", 1)
                self.assertEqual(hashlib.sha256((output / name).read_bytes()).hexdigest(), digest)

    def test_missing_platform_package_blocks_publication(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.create_input(root)
            (root / "Nexus-Linux" / f"Nexus-{VERSION}-Linux-x64.tar.gz").unlink()
            with self.assertRaisesRegex(ValueError, "Required release package"):
                MODULE.prepare_release(root, root / "release", "v" + VERSION)
            self.assertFalse((root / "release").exists())

    def test_failed_native_verification_blocks_publication(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.create_input(root)
            (root / "Nexus-macOS-installer-evidence" / "verification.json").write_text(
                '{"status":"failed"}'
            )
            with self.assertRaisesRegex(ValueError, "verification did not pass"):
                MODULE.prepare_release(root, root / "release", "v" + VERSION)
            self.assertFalse((root / "release").exists())

    def test_tag_version_mismatch_blocks_publication(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaisesRegex(ValueError, "tag"):
                MODULE.prepare_release(root, root / "release", "v0.0.0")

    def test_missing_native_package_blocks_publication(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.create_input(root)
            (root / "Nexus-Linux" / f"Nexus-{VERSION}-Linux-x64.deb").unlink()
            with self.assertRaisesRegex(ValueError, "Required release package"):
                MODULE.prepare_release(root, root / "release", "v" + VERSION)

    def test_failed_package_removal_blocks_publication(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.create_input(root)
            report = root / "Nexus-Fedora-44-RPM-installer-evidence" / "verification.json"
            data = json.loads(report.read_text())
            data["packageRemoved"] = False
            report.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, "lifecycle"):
                MODULE.prepare_release(root, root / "release", "v" + VERSION)


if __name__ == "__main__":
    unittest.main()
