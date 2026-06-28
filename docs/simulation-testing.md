# Simulation Testing

How to exercise the whole attitude link **with no hardware**. These paths cover the
entire software stack — transport interface, RFC 1662 framing, FCS-16, the
`AttitudeSample` codec, and the apps — using stand-in transports instead of a cable.

> See [hardware-testing.md](hardware-testing.md) for the real-serial paths.

## Setup

```bash
python -m pip install -e ".[dev]"   # installs serial_link + apps + pyserial + pytest
```

## 1. Unit + end-to-end tests (fastest)

```bash
python -m pytest -q
```

Covers FCS-16 and HDLC framing (good-residual `0xF0B8`, byte-stuffing, split reads,
garbage recovery, bad-FCS rejection), the `AttitudeSample` codec (round-trips +
big-endian wire layout), the CSV logger, and a full stream loopback
(`sensor_sim → decode`) asserting contiguous `seq` and model-accurate attitude.

## 2. TCP loopback (two processes, no hardware)

The monitor listens on a local TCP port; the sensor connects to it. The same
framing/codec runs byte-for-byte as it would over the cable.

```bash
# terminal 1 — receiver
python -m apps.monitor    --tcp 127.0.0.1:5555 --listen

# terminal 2 — simulated sensor
python -m apps.sensor_sim --tcp 127.0.0.1:5555
```

Bounded run for scripting/CI (`--count` stops after N samples):
```bash
python -m apps.monitor    --tcp 127.0.0.1:5555 --listen --log attitude.csv &
python -m apps.sensor_sim --tcp 127.0.0.1:5555 --rate 10 --count 50
```

## 3. Virtual serial ports (pty) — closest to a real `--port`

This drives the actual `SerialTransport` path through a pseudo-terminal, so it
behaves like `/dev/ttyUSB*` without hardware.

**Single command, two processes:** one side creates the pty and prints its slave
path; the other opens that path with `--port`:
```bash
python -m apps.sensor_sim --pty
#  -> [pty ready] point the peer at: /dev/pts/7
python -m apps.monitor --port /dev/pts/7
```

**Two linked ptys via socat** (both sides use `--port`):
```bash
./scripts/make_vport.sh            # prints two /dev/pts/N paths (needs socat)
python -m apps.monitor    --port /dev/pts/<A>
python -m apps.sensor_sim --port /dev/pts/<B>
```

## 4. In-process loopback (one Python snippet)

Useful for quick experiments or embedding in a test:

```python
import socket
from serial_link import FramedLink, LoopbackTransport
from apps.sensor_sim import stream

a, b = socket.socketpair()
tx = FramedLink(LoopbackTransport(a))
rx = FramedLink(LoopbackTransport(b))

stream(tx, rate=10, count=5, pace=False)   # pace=False = run flat-out, no sleeping
tx.close()                                  # EOF so rx.receive() terminates

for rf in rx.receive():
    print(rf.message)
```

## 5. Containers (podman compose)

Two self-contained test environments, each with its own compose file + run
script, sharing one image. No cable, no local Python env needed. Logs land in
each environment's `logs/`.

```bash
# attitude streaming: sensor_sim -> monitor (runs until Ctrl-C)
./environments/streaming/run.sh

# register command & control: c2 drives device_sim through a demo sequence, exits
./environments/c2/run.sh
```

Each `run.sh` is a thin wrapper; the equivalent raw commands are:
```bash
podman compose -f environments/streaming/compose.yaml up --build
podman compose -f environments/c2/compose.yaml up --build \
  --abort-on-container-exit --exit-code-from c2
```

Note: the monitor's live line uses `\r` (no newline), so `podman compose logs` may
buffer it. To confirm decode inside the image directly:
```bash
podman run --rm rs422-attitude python -c "
import socket
from serial_link import FramedLink, LoopbackTransport
from apps.sensor_sim import stream
a,b=socket.socketpair(); tx=FramedLink(LoopbackTransport(a)); rx=FramedLink(LoopbackTransport(b))
stream(tx, rate=10, count=5, pace=False); tx.close()
print([rf.message.seq for rf in rx.receive()])
"
```

## What simulation covers vs not

- ✅ Transport interface, HDLC framing + FCS, `AttitudeSample` codec, the apps, CSV
  logging, drop/rate accounting, and the full 10 Hz pipeline end-to-end.
- ❌ Real UART electrical behavior, device enumeration / `by-id` paths, baud
  mismatches, and RS-422 differential signaling — for those, see
  [hardware-testing.md](hardware-testing.md).
