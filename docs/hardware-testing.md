# Hardware Testing

How to exercise the attitude link over **real serial hardware** — first with cheap
USB-to-TTL adapters while you wait for the parts, then with the real Adafruit RS-422
cable and sensor.

> The bytes our stack sends are identical whether they ride on RS-422 differential
> pairs or plain TTL UART — the physical layer is invisible to `pyserial` and to
> `serial_link`. So a pair of TTL adapters faithfully tests everything *except* the
> RS-422 electrical/half-duplex layer (which our software never touches).

> **Run the `python -m …` commands from the `tools/` directory** (where the Python
> lives), or use the console scripts after `cd tools && pip install -e .`.

## Option A — two USB-to-TTL adapters (cheap stand-in)

### Parts
A 2-pack of CP2102 (or FT232RL) USB-to-TTL adapters, ~$8–10. See the Amazon links
in the project notes. Get **two** — one for each end.

### Wiring
Connect the two adapters back-to-back. Cross TX↔RX, share ground, leave power alone:

```
Adapter A  TX  ─────►  RX  Adapter B
Adapter A  RX  ◄─────  TX  Adapter B
Adapter A  GND ───────  GND Adapter B
                 (do NOT connect VCC / 5V / 3V3 between them)
```

Match logic levels — both adapters at 3.3 V or both at 5 V (most CP2102 boards are
3.3 V TTL). Only TX, RX, GND are connected.

### Find the device paths (Linux / Fedora)
Both adapters are plug-and-play (kernel `cp210x` / `ftdi_sio` drivers). Don't trust
`/dev/ttyUSB0` numbering — it changes on replug. Use the stable identifiers:

```bash
ls -l /dev/serial/by-id/            # stable names embed the chip + serial number
python -m serial.tools.list_ports -v   # VID:PID, serial number, device path
```

Each adapter shows a unique serial number, e.g.:
```
/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_..._0001-if00-port0 -> ../../ttyUSB0
```

### Permissions
Serial devices are group-owned (usually `dialout` on Fedora). If you get
`Permission denied`:
```bash
ls -l /dev/ttyUSB0                  # note the group (e.g. dialout)
sudo usermod -aG dialout "$USER"    # add yourself, then log out/in (or: newgrp dialout)
```

### Run it
Two terminals, one adapter each (use the `by-id` paths for stability):

```bash
# the simulated sensor
python -m apps.sensor_sim --port /dev/ttyUSB0 --baud 115200

# the receiver (optionally log to CSV)
python -m apps.monitor    --port /dev/ttyUSB1 --baud 115200 --log attitude.csv
```

The monitor should show a refreshing line at `rate=10.0Hz`, `0 dropped`.

## Option B — real Adafruit RS-422 cable(s)

The [Adafruit #5994 cable](https://www.adafruit.com/product/5994) (FTDI FT231X +
MAX485) enumerates exactly like the TTL adapters — same `--port` flow, same `by-id`
discovery. Differences to handle:

- **Two cables to talk on one PC.** Wire the RS-422/485 terminals: **A↔A, B↔B,
  GND↔GND**. Each cable is its own `/dev/ttyUSB*`; run `sensor_sim` on one,
  `monitor` on the other.
- **Confirm the terminal labels** on the physical cable before wiring (2-wire A/B
  vs 4-wire TX±/RX±) — see the build plan's open items.
- **Real sensor:** only **one** cable (sensor → PC). The sensor must emit the
  `AttitudeSample` format we defined (or we add a decoder for its real format).
  Confirm its **baud** and pass it with `--baud`.
- Half-duplex single A/B pair is fine for this one-way stream (the sensor always
  talks); the FTDI auto-direction handles the MAX485.

## Troubleshooting

| Symptom | Likely cause / fix |
|---------|--------------------|
| No samples at all | TX/RX not crossed — swap the two data wires. Check GND is connected. |
| `[monitor]` shows only bad frames | Baud mismatch — set the same `--baud` on both ends. |
| `Permission denied` opening port | Add yourself to the `dialout` group (above). |
| Don't know which port is which | `udevadm info -q property -n /dev/ttyUSB0`, or unplug one and re-run `ls /dev/serial/by-id/`. |
| Garbage / occasional bad FCS | Loose jumper, wrong logic level, or too-long wires. The decoder re-syncs on the next `0x7E` flag and rejects bad frames, so the stream recovers. |

## What this does and doesn't cover

- ✅ Real `pyserial` I/O, device enumeration, `by-id` paths, baud/8N1 framing,
  HDLC de-framing and FCS over a true UART, end-to-end at 10 Hz.
- ❌ RS-422 differential signaling, termination, and half-duplex direction control
  (only the real cable exercises those — but the software is identical).

See [simulation-testing.md](simulation-testing.md) for the no-hardware paths.
