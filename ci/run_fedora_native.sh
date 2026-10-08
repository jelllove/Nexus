#!/bin/bash
set -euo pipefail
package=$1
evidence=$2
dnf install -y python3 sudo xorg-x11-server-Xvfb xorg-x11-xauth \
  fluxbox dbus-daemon dejavu-sans-fonts
useradd --create-home --uid "$(stat -c %u /workspace)" nexus-ci
printf 'nexus-ci ALL=(ALL) NOPASSWD: ALL\n' > /etc/sudoers.d/nexus-ci
chmod 0440 /etc/sudoers.d/nexus-ci
Xvfb :99 -screen 0 1600x1000x24 > /tmp/nexus-xvfb.log 2>&1 &
xvfb_pid=$!
trap 'kill "$xvfb_pid" 2>/dev/null || true' EXIT
runuser -u nexus-ci -- env DISPLAY=:99 dbus-run-session -- \
  bash -c 'set -euo pipefail
    fluxbox > /tmp/nexus-window-manager.log 2>&1 &
    wm_pid=$!
    trap "kill $wm_pid 2>/dev/null || true" EXIT
    python3 ci/verify_native_package.py --package "$1" --evidence "$2"' \
  bash "$package" "$evidence"
