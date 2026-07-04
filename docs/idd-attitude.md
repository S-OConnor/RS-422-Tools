# Interface Design Document — Attitude Data Interface

| | |
|---|---|
| **Document ID** | IDD-ATT-001 |
| **Interface** | Attitude telemetry stream (sensor → host) |
| **Transport** | RS-422/485, RFC 1662 HDLC-like framing, FCS-16 |
| **Applies to** | `serial_link` C++ library and Python reference (`AttitudeSample`, type `0x20`) |
| **Companion** | [IDD-REG-001 — Register Command & Control](idd-registers.md) |

## 1. Scope

This document specifies the wire interface for **attitude telemetry**: a sensor
(the *device*) pushes Euler attitude samples (roll/pitch/yaw) to a listener (the
*host*) over a point-to-point serial link. It defines the physical layer, the
frame format, and the byte-exact layout of the `AttitudeSample` message so that
any two independent implementations interoperate.

The register command-and-control interface that shares the same physical and
framing layers is specified separately in [IDD-REG-001](idd-registers.md).

## 2. Interface overview

```
  ┌───────────┐   AttitudeSample (0x20) @ ~10 Hz, unsolicited   ┌───────────┐
  │  SENSOR   │ ───────────────────────────────────────────────►│   HOST    │
  │ (device)  │                                                  │ (listener)│
  └───────────┘                                                  └───────────┘
```

- **Direction:** one-way, device → host. The host never transmits on this
  interface; there is no acknowledgement, retransmission, or flow control.
- **Cadence:** periodic and unsolicited. Nominal **10 Hz** (configurable at the
  source). The stream is free-running — the host synchronises to the frame flags.
