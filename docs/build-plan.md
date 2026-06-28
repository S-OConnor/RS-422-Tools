# Board Comms SDK + Apps — Layered Build Plan

## Context

We have an [Adafruit #5994 *USB to Multi-Protocol Serial Cable*](https://www.adafruit.com/product/5994)
(FTDI **FT231X** + **MAX485**, RS-422/485). Over time we will build **several independent
applications** that all talk to boards over this cable, run at different times:

- **C2 of board A** — request/response: send commands / read-write registers, await replies.
- **Board B publisher** — a device that periodically publishes internal data as telemetry.
- **Serial→Ethernet bridge** — receives board-B frames and emits **UDP** datagrams downstream.

The point of this plan is **not** any one app — it is a **reusable SDK** that owns all the
cable/framing/codec code *once*, so each app is a thin composition on top. Messages use
**RFC 1662 HDLC-like framing** (flag-delimited, byte-stuffed, FCS-16) with a **struct-spec
codec** for the payloads. We start by building the **receive path** end-to-end.

## Layered architecture

```
┌──────────────────────────────────────────────────────────────┐
│  Applications  (thin — composition + app-specific logic only) │
│   • c2_boardA   • boardB_publisher   • serial2eth (UDP)       │
├──────────────────────────────────────────────────────────────┤
│  L3  Endpoint helpers (opt-in; apps pick their own model)     │
│   • FramedLink.receive() loop · req/resp correlation · publish│
│   • NO concurrency model forced — blocking core; apps add     │
│     threads/asyncio as they need them                         │
├──────────────────────────────────────────────────────────────┤
│  L2  Message codec — struct-spec catalog                      │
│   • Message = declarative field spec (name, struct fmt, endian)│
│   • type-id registry: id ↔ message class; pack()/unpack()     │
├──────────────────────────────────────────────────────────────┤
│  L1  Framing — RFC 1662 HDLC (flag, byte-stuffing, FCS-16)    │
│   • FrameDecoder (streaming, receive) · encode() (transmit)   │
├──────────────────────────────────────────────────────────────┤
│  L0  Transport — THE CABLE & HARDWARE  (written once, reused) │
│   • Transport ABC: open/close/read/write bytes                │
│   • SerialTransport (FTDI cable) · LoopbackTransport · PtyTransport │
└──────────────────────────────────────────────────────────────┘
```

**Component split (top-level directories):** the system is divided by concern into two
human-readable packages:

- **`serial_link/`** — *the serial connection*. Owns L0–L3 above (the cable, framing, codec,
  `FramedLink`). Knows nothing about Ethernet. Every serial app reuses it unchanged.
- **`ethernet_publisher/`** — *Ethernet publishing*. Depends on `serial_link` to receive
  decoded frames and forwards them downstream as **UDP** datagrams. Owns the UDP side only.

Data flows one way across the boundary:
`board → serial_link (decode) → ethernet_publisher (UDP) → consumer`.

**Reuse story:** the FTDI/cable code (L0) and framing (L1) live once in `serial_link` and never
change per app. A new app picks a transport, selects the message types it cares about, and runs
— ~50 lines. Multiple boards = **HDLC address field as a node/board id**; message routing =
**type-id** in the payload header.

**Decisions locked:** struct-spec codec (mirrors C++ structs 1:1 for the future port) · UDP
egress for the bridge · core stays blocking/synchronous so it ports cleanly and each app owns
its own concurrency.

## Frame & payload layout

```
HDLC frame:  FLAG | address(=board id) | control | INFO | FCS-16 | FLAG
INFO field:  type_id(1B) | <struct-spec message body, big-endian>
```

## Package layout (two directories, split by concern)

```
rs-422_application/
  README.md
  pyproject.toml                 # both packages + console entry points; pyserial dep
  serial_link/                   # ===== THE SERIAL CONNECTION =====
    __init__.py                  # flat façade re-exporting every layer
    transport/                   # L0: the byte pipe
      base.py                    #     Transport ABC (the seam)
      cable.py                   #     SerialTransport — the real RS-422 cable
      virtual.py                 #     PtyTransport, LoopbackTransport — cable stand-ins
    framing/                     # L1: RFC 1662 HDLC
      fcs.py                     #     FCS-16 (table from poly 0x8408)
      framing.py                 #     FrameDecoder (recv), encode (xmit)
    codec/                       # L2: messages
      codec.py                   #     struct-spec Message base, Field, type-id registry
      messages.py                #     concrete catalog (ReadReg, WriteReg, Telemetry…)
    link/                        # L3: endpoint
      link.py                    #     FramedLink (Transport+framing+codec) recv/send
    framelog.py                  # shared: hex + decoded-frame logging
    cli.py                       # shared: --port/--tcp/--pty → Transport
    tools/
      rx_monitor.py              # open cable, receive, decode, print
      tx_demo.py                 # stand-in board: send sample frames
  ethernet_publisher/            # ===== ETHERNET PUBLISHING (UDP) =====
    __init__.py
    udp.py                       # UdpPublisher: fire-and-forget UDP egress
    bridge.py                    # SerialToEthernetBridge: serial frames → UDP
    tools/
      serial2eth.py              # CLI bridge app
  scripts/
    make_vport.sh                # socat: two linked PTYs for hardware-free runs
  tests/
    test_fcs.py  test_framing.py  test_codec.py  test_link_loopback.py   # serial_link
    test_udp_publisher.py  test_bridge.py                                # ethernet_publisher
```

## Build phases

> **Status:** Phase 1 (serial_link receive base) is **done**, and the first slice of
> ethernet_publisher (`UdpPublisher` + `SerialToEthernetBridge` + `serial2eth`) is **done**.
> 27 unit + cross-package loopback tests pass. Remaining Phase 2 items are the request/response
> C2 app and the board-B periodic publisher.

### Phase 1 — Receive base (done)
The whole point of the first pass: **bytes off the cable → typed messages**, hardware-free.
1. `transport.py` — `Transport` ABC + `SerialTransport` (pyserial, 8N1) + `LoopbackTransport`/`PtyTransport`.
2. `fcs.py` — RFC 1662 FCS-16 (table generated from poly 0x8408); `fcs16()`, `frame_fcs()`,
   `check_fcs()` (good residual `0xF0B8`).
3. `framing.py` — `FrameDecoder`: feed byte chunks → de-stuff → FCS-check → yield frames;
   tolerant of split reads, leading garbage, back-to-back flags. (`encode()` built alongside.)
4. `codec.py` — struct-spec framework:
   - `Field(name, fmt)` using Python `struct` format chars; `Message` base with `TYPE_ID`,
     declarative `FIELDS`, `pack()`/`unpack()`; big-endian (network order) default.
   - `registry`: `type_id → Message subclass`; `decode(info)` reads `type_id`, dispatches.
5. `link.py` — `FramedLink(transport)`: `for msg in link.receive(): ...` (blocking generator);
   `link.send(msg, address=...)` for symmetry.
6. `serial_link/tools/rx_monitor.py` — opens a transport (`--port` / `--tcp` / `--pty`), prints
   each decoded message + raw hex. **This is the deliverable that proves the receive stack.**
7. Tests: FCS vectors, stuffing edge cases, codec round-trips, and a loopback that frames a
   message through a socket/pty pair and decodes it back.

### Ethernet publishing — first slice (done)
- `ethernet_publisher/udp.py` (`UdpPublisher.publish(bytes)` → `sendto`).
- `ethernet_publisher/bridge.py` (`SerialToEthernetBridge`: pump `serial_link` frames →
  pluggable transform → UDP; default transform forwards the raw INFO field).
- `ethernet_publisher/tools/serial2eth.py` (CLI: serial `--port/--tcp/--pty` + `--udp HOST:PORT`).

### Phase 2 — remaining serial apps
- `messages.py` catalog grows (Command/Ack/Nak; variable-length multi-register reads).
- `c2_boardA` (Requester: send + await correlated reply w/ timeout).
- `boardB_publisher` (DataStore = generalized register map + periodic publish loop).
- richer bridge transforms (e.g. JSON/structured datagrams instead of raw INFO).

## Verification

- `pytest -q` — all unit + loopback tests pass with **no hardware**.
- **Receive-base demo (hardware-free):** `python -m serial_link.tools.rx_monitor --tcp
  127.0.0.1:5555 --listen` in one terminal, `python -m serial_link.tools.tx_demo --tcp
  127.0.0.1:5555` in another → decoded messages print. (Or `scripts/make_vport.sh` + `--port`.)
- **Bridge demo (hardware-free):** `python -m ethernet_publisher.tools.serial2eth --tcp
  127.0.0.1:5555 --listen --udp 127.0.0.1:9000`, feed it with `tx_demo`, watch UDP on :9000.
- **Hardware:** `ls /dev/serial/by-id/` to find the FT231X port(s); wire A-A / B-B / GND-GND
  between two cables; run a tool on one while the other transmits; confirm clean decode.
- **Negative checks:** corrupt a byte → FCS reject; unknown type-id → logged + skipped, stream
  recovers after garbage between flags.

## Open items

- Physical RS-422/485 terminal labels (2-wire A/B vs 4-wire TX±/RX±) — confirm at bring-up.
- Baud rate to match real firmware (default 115200, 8N1).
- Whether `address` carries a real board id now or stays PPP-default `0xFF` until multi-drop.

## References
- [Adafruit #5994 cable](https://www.adafruit.com/product/5994) ·
  [Cirkit Designer pinout notes](https://docs.cirkitdesigner.com/component/3c6d6906-3273-4c53-8e21-a6083c3fca3b/usb-to-multi-protocol-serial-cable) ·
  RFC 1662 (FCS-16 & transparency in Appendix C)
