# Interface Design Document — Register Command & Control Interface

| | |
|---|---|
| **Document ID** | IDD-REG-001 |
| **Interface** | Register read/write command & control (host → device, with reply) |
| **Transport** | RS-422/485, RFC 1662 HDLC-like framing, FCS-16 |
| **Applies to** | `serial_link` C++ library and Python reference (`ReadRegister`/`WriteRegister`/`ReadResponse`/`WriteAck`/`Nak`) |
| **Companion** | [IDD-ATT-001 — Attitude Data](idd-attitude.md) |

## 1. Scope

This document specifies the wire interface for **register command and control**: a
host (the *master*) reads and writes 16-bit registers on a board (the *device* /
*slave*) and awaits a reply for each request. It defines the physical layer, the
frame format, the byte-exact layout of every request/response message, the
register model, and the transaction rules.

The attitude telemetry interface that shares the same physical and framing layers
is specified separately in [IDD-ATT-001](idd-attitude.md).

## 2. Interface overview

```
  ┌───────────┐   request:  ReadRegister(0x01) / WriteRegister(0x02)   ┌───────────┐
  │   HOST    │ ─────────────────────────────────────────────────────►│  DEVICE   │
  │ (master)  │                                                        │  (slave)  │
  │           │◄───────────────────────────────────────────────────── │           │
  └───────────┘   reply: ReadResponse(0x81) / WriteAck(0x82) / Nak(0xFF)└───────────┘
```

- **Model:** strict request → reply. The host sends exactly one request and waits
  for exactly one reply. The device never speaks unsolicited on this interface.
- **Correlation:** *implicit* — on a point-to-point link the next frame the host
  receives is the reply to its outstanding request. There is no transaction id.
- **Outstanding requests:** at most **one** in flight. The host must receive (or
  time out) the reply before issuing the next request.
- **Timeout:** host-side, default **1.0 s**. On timeout the host reports failure;
  the transaction is not retried automatically.
