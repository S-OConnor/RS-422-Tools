#!/usr/bin/env bash
# Hardware-free integration smoke test.
#
# Exercises the built C++ apps against the Python reference simulators (in tools/)
# over TCP loopback — the same interop the README documents, but scripted and
# asserted. Works for native binaries and for cross-built binaries run under
# qemu-user (set $EMU to the emulator prefix).
#
#   ci/integration_smoke.sh <bindir>          # native
#   EMU="qemu-aarch64-static -L /usr/aarch64-linux-gnu -cpu cortex-a72" \
#       ci/integration_smoke.sh <bindir>      # cross, under emulation
#
# Scenarios:
#   1. streaming — Python sensor_sim streams 50 AttitudeSamples into the C++
#      rs422-monitor (listener); assert the monitor decodes them and logs a CSV.
#   2. c2        — C++ rs422-c2 reads/writes registers on the Python device_sim;
#      assert the seeded value reads back and a written value round-trips.
set -euo pipefail

BIN="${1:?usage: ci/integration_smoke.sh <bindir> (dir holding rs422-* binaries)}"
EMU="${EMU:-}"                      # emulator prefix for foreign-arch binaries
HOST=127.0.0.1
STREAM_PORT=5555
C2_PORT=5556

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PYTHONPATH="$REPO/tools${PYTHONPATH:+:$PYTHONPATH}"
WORK="$(mktemp -d)"

pids=()
cleanup() {
  for p in "${pids[@]:-}"; do kill "$p" 2>/dev/null || true; done
  rm -rf "$WORK"
}
trap cleanup EXIT

# Run a C++ app, prefixed by the emulator when cross-compiled ($EMU word-splits).
# shellcheck disable=SC2086
cpp() { local app="$1"; shift; $EMU "$BIN/$app" "$@"; }

# Wait until *something* is LISTENing on a port, without connecting to it (a probe
# connection would be consumed as the monitor's single accepted client).
wait_listen() {
  for _ in $(seq 1 100); do
    if ss -Htnl 2>/dev/null | grep -qE "[:.]$1[[:space:]]"; then return 0; fi
    sleep 0.1
  done
  echo "integration: timed out waiting for a listener on :$1" >&2
  return 1
}

echo "== scenario 1: streaming (python sensor_sim -> ${EMU:+qemu }rs422-monitor) =="
cpp rs422-monitor --tcp "$HOST:$STREAM_PORT" --listen --log "$WORK/attitude.csv" &
mon=$!; pids+=("$mon")
wait_listen "$STREAM_PORT"
python3 -m apps.sensor_sim --tcp "$HOST:$STREAM_PORT" --rate 100 --count 50
if wait "$mon"; then mon_rc=0; else mon_rc=$?; fi
pids=("${pids[@]/$mon}")
lines=$(wc -l < "$WORK/attitude.csv" 2>/dev/null || echo 0)
echo "   monitor exit=$mon_rc, csv lines=$lines"
[ "$mon_rc" -eq 0 ] || { echo "FAIL: monitor exited non-zero"; exit 1; }
[ "$lines" -ge 45 ] || { echo "FAIL: expected >=45 logged samples, got $lines"; exit 1; }

echo "== scenario 2: c2 (${EMU:+qemu }rs422-c2 -> python device_sim) =="
python3 -m apps.device_sim --tcp "$HOST:$C2_PORT" --listen --size 256 \
        --seed 0x10=0x1234 --seed 0x11=0x00AA &
dev=$!; pids+=("$dev")
wait_listen "$C2_PORT"

out=$(cpp rs422-c2 --tcp "$HOST:$C2_PORT" read 0x10); echo "   read 0x10  -> $out"
echo "$out" | grep -qi "0x1234" || { echo "FAIL: seeded read 0x10 != 0x1234"; exit 1; }
sleep 0.3
cpp rs422-c2 --tcp "$HOST:$C2_PORT" write 0x20 0xBEEF >/dev/null
sleep 0.3
out=$(cpp rs422-c2 --tcp "$HOST:$C2_PORT" read 0x20); echo "   read 0x20  -> $out"
echo "$out" | grep -qi "0xBEEF" || { echo "FAIL: write/read-back 0x20 != 0xBEEF"; exit 1; }

echo "ALL INTEGRATION CHECKS PASSED"
