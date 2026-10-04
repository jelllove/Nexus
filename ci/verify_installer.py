"""Verify installed Nexus packages and collect GUI evidence on native CI hosts."""

import argparse
import json
import os
from pathlib import Path
import platform
import plistlib
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import zlib


def clean_environment(environment, platform):
    env = {
        key: value for key, value in environment.items()
        if not key.startswith(("QT", "QML", "DYLD_", "LD_"))
    }
    env["PATH"] = "/usr/bin:/bin" if platform == "Linux" else "/usr/bin:/bin:/usr/sbin:/sbin"
    if platform == "Linux":
        env["QT_QPA_PLATFORM"] = "xcb"
        env["QTWEBENGINE_CHROMIUM_FLAGS"] = "--disable-gpu"
        env["QT_DEBUG_PLUGINS"] = "1"
    return env


def validate_evidence(output, executable):
    report_file = output / "report.json"
    if not report_file.is_file():
        raise ValueError("Installer verification report is missing.")
    report = json.loads(report_file.read_text(encoding="utf-8"))
    if report.get("status") != "passed":
        raise ValueError(f"Installer report failed: {report.get('message')}")
    if Path(report.get("executable", "")).resolve() != executable.resolve():
        raise ValueError("Report executable is not the freshly installed application.")
    if report.get("editorRendered") is not True or report.get("databaseSaved") is not True:
        raise ValueError("Editor rendering/database save was not verified.")
    for name in ("app-window.png", "desktop.png"):
        image = output / name
        if not image.is_file():
            raise ValueError(f"Installer screenshot is missing: {name}")
        data = image.read_bytes()
        header = data[:24]
        if (len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n"
                or header[12:16] != b"IHDR"):
            raise ValueError(f"Installer screenshot is not a PNG: {name}")
        width, height = struct.unpack(">II", header[16:24])
        if width < 800 or height < 500:
            raise ValueError(f"Installer screenshot dimensions are too small: {name}")
        offset = 8
        has_pixels = False
        while offset < len(data):
            if offset + 12 > len(data):
                raise ValueError(f"Installer screenshot PNG is truncated: {name}")
            length = struct.unpack(">I", data[offset:offset + 4])[0]
            end = offset + 12 + length
            if end > len(data):
                raise ValueError(f"Installer screenshot PNG is truncated: {name}")
            kind = data[offset + 4:offset + 8]
            contents = data[offset + 8:end - 4]
            checksum = struct.unpack(">I", data[end - 4:end])[0]
            if zlib.crc32(kind + contents) != checksum:
                raise ValueError(f"Installer screenshot PNG checksum failed: {name}")
            has_pixels |= kind == b"IDAT" and bool(contents)
            offset = end
            if kind == b"IEND":
                if length != 0 or offset != len(data) or not has_pixels:
                    raise ValueError(f"Installer screenshot PNG has no complete image: {name}")
                break
        else:
            raise ValueError(f"Installer screenshot PNG is truncated: {name}")
    return report


def install_macos(package, root, log):
    mount = root / "mounted-image"
    mount.mkdir()
    result = subprocess.run(
        ["/usr/bin/hdiutil", "attach", str(package), "-readonly", "-nobrowse",
         "-mountpoint", str(mount), "-plist"],
        stdout=subprocess.PIPE, stderr=log, check=True, timeout=60,
    )
    try:
        info = plistlib.loads(result.stdout)
        log.write(json.dumps(info, default=str).encode() + b"\n")
        log.flush()
        source = mount / "Nexus.app"
        if not source.is_dir():
            raise ValueError("Mounted DMG does not contain Nexus.app.")
        bundle = root / "installed" / "Nexus.app"
        shutil.copytree(source, bundle, symlinks=True)
    finally:
        subprocess.run(
            ["/usr/bin/hdiutil", "detach", str(mount)],
            stdout=log, stderr=log, check=True, timeout=60,
        )
    executable = bundle / "Contents" / "MacOS" / "Nexus"
    if not executable.is_file():
        raise ValueError("Installed macOS bundle has no Nexus executable.")
    return executable, executable


def install_linux(package, root, log):
    install = root / "installed"
    install.mkdir()
    with tarfile.open(package, "r:gz") as archive:
        # Python's data filter refuses traversal, external symlinks and special files.
        archive.extractall(install, filter="data")
    candidates = list(install.glob("*/bin/nexus-launch"))
    if len(candidates) != 1:
        raise ValueError("Linux archive must contain exactly one bin/nexus-launch.")
    launcher = candidates[0]
    executable = launcher.with_name("Nexus")
    if not executable.is_file() or not os.access(launcher, os.X_OK):
        raise ValueError("Installed Linux executable/launcher is missing or not executable.")
    log.write(f"Extracted {package.name} into {install}\n".encode())
    log.flush()
    return launcher, executable


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    args = parser.parse_args()
    system = platform.system()
    if system not in ("Darwin", "Linux"):
        parser.error("Native installer verification requires macOS or Linux.")
    package = args.package.resolve(strict=True)
    evidence = args.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=False)
    system_name = "macOS" if system == "Darwin" else "Linux"
    status = {"status": "failed", "platform": system_name, "package": package.name}
    try:
        with tempfile.TemporaryDirectory(prefix="nexus-installed-") as directory:
            root = Path(directory)
            with (evidence / "install.log").open("wb") as log:
                launcher, executable = (
                    install_macos(package, root, log) if system == "Darwin"
                    else install_linux(package, root, log)
                )
            env = clean_environment(os.environ, system_name)
            if system == "Linux":
                home = root / "home"
                home.mkdir()
                env.update({
                    "HOME": str(home), "XDG_CONFIG_HOME": str(home / "config"),
                    "XDG_DATA_HOME": str(home / "data"), "XDG_CACHE_HOME": str(home / "cache"),
                })
            with (evidence / "application.log").open("wb") as log:
                subprocess.run(
                    [str(launcher), "--installer-smoke", str(evidence / "screenshots")],
                    cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT,
                    check=True, timeout=90,
                )
            report = validate_evidence(evidence / "screenshots", executable)
            status.update({"status": "passed", "installedExecutable": str(executable.resolve()),
                           "verification": report})
    except (OSError, ValueError, plistlib.InvalidFileException,
            subprocess.SubprocessError, tarfile.TarError) as error:
        status["error"] = str(error)
        print(f"Installer verification failed: {error}", file=sys.stderr)
    finally:
        (evidence / "verification.json").write_text(
            json.dumps(status, indent=2) + "\n", encoding="utf-8"
        )
    if status["status"] != "passed":
        return 1
    print(f"PASS: {system_name} installed package; screenshots: {evidence / 'screenshots'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