- **Topology:** point-to-point (one host, one device). The frame address field is
  reserved for future multi-drop addressing (see [§4](#4-data-link-layer-rfc-1662-framing)).

## 3. Physical / electrical layer

| Parameter | Value |
|-----------|-------|
| Signalling | RS-422 / RS-485 differential pairs (A/B), or 5 V/3.3 V TTL UART for bench test |
| Reference cable | Adafruit #5994 (FTDI FT231X + MAX485) |
| Byte format | 8 data bits, no parity, 1 stop bit (**8N1**), no hardware flow control |
| Default baud | **115 200** (must match at both ends; settable via `--baud`) |
| Duplex | Half-duplex A/B pair — only one side transmits at a time (request, then reply) |
| Bit/byte order | Multi-byte fields are **big-endian** |

## 4. Data-link layer (RFC 1662 framing)

Every request and reply is carried in one HDLC-like frame:

```
  FLAG | Address | Control | INFO | FCS-16 | FLAG
  0x7E |  0xFF   |  0x03   | ...  | 2 bytes| 0x7E
```

| Field | Size | Value | Notes |
|-------|------|-------|-------|
| FLAG | 1 | `0x7E` | Delimits frames |
| Address | 1 | `0xFF` | RFC 1662 All-Stations. Reserved as a board/node id for future multi-drop |
| Control | 1 | `0x03` | RFC 1662 Unnumbered Information (UI) |
| INFO | var | — | The message: `type_id` (1 byte) + big-endian body (see [§5](#5-message-catalogue)) |
| FCS-16 | 2 | — | Frame check sequence over Address + Control + INFO |

The FCS covers **Address + Control + INFO**; the whole frame is then byte-stuffed.

### 4.1 Octet transparency (byte stuffing)

After the FCS is appended, each octet between the flags is escaped as `ESC`
(`0x7D`) followed by `octet XOR 0x20` when the octet is `0x7E`, `0x7D`, or any
control octet `< 0x20`. Register addresses/values are frequently small (`0x00`,
`0x10`, …), so expect escaping — it is transparent to the codec.

### 4.2 Frame check sequence (FCS-16)

| Property | Value |
|----------|-------|
| Algorithm | RFC 1662 FCS-16 = CRC-CCITT, reflected polynomial `0x8408` |
| Initial value | `0xFFFF` |
| Transmitted value | Ones-complement of the running FCS, **low byte first** |
| Good residual | `0xF0B8` over `INFO + FCS` for an intact frame |

A frame that fails the FCS check is discarded. On the host, a corrupt or
undecodable reply is reported as a bad reply (the transaction fails); the host may
re-issue the request. A device that receives a corrupt request simply gets no
valid request and stays silent, so the host times out.

### 4.3 Receiver framing rules

Bytes before the first flag are discarded; back-to-back flags are tolerated; a
frame under **4 octets** (Address + Control + 2 FCS) is a runt and dropped.

## 5. Message catalogue

Type-id allocation for this interface:

| Range | Direction | Purpose |
|-------|-----------|---------|
| `0x01`–`0x0F` | host → device | register command/control **requests** |
| `0x80`–`0x8F` | device → host | register **responses** |
| `0xFF` | device → host | **Nak** (error reply) |

All bodies are big-endian. INFO = 1-byte `type_id` + body.

### 5.1 `ReadRegister` — type `0x01` (host → device)

Read `count` consecutive 16-bit registers starting at `addr`. **3-byte body.**

| Offset | Size | Field | Type | Description |
|-------:|-----:|-------|------|-------------|
| 0 | 2 | `addr`  | uint16 BE | First register address |
| 2 | 1 | `count` | uint8 | Number of registers to read (**≥ 1**) |

### 5.2 `WriteRegister` — type `0x02` (host → device)

Write one 16-bit `value` to register `addr`. **4-byte body.**

| Offset | Size | Field | Type | Description |
|-------:|-----:|-------|------|-------------|
| 0 | 2 | `addr`  | uint16 BE | Register address |
| 2 | 2 | `value` | uint16 BE | Value to store |

### 5.3 `ReadResponse` — type `0x81` (device → host)

Successful reply to a `ReadRegister`. **Variable-length body**: a 2-byte echoed
address followed by one uint16 per register read. The value count is implied by the
frame length (`(body_len − 2) / 2`), and matches the request's `count`.

| Offset | Size | Field | Type | Description |
|-------:|-----:|-------|------|-------------|
| 0 | 2 | `addr` | uint16 BE | First register address (echo of the request) |
| 2 | 2×N | `values[N]` | uint16 BE[] | The register values, in ascending address order |

### 5.4 `WriteAck` — type `0x82` (device → host)

Successful reply to a `WriteRegister`. **5-byte body.** Echoes the stored value so
the host can confirm what landed.

| Offset | Size | Field | Type | Description |
|-------:|-----:|-------|------|-------------|
| 0 | 2 | `addr`   | uint16 BE | Register address (echo) |
| 2 | 2 | `value`  | uint16 BE | Value actually stored (echo) |
| 4 | 1 | `status` | uint8 | `0` = OK (reserved for future non-fatal status codes) |

### 5.5 `Nak` — type `0xFF` (device → host)

Negative acknowledgement — the request was rejected. **3-byte body.**

| Offset | Size | Field | Type | Description |
|-------:|-----:|-------|------|-------------|
| 0 | 1 | `code` | uint8 | Reason code (table below) |
| 1 | 2 | `addr` | uint16 BE | Offending register address (`0x0000` if not applicable) |

**Nak reason codes**

| Code | Name | Meaning |
|-----:|------|---------|
| 1 | `UNKNOWN_CMD` | Message type not understood / not a serviceable request |
| 2 | `BAD_ADDR` | Address, or address+count, out of range |
| 3 | `BAD_LENGTH` | Malformed request body (wrong length for the type) |
| 4 | `READ_ONLY` | Write attempted on a read-only register |

## 6. Register model

- **Registers are 16-bit**, organised as a flat bank addressed `0 .. size−1`
  (reference device: `size = 256`). Address and count are validated per request.
- **Read validity:** `count ≥ 1` and `addr + count ≤ size`; otherwise the device
  replies `Nak(BAD_ADDR)`. A read returns register values in ascending address
  order.
- **Write validity:** `addr < size`; a write to a read-only register replies
  `Nak(READ_ONLY)`; an out-of-range address replies `Nak(BAD_ADDR)`. On success
  the device stores `value & 0xFFFF` and echoes it in the `WriteAck`.
- **Register map:** the meaning of individual addresses (identity/whoami, config,
  status, command registers, and which are read-only) is device-specific and is
  defined in the applicable **device register map**, not in this IDD. This document
  fixes only the *access protocol*. A device register-map annex should tabulate, per
  address: name, access (R/RW/RO), reset value, and field semantics.

## 7. Transaction rules & error handling

1. The host sends one request frame and starts a reply timer (default 1.0 s).
2. The device validates and replies with exactly one frame:
   - `ReadRegister` → `ReadResponse` (ok) or `Nak`.
   - `WriteRegister` → `WriteAck` (ok) or `Nak`.
   - Any other/unknown request type → `Nak(UNKNOWN_CMD)`.
   - A stray non-request frame (a response/telemetry frame) is **ignored** — the
     device produces no reply.
3. The host classifies the outcome:

| Host observation | Meaning | Host action |
|------------------|---------|-------------|
| Matching response frame, FCS ok | Success | Use the decoded values / ack |
| `Nak` frame | Device rejected the request | Report `code`/`addr`; do not retry blindly |
| FCS fail / undecodable / unexpected type | Corrupt or spurious reply | Report bad reply; may re-issue |
| No frame within timeout | Lost request/reply, or device absent | Report timeout; may re-issue |

Because correlation is positional, the host must **drain or ignore** any unexpected
frame before issuing the next request, so a late reply is never mistaken for the
next transaction's answer.

## 8. Worked examples

**Read 2 registers at `0x0010`** — request and successful reply:

```
Request  ReadRegister(addr=0x0010, count=2)
  INFO  : 01 00 10 02
  FRAME : 7E FF 7D 23 7D 21 7D 20 7D 30 7D 22 51 BD 7E

Reply    ReadResponse(addr=0x0010, values=[0x1234, 0xBEEF])
  INFO  : 81 00 10 12 34 BE EF
  FRAME : 7E FF 7D 23 81 7D 20 7D 30 7D 32 34 BE EF 7D 31 8A 7E
```

**Write `0x1234` to `0x0020`** — request and successful ack:

```
Request  WriteRegister(addr=0x0020, value=0x1234)
  INFO  : 02 00 20 12 34
  FRAME : 7E FF 7D 23 7D 22 7D 20 20 7D 32 34 9D CA 7E

Reply    WriteAck(addr=0x0020, value=0x1234, status=0)
  INFO  : 82 00 20 12 34 00
  FRAME : 7E FF 7D 23 82 7D 20 20 7D 32 34 7D 20 7C BA 7E
```

**Rejected request** — read of an out-of-range address returns a Nak:

```
Reply    Nak(code=2 BAD_ADDR, addr=0x0999)
  INFO  : FF 02 09 99
  FRAME : 7E FF 7D 23 FF 7D 22 7D 29 99 53 B9 7E
```

In each frame: the leading `7E FF 7D 23` is FLAG + address `0xFF` + the stuffed
control octet `0x03` (`7D 23`); the two octets before the closing `7E` are the
transmitted FCS (low byte first); interior `0x00`/`0x10`/`0x01` etc. appear stuffed
as `7D 20` / `7D 30` / `7D 21`.

## 9. Conformance & test references

| Aspect | Reference |
|--------|-----------|
| Request/response codec round-trips & byte layout | `tests/test_register_codec.cpp`, `tools/tests/test_register_codec.py` |
| Register model (bounds, read-only, Nak mapping) | `tools/tests/test_registers.py` |
| Full request→reply transaction | `tools/tests/test_request_response.py` |
| FCS-16 & framing | `tests/test_fcs.cpp`, `tests/test_framing.cpp` |
| Reference host / device | `rs422-c2` (C++), `apps.device_sim` (Py) |

A conforming implementation is byte-compatible with the reference: the C++
`rs422-c2` host drives the Python `device_sim` across the language boundary in
both directions.
