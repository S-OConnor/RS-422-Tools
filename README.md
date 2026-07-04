# serial_link — RS-422 attitude & register link

Software for talking to a board over RS-422/485 with an
[Adafruit #5994 USB to Multi-Protocol Serial Cable](https://www.adafruit.com/product/5994)
(FTDI FT231X + MAX485). Two supported flows:

- **Attitude streaming** — a sensor pushes Euler attitude (roll/pitch/yaw) at 10 Hz; the PC listens.
- **Register command & control** — a host reads/writes registers on a board and awaits replies.

Both use **RFC 1662 HDLC-like framing** (flag-delimited, byte-stuffed, FCS-16) with a
struct-spec codec whose message bodies map 1:1 onto the big-endian wire format.

## Repository layout

```
CMakeLists.txt · src/ · include/ · apps/ · tests/   ← the C++20 port (ships on the board / Yocto)
Containerfile.build · Containerfile.runtime          ← C++ build + runtime images
tools/                                               ← Python test drivers & framework (tools/README.md)
environments/                                        ← containerised integration envs (streaming, c2)
docs/                                                ← testing guides + design notes
```

- **Root (C++)** is the production code — the `serial_link` library plus the `rs422-*` apps
  that run on the target.
- **[tools/](tools/)** is the Python: **test drivers** (`sensor_sim`, `device_sim`, `monitor`,
  …) and the reference implementation the C++ is ported from. The two speak the **same wire
  format**, so C++ apps and Python sims interoperate directly (verified below).

---

# C++ port (the production apps)

A faithful **C++20** port of the `serial_link` core and the production apps, for deployment in
the Yocto build. Everything is `-Wall -Wextra` clean and blocking/synchronous — each app owns
its own concurrency, no threads forced.

## What's here

| Layer | Header(s) |
|-------|-----------|
| L0 transport | `transport.hpp` — `Transport`, `SerialTransport` (termios 8N1), `TcpTransport`, `PtyTransport` |
| L1 framing | `fcs.hpp` (RFC 1662 FCS-16), `framing.hpp` (`encode` / `FrameDecoder`) |
| L2 codec | `messages.hpp` (the catalog as plain structs), `codec.hpp` (`decode` → `std::variant<…>`) |
| L3 link | `link.hpp` — `FramedLink` (`send` / `next` / `request`) |
| shared | `cli.hpp`, `framelog.hpp`, `attitude_log.hpp` |

**Production apps** (built to `rs422-*` binaries):

| Binary | Python equivalent | Role |
|--------|-------------------|------|
| `rs422-monitor` | `apps.monitor` | RX: receive attitude, live-display, optional `--log CSV` |
| `rs422-attitude-publisher` | `apps.attitude_publisher` | TX: replay a recorded CSV over the link |
| `rs422-c2` | `apps.c2` | register command & control host (read/write, `--log`) |

The `sensor_sim` and `device_sim` simulators intentionally **stay in Python** (in `tools/`) —
they are dev/test stand-ins, and the C++ apps talk to them directly over TCP/pty for
hardware-free testing.

Design notes: messages are plain structs whose fields map 1:1 onto the big-endian wire body;
`decode()` returns a `std::variant` (switch on it with `std::get_if` / `std::visit`).

## Build (local)

Needs a C++20 compiler and CMake ≥ 3.16.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure     # GoogleTest suite
```

| CMake option | Default | Meaning |
|--------------|---------|---------|
| `SERIAL_LINK_BUILD_APPS` | `ON` | build the three `rs422-*` apps |
| `SERIAL_LINK_BUILD_TESTS` | `ON` | build the GoogleTest suite (system GTest if found, else fetches it) |

`cmake --install build --prefix <dir>` installs `libserial_link.a`, the public headers, and the
app binaries under `lib/`, `include/`, `bin/`.

## Run (no hardware)

Two terminals over TCP. The Python drivers live in `tools/` (run from there, or install per
[tools/README.md](tools/README.md)):

```bash
# terminal 1 — C++ live display (listener)
./build/rs422-monitor --tcp 127.0.0.1:5555 --listen
# terminal 2 — the Python simulated sensor (dials in)
cd tools && python -m apps.sensor_sim --tcp 127.0.0.1:5555
```

Register C2 against the Python device simulator:

```bash
cd tools && python -m apps.device_sim --tcp 127.0.0.1:5556 --listen --seed 0x10=0x1234  # term 1
./build/rs422-c2 --tcp 127.0.0.1:5556 read 0x10 2                                        # term 2
```

All apps share the transport flags: `--port DEV` (real cable) · `--tcp HOST:PORT` · `--pty` ·
`--baud` · `--listen` (with `--tcp`).

## Containers

Two Containerfiles (podman/docker); **build context is the repo root**:

```bash
# Runtime image — slim, just the app binaries; libstdc++/libgcc linked statically.
podman build -f Containerfile.runtime -t rs422-cpp .
podman run --rm -it -p 5555:5555 rs422-cpp rs422-monitor --tcp 0.0.0.0:5555 --listen
podman run --rm -it --device /dev/ttyUSB0 rs422-cpp rs422-monitor --port /dev/ttyUSB0

# Build image — full toolchain (GCC + CMake + git); compiles + installs to /opt/serial_link.
podman build -f Containerfile.build -t rs422-cpp-build .
podman run --rm rs422-cpp-build ctest --test-dir /build --output-on-failure
```

For a serial device, pass it through with `--device /dev/ttyUSB0`; the runtime image's user is
in the `dialout` group.

## Yocto integration

Stock CMake project — in your recipe, `inherit cmake`:

```bitbake
DEPENDS = "googletest"                            # only if you build the ptest package
EXTRA_OECMAKE = "-DSERIAL_LINK_BUILD_TESTS=OFF"   # skip tests (and the GTest fetch) in the image
```

`do_install` is handled by the `install(TARGETS …)` rules (binaries → `${bindir}`,
`libserial_link.a` → `${libdir}`, headers → `${includedir}`). Keep tests OFF for image builds so
the build never fetches GoogleTest; enable it only in a ptest recipe that `DEPENDS` on it.

## Cross-language interop

The port keeps the wire format identical to Python, confirmed end-to-end:

- **Python `sensor_sim` → C++ `rs422-monitor`** — the C++ receiver decodes the Python attitude
  stream (big-endian float32) with zero bad frames.
- **C++ `rs422-c2` → Python `device_sim`** — C++ encodes the request, Python decodes and replies,
  C++ decodes the reply. Both directions across the boundary.
- **C++ `rs422-attitude-publisher` → C++ `rs422-monitor`** — CSV replay round-trips.

---

# Python test drivers & framework

See [tools/README.md](tools/README.md). Quick start:

```bash
cd tools && python -m pip install -e ".[dev]"
python -m pytest -q                                        # the Python test suite
python -m apps.monitor --tcp 127.0.0.1:5555 --listen       # a driver
```

Containerised integration environments (no hardware) live in
[environments/](environments/):

```bash
./environments/streaming/run.sh   # attitude: sensor_sim -> monitor
./environments/c2/run.sh          # register C2: c2 drives device_sim, then exits
```

For the full menu of test paths, see [docs/simulation-testing.md](docs/simulation-testing.md)
and [docs/hardware-testing.md](docs/hardware-testing.md).
