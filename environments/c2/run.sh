#!/usr/bin/env bash
# Command & control test environment: c2 (host) drives device_sim over the link.
# The c2 runner executes a demo sequence then exits; compose stops the device too.
# Logs land in ./logs/ (device.csv, c2.csv).
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p logs

echo "[c2] running demo sequence…"
trap 'podman compose -f compose.yaml down' EXIT
podman compose -f compose.yaml up --build \
  --abort-on-container-exit --exit-code-from c2
