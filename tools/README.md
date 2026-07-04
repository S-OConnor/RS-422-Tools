# tools/ — RS-422 integration-test drivers & framework

This is the Python software for the project. It is **not production firmware** — it
is a set of **test drivers** plus a small **framework** for **integration-testing**
the RS-422 link and the devices that sit on it (the attitude sensor and the
command-&-control board). You use it to stand in for hardware that isn't built yet,
to drive and observe real hardware once it is, and to run automated tests of the
on-wire protocol.

## Contents

```
tools/
  serial_link/   THE FRAMEWORK — the reusable link stack, shared by every driver:
                 L0 transport (cable + hardware-free stand-ins) · L1 RFC 1662 HDLC
                 framing + FCS-16 · L2 struct-spec codec + message catalog · L3 FramedLink
  apps/          THE TEST DRIVERS — thin programs that each simulate, drive, or observe
                 one side of the bus (see apps/README.md):
                   attitude:  sensor_sim · attitude_publisher · monitor
                   register C2: c2 (host) · device_sim (device)
  tests/         Automated unit + integration tests (pytest)
  pyproject.toml Install + console-script entry points
  Containerfile  Image used by the containerised environments (../environments/*)
```

## What each piece is for in integration testing

- **serial_link** is the framework the drivers are built on. It owns the cable, the
  framing, and the message codec, so every driver speaks the identical wire format —
  whether over a real RS-422 cable, a TCP loopback, or a pseudo-terminal.
- **apps** are the drivers. They come in complementary pairs so you can test one side
  without the other's hardware: `sensor_sim` ↔ `monitor` (attitude stream),
  `c2` ↔ `device_sim` (register read/write). Swap a simulator for the real device and
  nothing else changes.
- **tests** assert the protocol end-to-end (framing, FCS, codec round-trips,
  request/response, record→replay) with no hardware.

## Run it

All commands run from **this `tools/` directory** (or install once, then the console
scripts work from anywhere):

```bash
cd tools
python -m pip install -e ".[dev]"     # framework + drivers + pytest

python -m pytest -q                    # automated tests

python -m apps.monitor --tcp 127.0.0.1:5555 --listen      # a driver
python -m apps.sensor_sim --tcp 127.0.0.1:5555
```

After `pip install`, the drivers are also on PATH as `sensor-sim`, `monitor`,
`attitude-publisher`, `c2`, `device-sim`.

## Related

- [apps/README.md](apps/README.md) — what each driver does, with example commands.
- [../environments/](../environments/) — containerised integration environments
  (streaming, C2) that wire the drivers together with `podman compose`.
- [../docs/simulation-testing.md](../docs/simulation-testing.md) /
  [../docs/hardware-testing.md](../docs/hardware-testing.md) — the full menu of
  no-hardware and real-serial test paths.