- **Correlation:** none required. Each sample carries a `seq` counter so the host
  can detect drops and reordering (see [§6](#6-behaviour-timing-and-sequencing)).
- **Topology:** point-to-point (one sensor, one listener). The frame address
  field is reserved for future multi-drop use (see [§4](#4-data-link-layer-rfc-1662-framing)).

## 3. Physical / electrical layer

| Parameter | Value |
|-----------|-------|
| Signalling | RS-422 / RS-485 differential pairs (A/B), or 5 V/3.3 V TTL UART for bench test |
| Reference cable | Adafruit #5994 (FTDI FT231X + MAX485) |
| Byte format | 8 data bits, no parity, 1 stop bit (**8N1**), no hardware flow control |
| Default baud | **115 200** (must match at both ends; settable via `--baud`) |
| Duplex | Half-duplex A/B pair is sufficient — the sensor is the only talker |
| Bit/byte order | Bytes sent in the order given below; multi-byte fields are **big-endian** |

The physical layer is transparent to the framing above it: the same bytes ride on
RS-422 differential pairs or plain TTL. See [hardware-testing.md](hardware-testing.md).

## 4. Data-link layer (RFC 1662 framing)

Every message is carried in one HDLC-like frame:

```
  FLAG | Address | Control | INFO | FCS-16 | FLAG
  0x7E |  0xFF   |  0x03   | ...  | 2 bytes| 0x7E
```

| Field | Size | Value | Notes |
|-------|------|-------|-------|
| FLAG | 1 | `0x7E` | Delimits frames; opens and closes every frame |
| Address | 1 | `0xFF` | RFC 1662 All-Stations. Reserved as a board/node id for future multi-drop |
| Control | 1 | `0x03` | RFC 1662 Unnumbered Information (UI) |
| INFO | var | — | The message: `type_id` (1 byte) + big-endian body (see [§5](#5-message-definition)) |
| FCS-16 | 2 | — | Frame check sequence over Address + Control + INFO (see [§4.2](#42-frame-check-sequence-fcs-16)) |

The FCS is computed over **Address + Control + INFO** (not the flags), then the
whole frame is byte-stuffed.

### 4.1 Octet transparency (byte stuffing)

After the FCS is appended, every octet between the opening and closing flags is
escaped as follows. A byte `b` is replaced by `ESC` (`0x7D`) followed by
`b XOR 0x20` when:

- `b == 0x7E` (FLAG), or
- `b == 0x7D` (ESC), or
- `b < 0x20` (any control octet — the conservative default ACCM).

The receiver reverses this: on `0x7D`, the next octet is XORed with `0x20`.
Because attitude bodies are full of `0x00` bytes (see the example in
[§7](#7-worked-example)), expect heavy escaping on the wire — this is normal and
handled transparently by the codec.

### 4.2 Frame check sequence (FCS-16)

| Property | Value |
|----------|-------|
| Algorithm | RFC 1662 FCS-16 = CRC-CCITT, reflected polynomial `0x8408` (x¹⁶+x¹²+x⁵+1) |
| Initial value | `0xFFFF` |
| Transmitted value | Ones-complement of the running FCS, **low byte first** |
| Good residual | A receiver running the FCS over `INFO + FCS` gets `0xF0B8` for an intact frame |

A frame whose FCS residual is not `0xF0B8` is **corrupt**; the host counts it as a
bad frame and discards it. The decoder re-synchronises on the next `0x7E` flag, so
the stream self-heals after line noise.

### 4.3 Receiver framing rules

- Bytes before the first flag are discarded (leading idle/garbage).
- Shared or back-to-back flags between frames are tolerated.
- A frame shorter than **4 octets** (Address + Control + 2 FCS) is a runt and is
  dropped silently (idle line).

## 5. Message definition

### 5.1 `AttitudeSample` — type `0x20`

One Euler attitude sample. **Fixed 20-byte body**, big-endian, mapping 1:1 onto a
packed struct. The INFO field is the 1-byte `type_id` (`0x20`) followed by this
body:

| Offset | Size | Field | Type | Units | Description |
|-------:|-----:|-------|------|-------|-------------|
| 0 | 4 | `seq`   | uint32 BE | count | Sample counter; +1 per sample. Wraps at 2³². Used for drop/gap detection |
| 4 | 4 | `t_ms`  | uint32 BE | ms | Sensor timestamp, milliseconds since sensor start. Wraps at 2³² (~49.7 days) |
| 8 | 4 | `roll`  | float32 BE (IEEE-754) | degrees | Rotation about the longitudinal (X) axis |
| 12 | 4 | `pitch` | float32 BE (IEEE-754) | degrees | Rotation about the lateral (Y) axis |
| 16 | 4 | `yaw`   | float32 BE (IEEE-754) | degrees | Rotation about the vertical (Z) axis |

Total INFO length: **21 bytes** (`0x20` type id + 20-byte body).

**Field conventions**

- Angles are IEEE-754 single-precision floats in **degrees**. Conventional ranges
  are roll ∈ [−180, 180], pitch ∈ [−90, 90], yaw ∈ [0, 360); the wire format
  itself imposes no range and any finite float32 is valid. `NaN`/`±Inf` are not
  expected and should be treated by the host as a bad sample.
- `seq` is strictly incrementing modulo 2³². A gap indicates dropped frames; a
  decrease (other than a 2³² wrap) indicates a sensor restart or reordering.
- `t_ms` is the sensor's own clock, not host time; do not assume it is
  synchronised to the host. Use it for inter-sample intervals, not absolute time.

### 5.2 Type-id allocation

Type ids `0x20`–`0x2F` are reserved for attitude / motion telemetry. Only `0x20`
(`AttitudeSample`) is defined today; a receiver **must** ignore unknown type ids in
this range without dropping the stream. (Register-interface type ids `0x01`–`0x0F`,
`0x80`–`0x8F`, and `0xFF` do not appear on this interface — see
[IDD-REG-001](idd-registers.md).)

## 6. Behaviour, timing, and sequencing

- **Rate:** nominal 10 Hz (period 100 ms). The host must not assume an exact
  period; it derives the observed rate from arrival times and/or `t_ms`.
- **Start-up:** the sensor may begin streaming at any time; the host tolerates
  joining mid-stream (it discards the partial frame before the first flag).
- **Drop detection:** the host tracks the last `seq`; `dropped += (seq − last − 1)`
  across the wrap boundary.
- **No back-channel:** the host does not ACK, request retransmission, or pace the
  sensor. A missed sample is simply lost.
- **Bandwidth:** a fully-escaped `AttitudeSample` frame is ≤ ~45 octets on the
  wire; at 10 Hz that is < 5 kbit/s, far under a 115 200-baud link.

## 7. Worked example

`AttitudeSample` with `seq=7`, `t_ms=700`, `roll=15.0°`, `pitch=−3.5°`,
`yaw=142.0°`:

```
INFO  (21 bytes, before stuffing):
  20 00 00 00 07 00 00 02 BC 41 70 00 00 C0 60 00 00 43 0E 00 00
  │  └── seq=7 ──┘ └─ t_ms=700 ─┘ └ roll ─┘ └ pitch ┘ └─ yaw ──┘
  type=0x20        (0x02BC=700)   15.0f     -3.5f      142.0f

FRAME (on the wire, flags + address 0xFF + control 0x03 + FCS, byte-stuffed):
  7E FF 7D 23 20 7D 20 7D 20 7D 20 7D 27 7D 20 7D 20 7D 22 BC 41
  70 7D 20 7D 20 C0 60 7D 20 7D 20 43 7D 2E 7D 20 7D 20 FF 9E 7E
```

Notes on the framing: the control octet `0x03` is stuffed to `7D 23`; every `0x00`
in the body is stuffed to `7D 20`; `0x0E` (in `yaw`) is stuffed to `7D 2E`. The
trailing `FF 9E` before the closing flag is the transmitted FCS (low byte first).

## 8. Conformance & test references

| Aspect | Reference |
|--------|-----------|
| Codec round-trip & big-endian layout | `tests/test_attitude_codec.cpp`, `tools/tests/test_attitude_codec.py` |
| FCS-16 vectors & good residual `0xF0B8` | `tests/test_fcs.cpp`, `tools/tests/test_fcs.py` |
| Byte-stuffing, split reads, garbage recovery | `tests/test_framing.cpp`, `tools/tests/test_framing.py` |
| End-to-end stream (sensor → decode) | `tools/tests/test_stream_loopback.py` |
| Reference producer / consumer | `apps.sensor_sim` (Py), `rs422-monitor` (C++) |

A conforming implementation is byte-compatible with the reference above: the C++
`rs422-monitor` decodes the Python `sensor_sim` stream with zero bad frames, and
vice versa.
