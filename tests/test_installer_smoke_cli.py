"""Negative CLI tests do not initialize WebEngine or access a user's database."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


EXECUTABLE = Path(sys.argv.pop(1)).resolve()


class InstallerSmokeCliTest(unittest.TestCase):
    def run_smoke(self, *arguments):
        environment = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_LOGGING_TO_CONSOLE="1")
        return subprocess.run(
            [str(EXECUTABLE), "--installer-smoke", *arguments],
            env=environment, capture_output=True, timeout=15, check=False,
        )

    def test_missing_output_is_rejected(self):
        result = self.run_smoke()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"Usage: Nexus --installer-smoke", result.stdout + result.stderr)

    def test_extra_argument_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.run_smoke(str(Path(directory) / "new"), "unexpected")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"Usage: Nexus --installer-smoke", result.stdout + result.stderr)
            self.assertFalse((Path(directory) / "new").exists())

    def test_existing_output_directory_is_untouched(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sentinel = root / "nexus.db"
            sentinel.write_bytes(b"Do not overwrite existing data")
            result = self.run_smoke(str(root))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"must not already exist", result.stdout + result.stderr)
            self.assertEqual(sentinel.read_bytes(), b"Do not overwrite existing data")
            self.assertEqual(list(root.iterdir()), [sentinel])


if __name__ == "__main__":
    unittest.main()
