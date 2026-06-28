# Attitude Sensor Link @ 10 Hz — Build Plan (attempt 2)

## Context

Same hardware as before — an [Adafruit #5994 *USB to Multi-Protocol Serial Cable*](https://www.adafruit.com/product/5994)
(FTDI **FT231X** + **MAX485**, RS-422/485). New mission: connect an **attitude sensor that
streams Euler attitude (roll/pitch/yaw) periodically at 10 Hz**. The PC is a **listener** — this
is a one-way, read-only telemetry stream, not a request/response protocol.

We **define the wire format ourselves and simulate the sensor** now (real sensor swaps in later).
The previous register-read/write project is parked in `archive/`; its layered core
(`transport/`, `framing/`, `codec/`, `link/`) is proven and **reused almost verbatim** here —
we only replace the message catalog and the apps.

Goal of this pass: a **sensor simulator** that emits attitude frames at 10 Hz and a **monitor**
that shows live attitude in the terminal, both runnable with no hardware.

## Why keep RFC 1662 HDLC framing for a one-way stream

Flag-delimited HDLC framing is **self-synchronizing**: a receiver that starts mid-stream, or
hits line noise, re-syncs on the next `0x7E` flag, and the FCS-16 rejects partial/corrupt
frames. Exactly the property a continuous 10 Hz feed needs. So we keep the archived
framing + struct-spec codec unchanged.

## Architecture (layered, reused from archive)

```
  sensor (or sensor_sim) ──RS-422──▶ monitor
                                     receive → de-frame → decode → live display

  L3 link       FramedLink.receive()  (blocking generator; one-way: recv only)
  L2 codec      struct-spec; single AttitudeSample message
  L1 framing    RFC 1662 HDLC (flag, byte-stuffing, FCS-16)   [verbatim from archive]
  L0 transport  SerialTransport (cable) + Pty/Loopback stand-ins [verbatim from archive]
```

## Message — `AttitudeSample`

`type_id = 0x20`, big-endian, fixed 20-byte body (maps 1:1 to a C++ packed struct):

| field   | type | notes |
|---------|------|-------|
| `seq`   | u32  | increments per sample → drop/gap detection |
| `t_ms`  | u32  | sensor timestamp, ms since start |
| `roll`  | f32  | degrees |
| `pitch` | f32  | degrees |
| `yaw`   | f32  | degrees |

Frame on the wire: `FLAG | address | control | 0x20 | seq | t_ms | roll | pitch | yaw | FCS-16 | FLAG`.

## Proposed layout

```
rs-422_application/
  archive/                       # parked previous project (untouched, reference only)
  serial_link/                   # core lifted from archive/serial_link
    transport/ base.py cable.py virtual.py     # L0  (verbatim)
    framing/   fcs.py framing.py               # L1  (verbatim)
    codec/     codec.py messages.py            # L2  (codec verbatim; messages → AttitudeSample)
    link/      link.py                         # L3  (verbatim)
    cli.py  framelog.py                        # shared helpers (verbatim)
  apps/
    sensor_sim.py                # emit AttitudeSample @ 10 Hz (motion model)
    monitor.py                   # receive + live single-line display
  scripts/make_vport.sh          # socat linked PTYs for hardware-free runs (from archive)
  tests/  test_fcs.py test_framing.py test_attitude_codec.py test_stream_loopback.py
  pyproject.toml  README.md
```

## Components

### serial_link core (reuse)
Copy `archive/serial_link/{transport,framing,link,cli.py,framelog.py}` out of the archive
unchanged. Replace `codec/messages.py` with a single `AttitudeSample` (drop ReadRegister/
WriteRegister/Telemetry). `codec/codec.py` (struct-spec `Message`/`Field` + type-id registry)
is reused as-is — `AttitudeSample` is fixed-size, so no variable-length codec work is needed.

### apps/sensor_sim.py  (the sensor stand-in)
- Reuse `serial_link.cli` for `--port/--tcp/--pty` transport selection.
- Drift-corrected 10 Hz loop: `target = start + n*0.1; sleep until target` (no cumulative drift).
- Gentle motion model so the display moves: yaw ramps (~20°/s, wraps), roll/pitch slow sinusoids.
- Make the **period and sample count injectable** (e.g. `--rate`, `--count`) so tests run it
  deterministically without wall-clock dependence.
- Per tick: build `AttitudeSample(seq, t_ms, roll, pitch, yaw)`, `link.send(...)`.

### apps/monitor.py  (the receiver)
- Open transport, `for rf in link.receive():` — on each decoded `AttitudeSample`, refresh one
  terminal line (`\r`): `roll=+ddd.d°  pitch=+ddd.d°  yaw=ddd.d°   seq=N  rate=NN.N Hz`.
- Measured rate from inter-sample time (cheap, confirms the 10 Hz); note seq gaps if any.
- Bad-FCS / unknown-type frames: count + skip (the decoder already surfaces these).
- Live display is the deliverable; a curses TUI is an easy later upgrade.

## Decisions & open items
- **Python first** (continuity); struct-spec codec keeps the C++ port 1:1.
- **Units:** degrees, `float32` big-endian (simple, precise enough; scaled-int alternative if
  wire size ever matters).
- **Baud:** default 115200, 8N1 — confirm against the real sensor at bring-up.
- **Display richness:** single-line refreshing CLI by default; curses TUI optional.
- One-way link: half-duplex single A/B pair is fine (sensor always talks). Confirm cable's
  actual RS-422/485 terminal labels before wiring two cables together.

## Verification
- `pytest -q` — hardware-free:
  - `test_fcs` / `test_framing` reused from archive (FCS good-residual `0xF0B8`, stuffing,
    split reads, garbage recovery, bad-FCS reject).
  - `test_attitude_codec` — `AttitudeSample` encode/decode round-trip (floats via `pytest.approx`).
  - `test_stream_loopback` — run sensor_sim for N samples into a loopback transport; monitor-side
    decode asserts **seq is contiguous** and roll/pitch/yaw match the model (approx).
- **Live demo (no hardware):** `python -m apps.sensor_sim --tcp 127.0.0.1:5555` (or `--pty`) in
  one terminal and `python -m apps.monitor --tcp 127.0.0.1:5555 --listen` in another → the
  monitor line updates ~10×/sec with moving attitude.
- **Hardware:** `ls /dev/serial/by-id/` for the FT231X port(s); wire A↔A / B↔B / GND↔GND between
  two cables; run sensor_sim on one port, monitor on the other; confirm clean ~10 Hz decode.

## References
- [Adafruit #5994 cable](https://www.adafruit.com/product/5994) ·
  RFC 1662 (HDLC framing + FCS-16, Appendix C) ·
  archived layered core under `archive/serial_link/`
