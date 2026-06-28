#!/usr/bin/env bash
# Streaming test environment: sensor_sim -> monitor (attitude @ 10 Hz).
# Runs until you Ctrl-C, then tears the stack down. Logs land in ./logs/.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p logs

echo "[streaming] starting (Ctrl-C to stop)…"
trap 'podman compose -f compose.yaml down' EXIT
podman compose -f compose.yaml up --build
