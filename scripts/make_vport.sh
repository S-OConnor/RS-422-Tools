#!/usr/bin/env bash
# Create two linked pseudo-terminals so two processes can talk over a "serial"
# link with no hardware. socat prints the two /dev/pts/N paths it created; point
# the monitor at one and the sensor at the other (each via --port).
#
#   ./scripts/make_vport.sh
#   # note the two PTY paths it prints, then in two terminals (from repo root):
#   python -m apps.monitor    --port /dev/pts/<A>
#   python -m apps.sensor_sim --port /dev/pts/<B>
#
# Requires: socat, and pyserial (pip install pyserial) for --port mode.
# No-socat alternative: use --tcp on both apps instead (see monitor --help).
set -euo pipefail

if ! command -v socat >/dev/null 2>&1; then
  echo "socat not found. Install it, or use --tcp mode instead." >&2
  exit 1
fi

echo "Creating linked PTYs (Ctrl-C to tear down)..."
exec socat -d -d pty,raw,echo=0 pty,raw,echo=0
