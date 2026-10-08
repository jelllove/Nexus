import importlib.util
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "ci"))
SPEC = importlib.util.spec_from_file_location(
    "verify_native_package", ROOT / "ci/verify_native_package.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class NativePackageTest(unittest.TestCase):
    def test_deb_uses_package_manager_and_named_removal(self):
        package = Path("/tmp/native package.deb")
        install, reinstall, remove, query = MODULE.manager_commands(package)
        self.assertEqual(install, ["sudo", "-n", "apt-get", "install", "-y", str(package)])
        self.assertIn("--reinstall", reinstall)
        self.assertEqual(remove, ["sudo", "-n", "apt-get", "remove", "-y", "nexus"])
        self.assertEqual(query[-1], "nexus")

    def test_rpm_uses_dnf_with_local_package(self):
        package = Path("/tmp/native package.rpm")
        install, reinstall, remove, query = MODULE.manager_commands(package)
        self.assertEqual(install, ["sudo", "-n", "dnf", "install", "-y", str(package)])
        self.assertEqual(reinstall, ["sudo", "-n", "dnf", "reinstall", "-y", str(package)])
        self.assertEqual(remove, ["sudo", "-n", "dnf", "remove", "-y", "nexus"])
        self.assertEqual(query[-1], "nexus")

    def test_archive_is_not_silently_treated_as_native_package(self):
        with self.assertRaisesRegex(ValueError, ".deb or .rpm"):
            MODULE.manager_commands(Path("/tmp/nexus.tar.gz"))


if __name__ == "__main__":
    unittest.main()
