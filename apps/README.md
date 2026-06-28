# apps/

Thin applications built on the `serial_link` core. Each one just picks a
transport, then sends or receives `AttitudeSample` frames — all the real work
(framing, FCS, codec) lives in `serial_link`. They split by **data direction**:

| App | Dir | Role |
|-----|-----|------|
| [`sensor_sim/`](sensor_sim/) | TX | Synthesise an attitude stream (test stand-in for the sensor) |
| [`attitude_publisher/`](attitude_publisher/) | TX | Replay recorded attitude from a CSV file |
| [`monitor/`](monitor/) | RX | Receive attitude, live-display it, optionally log to CSV |
| [`attitude_log.py`](attitude_log.py) | — | Shared CSV read/write (not an app; used by monitor + publisher) |

```
   sensor_sim ─┐
               ├─► (RS-422 / TCP / pty) ─► monitor ─► display + CSV
 attitude_pub ─┘                                          │
        ▲                                                 │
        └──────────────── CSV recording ◄─────────────────┘   (record → replay)
```

Run any app from the repo root with `python -m apps.<name>`. All three share the
transport flags from `serial_link.cli`: `--port DEV` (real cable), `--tcp HOST:PORT`,
`--pty`, plus `--baud` and (for `--tcp`) `--listen`.

---

## sensor_sim — synthetic attitude source (TX)

A stand-in for the real sensor: streams `AttitudeSample` frames at a fixed rate
with a smooth motion model (yaw ramps, roll/pitch sinusoids) so a receiver has
moving data to show. Used for hardware-free development; the real sensor replaces
it without changing anything downstream.

```bash
python -m apps.sensor_sim --tcp 127.0.0.1:5555          # stream @ 10 Hz
python -m apps.sensor_sim --pty --rate 50 --count 100   # 100 samples @ 50 Hz
```
Flags: `--rate HZ` (default 10), `--count N` (default: forever).

## attitude_publisher — replay a recording (TX)

The inverse of `monitor`: reads a CSV of attitude samples and streams it back out
over the link. Reads the same format `monitor --log` writes, so a recording
round-trips. Any CSV with `roll`/`pitch`/`yaw` columns works (`seq`/`t_ms` optional).

```bash
python -m apps.attitude_publisher --port /dev/ttyUSB0 flight.csv
python -m apps.attitude_publisher --tcp 127.0.0.1:5555 --realtime flight.csv
```
Flags: `--rate HZ` (fixed cadence) or `--realtime` (honour recorded `t_ms` timing),
`--loop` (repeat), `--count N`.

## monitor — receive + display (RX)

Decodes the incoming stream, refreshes a single live terminal line (attitude +
seq + measured rate), counts dropped/bad frames, and can record every sample to
CSV. Bad-FCS / unknown frames are counted and skipped; the decoder re-syncs on the
next `0x7E` flag.

```bash
python -m apps.monitor --tcp 127.0.0.1:5555 --listen
python -m apps.monitor --port /dev/ttyUSB0 --log attitude.csv
```
Flags: `--log FILE` (record to CSV).

## attitude_log.py — shared CSV (not an app)

`AttitudeCsvLogger` (write) and `read_attitude_csv` (read) define the on-disk
format shared by `monitor --log` and `attitude_publisher`. Columns:
`rx_unix, seq, t_ms, roll, pitch, yaw`.

---

For the full set of ways to run these (loopback, pty, containers, real cable),
see [../docs/simulation-testing.md](../docs/simulation-testing.md) and
[../docs/hardware-testing.md](../docs/hardware-testing.md).
