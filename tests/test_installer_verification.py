import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zlib


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "verify_installer", ROOT / "ci" / "verify_installer.py"
)


class InstallerVerificationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.module = importlib.util.module_from_spec(SPEC)
        SPEC.loader.exec_module(cls.module)

    def test_environment_removes_development_qt_overrides(self):
        env = self.module.clean_environment({
            "PATH": "/opt/Qt/bin:/usr/bin",
            "QT_PLUGIN_PATH": "/opt/Qt/plugins",
            "QML2_IMPORT_PATH": "/opt/Qt/qml",
            "LD_LIBRARY_PATH": "/opt/Qt/lib",
            "DYLD_FRAMEWORK_PATH": "/opt/Qt/lib",
            "DISPLAY": ":99",
            "DBUS_SESSION_BUS_ADDRESS": "unix:path=/tmp/session",
            "HOME": "/tmp/user",
        }, "Linux")
        self.assertEqual(env["PATH"], "/usr/bin:/bin")
        self.assertEqual(env["QT_QPA_PLATFORM"], "xcb")
        self.assertEqual(env["DISPLAY"], ":99")
        for key in ("QT_PLUGIN_PATH", "QML2_IMPORT_PATH", "LD_LIBRARY_PATH",
                    "DYLD_FRAMEWORK_PATH"):
            self.assertNotIn(key, env)

    def test_missing_report_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "report"):
                self.module.validate_evidence(Path(directory), Path("/installed/Nexus"))

    def test_wrong_executable_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "report.json").write_text(json.dumps({
                "status": "passed", "executable": "/build/Nexus",
                "editorRendered": True, "databaseSaved": True,
            }))
            with self.assertRaisesRegex(ValueError, "executable"):
                self.module.validate_evidence(path, Path("/installed/Nexus"))

    def test_small_or_missing_screenshot_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "report.json").write_text(json.dumps({
                "status": "passed", "executable": "/installed/Nexus",
                "editorRendered": True, "databaseSaved": True,
            }))
            with self.assertRaisesRegex(ValueError, "screenshot"):
                self.module.validate_evidence(path, Path("/installed/Nexus"))
            (path / "app-window.png").write_bytes(
                b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR"
                + struct.pack(">II", 10, 10) + bytes(5)
            )
            with self.assertRaisesRegex(ValueError, "dimensions"):
                self.module.validate_evidence(path, Path("/installed/Nexus"))

    def test_failed_render_report_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "report.json").write_text(json.dumps({
                "status": "passed", "executable": "/installed/Nexus",
                "editorRendered": False, "databaseSaved": True,
            }))
            with self.assertRaisesRegex(ValueError, "render"):
                self.module.validate_evidence(path, Path("/installed/Nexus"))

    def test_truncated_png_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "report.json").write_text(json.dumps({
                "status": "passed", "executable": "/installed/Nexus",
                "editorRendered": True, "databaseSaved": True,
            }))
            (path / "app-window.png").write_bytes(
                b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR"
                + struct.pack(">II", 800, 500) + bytes(5)
            )
            with self.assertRaisesRegex(ValueError, "truncated"):
                self.module.validate_evidence(path, Path("/installed/Nexus"))

    def test_real_png_evidence_passes(self):
        def chunk(kind, data):
            return (struct.pack(">I", len(data)) + kind + data
                    + struct.pack(">I", zlib.crc32(kind + data)))

        width, height = 800, 500
        header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
        pixels = (b"\0" + b"\xff\xff\xff" * (width // 2)
                  + b"\0\0\0" * (width // 2)) * height
        image = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
                 + chunk(b"IDAT", zlib.compress(pixels)) + chunk(b"IEND", b""))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "report.json").write_text(json.dumps({
                "status": "passed", "executable": "/installed/Nexus",
                "editorRendered": True, "databaseSaved": True,
            }))
            for name in ("app-window.png", "desktop.png"):
                (path / name).write_bytes(image)
            report = self.module.validate_evidence(path, Path("/installed/Nexus"))
            self.assertEqual(report["status"], "passed")

    def test_archive_traversal_is_rejected(self):
        import io
        import tarfile

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "unsafe.tar.gz"
            with tarfile.open(package, "w:gz") as archive:
                entry = tarfile.TarInfo("../../outside")
                entry.size = 1
                archive.addfile(entry, io.BytesIO(b"x"))
            with self.assertRaises(tarfile.TarError):
                self.module.install_linux(package, root, io.BytesIO())
            self.assertFalse((root / "outside").exists())

    def test_linux_package_installs_into_clean_directory(self):
        import io
        import tarfile

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "native.tar.gz"
            with tarfile.open(package, "w:gz") as archive:
                for name in ("nexus-launch", "Nexus"):
                    payload = b"#!/bin/sh\nexit 0\n"
                    entry = tarfile.TarInfo("Nexus-1.0.8-Linux-x64/bin/" + name)
                    entry.size = len(payload)
                    entry.mode = 0o755
                    archive.addfile(entry, io.BytesIO(payload))
            launcher, executable = self.module.install_linux(package, root, io.BytesIO())
            self.assertTrue(launcher.is_file())
            self.assertTrue(executable.is_file())
            self.assertEqual(executable.parent, launcher.parent)
            self.assertTrue(executable.is_relative_to(root / "installed"))


if __name__ == "__main__":
    unittest.main()
