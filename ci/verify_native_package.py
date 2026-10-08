"""Install, reinstall, launch and uninstall a native DEB/RPM on an isolated CI host."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from verify_installer import clean_environment, validate_evidence

APP = Path("/opt/nexus/bin/Nexus")
LAUNCHER = Path("/usr/bin/nexus")
DESKTOP = Path("/usr/share/applications/com.nexus.app.desktop")
ICON = Path("/usr/share/icons/hicolor/256x256/apps/nexus.png")


def manager_commands(package):
    if package.suffix == ".deb":
        return (["sudo", "-n", "apt-get", "install", "-y", str(package)],
                ["sudo", "-n", "apt-get", "install", "--reinstall", "-y", str(package)],
                ["sudo", "-n", "apt-get", "remove", "-y", "nexus"],
                ["dpkg-query", "-W", "-f=${Version}", "nexus"])
    if package.suffix == ".rpm":
        return (["sudo", "-n", "dnf", "install", "-y", str(package)],
                ["sudo", "-n", "dnf", "reinstall", "-y", str(package)],
                ["sudo", "-n", "dnf", "remove", "-y", "nexus"],
                ["rpm", "-q", "--queryformat", "%{VERSION}", "nexus"])
    raise ValueError("Native package must be a .deb or .rpm.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    args = parser.parse_args()
    if not sys.platform.startswith("linux") or os.geteuid() == 0:
        parser.error("Run as an unprivileged user with passwordless sudo in isolated Linux CI.")
    package = args.package.resolve(strict=True)
    evidence = args.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=False)
    status = {"status": "failed", "package": package.name,
              "installedExecutable": str(APP), "osRelease": Path("/etc/os-release").read_text()}
    installed = False
    removed = False
    try:
        install, reinstall, remove, query = manager_commands(package)
        with (evidence / "install.log").open("wb") as log:
            if APP.exists() or LAUNCHER.exists():
                raise ValueError("Native verification requires a clean host without Nexus.")
            subprocess.run(install, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=300)
            installed = True
            status["packageInstalled"] = True
            status["packageVersion"] = subprocess.check_output(query, text=True).strip()
            version = re.fullmatch(r"Nexus-([0-9.]+)-Linux-x64\.(deb|rpm)", package.name)
            if not version or status["packageVersion"] != version[1]:
                raise ValueError("Installed package metadata does not match the release version.")
            subprocess.run(reinstall, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=300)
            status["packageReinstalled"] = True
            loader_env = dict(os.environ, LD_LIBRARY_PATH="/opt/nexus/lib")
            libraries = [APP, Path("/opt/nexus/libexec/QtWebEngineProcess"),
                         *Path("/opt/nexus/plugins").rglob("*.so")]
            missing = []
            for library in libraries:
                result = subprocess.run(["ldd", str(library)], env=loader_env,
                                        capture_output=True, text=True, check=True, timeout=30)
                log.write(f"\nDependencies: {library}\n{result.stdout}{result.stderr}".encode())
                missing.extend(line.strip() for line in result.stdout.splitlines()
                               if "not found" in line)
            if missing:
                raise ValueError("Installed runtime dependencies are missing: "
                                 + "; ".join(sorted(set(missing))))
            if (not APP.is_file() or not LAUNCHER.is_file() or not ICON.is_file()
                    or not os.access(LAUNCHER, os.X_OK)
                    or "Exec=nexus\n" not in DESKTOP.read_text()
                    or not Path("/usr/share/licenses/nexus/LICENSE").is_file()):
                raise ValueError("Native launcher/menu/icon/license integration is incomplete.")
            status["systemIntegration"] = True
            with tempfile.TemporaryDirectory(prefix="nexus-native-home-") as directory:
                home = Path(directory)
                sentinel = home / ".local/share/Nexus/Nexus/preserved-note.txt"
                sentinel.parent.mkdir(parents=True)
                sentinel.write_text("User notes must survive package removal.\n")
                env = clean_environment(os.environ, "Linux")
                env.update({"HOME": str(home), "XDG_CONFIG_HOME": str(home / ".config"),
                            "XDG_DATA_HOME": str(home / ".local/share"),
                            "XDG_CACHE_HOME": str(home / ".cache"),
                            "QT_LOGGING_TO_CONSOLE": "1"})
                with (evidence / "application.log").open("wb") as app_log:
                    subprocess.run([str(LAUNCHER), "--installer-smoke",
                                    str(evidence / "screenshots")],
                                   env=env, cwd=home, stdout=app_log, stderr=subprocess.STDOUT,
                                   check=True, timeout=90)
                validate_evidence(evidence / "screenshots", APP)
                database = evidence / "screenshots/nexus.db"
                before = hashlib.sha256(database.read_bytes()).hexdigest()
                subprocess.run(remove, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=300)
                removed = True
                if any(path.exists() for path in (APP, LAUNCHER, DESKTOP, ICON)):
                    raise ValueError("Uninstall left owned application/integration files.")
                status["packageRemoved"] = True
                if (sentinel.read_text() != "User notes must survive package removal.\n"
                        or hashlib.sha256(database.read_bytes()).hexdigest() != before):
                    raise ValueError("Uninstall changed or deleted user/synthetic note data.")
                status["userDataPreserved"] = True
                status["status"] = "passed"
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        status["error"] = str(error)
        print(f"Native package verification failed: {error}", file=sys.stderr)
    finally:
        if installed and not removed:
            with (evidence / "install.log").open("ab") as log:
                cleanup = subprocess.run(remove, stdout=log, stderr=subprocess.STDOUT, timeout=300)
                if cleanup.returncode:
                    status["cleanupError"] = f"Package removal failed ({cleanup.returncode})."
                    print(status["cleanupError"], file=sys.stderr)
        (evidence / "verification.json").write_text(json.dumps(status, indent=2) + "\n")
    return 0 if status["status"] == "passed" else 1


if __name__ == "__main__":
    sys.exit(main())
