# RS-422 Attitude Sensor Link

Receive a stream of **Euler attitude (roll/pitch/yaw) at 10 Hz** from a sensor over an
[Adafruit #5994 USB to Multi-Protocol Serial Cable](https://www.adafruit.com/product/5994)
(FTDI FT231X + MAX485, RS-422/485). The PC is a listener — this is a one-way telemetry stream.

The sensor's wire format is defined here, so a **sensor simulator** lets you develop and test
the whole pipeline with no hardware. A real sensor swaps in later without changing anything
downstream. See [docs/plan-attempt2.md](docs/plan-attempt2.md) for the design rationale.

## Layout

```
serial_link/        reusable link core (lifted from the archived project)
  transport/        L0  the cable (SerialTransport) + hardware-free stand-ins (Pty/Loopback)
  framing/          L1  RFC 1662 HDLC framing + FCS-16
  codec/            L2  struct-spec messages + type-id registry  (messages/ = one file per message)
  link/             L3  FramedLink: send/receive Message objects
apps/                  thin apps on the core — see apps/README.md
  sensor_sim/          attitude TX: synthesise an AttitudeSample stream @ 10 Hz
  attitude_publisher/  attitude TX: replay a recorded CSV back out over the link
  monitor/             attitude RX: receive + live display (+ optional CSV log)
  attitude_log.py      shared CSV read/write (used by monitor + publisher)
  c2/                  register C2 host: send read/write, await + print reply
  device_sim/          register C2 device stand-in: hold registers, answer requests
archive/            the parked register-read/write project (reference only)
```

**Why HDLC framing for a one-way stream?** The `0x7E` flags make it self-synchronizing — a
receiver that starts mid-stream or hits noise re-syncs on the next flag, and the FCS-16 rejects
partial/corrupt frames.

## Message — `AttitudeSample` (`type_id 0x20`, 20-byte body, big-endian)

`seq:u32` · `t_ms:u32` · `roll:f32` · `pitch:f32` · `yaw:f32` (degrees). Maps 1:1 to a packed
C/C++ struct for a future port.

## Install

```bash
python -m pip install -e ".[dev]"   # pyserial + pytest
```

## Try it (no hardware)

Two terminals, talking over TCP:

```bash
# terminal 1 — live attitude display
python -m apps.monitor --tcp 127.0.0.1:5555 --listen
# terminal 2 — the simulated sensor
python -m apps.sensor_sim --tcp 127.0.0.1:5555
```

The monitor refreshes one line ~10×/sec:

```
roll=  +6.4°  pitch=  +5.9°  yaw=  20.0°   seq=10     rate=10.0Hz
```

`--count N` sends a fixed number of samples; `--rate HZ` changes the cadence. For a pty-based
"serial" link instead of TCP, run `./scripts/make_vport.sh` (needs `socat`) and use
`--port /dev/pts/N` on both apps.

### Record and replay

`monitor --log FILE` records the stream to CSV; `attitude_publisher` plays a CSV back out over
the link (the inverse of the monitor) — a recording round-trips byte-for-byte:

```bash
python -m apps.monitor --tcp 127.0.0.1:5555 --listen --log flight.csv   # record
python -m apps.attitude_publisher --port /dev/ttyUSB0 flight.csv         # replay over RS-422
python -m apps.attitude_publisher --tcp 127.0.0.1:5555 --realtime flight.csv  # honour recorded timing
```

The publisher reads any CSV with `roll`/`pitch`/`yaw` columns (`seq`/`t_ms` optional). Use
`--realtime` to replay the original cadence, `--rate HZ` for a fixed rate, `--loop` to repeat.

## Hardware

```bash
ls /dev/serial/by-id/                              # find the FT231X port(s)
python -m apps.monitor --port /dev/ttyUSB0 --baud 115200
```

Sensor on one cable, PC on another: wire **A↔A, B↔B, GND↔GND**. Confirm the cable's actual
RS-422/485 terminal labels and the sensor's baud before wiring.

## Test

```bash
python -m pytest -q
```

Covers FCS-16 + HDLC framing (reused from the archive), the `AttitudeSample` codec, the CSV
logger, and an end-to-end stream loopback (sensor_sim → decode) asserting contiguous `seq` and
model-accurate attitude.

### Containerised test environments

Two self-contained environments (own compose file + run script), no hardware:

```bash
./environments/streaming/run.sh   # attitude: sensor_sim -> monitor (Ctrl-C to stop)
./environments/c2/run.sh          # register C2: c2 drives device_sim, then exits
```

Logs land in each environment's `logs/`.

For the full set of ways to run this:
- [docs/simulation-testing.md](docs/simulation-testing.md) — no-hardware paths (pytest, TCP
  loopback, pty/socat virtual ports, in-process loopback, containers).
- [docs/hardware-testing.md](docs/hardware-testing.md) — real serial paths (cheap USB-TTL
  adapters as a stand-in, finding devices via `by-id`, then the real RS-422 cable + sensor).
