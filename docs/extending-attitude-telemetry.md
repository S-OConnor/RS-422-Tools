# Extending the attitude stream with new data types

How to add new data to the streaming attitude telemetry — in **both** the C++ port
(the production apps, repo root) and the Python reference/test drivers (`tools/`).

There are two kinds of change, covered separately below:

- **[Change A — add a field to `AttitudeSample`](#change-a--add-a-field-to-attitudesample)**
  (e.g. angular rate, a temperature, a status byte). Grows the existing 20‑byte body.
- **[Change B — add a new telemetry message type](#change-b--add-a-new-telemetry-message-type)**
  (a second message in the `0x20`–`0x2F` range, e.g. an IMU/GPS packet alongside attitude).

> **The one rule that governs everything:** the wire format is defined once and both
> languages must stay **byte‑for‑byte identical**. The Python `serial_link` package is the
> reference; the C++ port mirrors it. A field added on one side but not the other changes the
> body size, and the mismatched receiver rejects every frame (`CodecError` / bad‑FCS count),
> so **the two sides must move together** or they stop interoperating. The
> [Python `sensor_sim` → C++ `rs422-monitor`](../README.md#cross-language-interop) interop path
> is the canary — run it after any change (see [Verify](#verify)).

---

## The wire contract (quick reference)

An encoded message is an **INFO field**: `type_id (1 byte) | body`. The body is
**big‑endian** (network order), fields packed back‑to‑back with **no padding**, in declared
order. `AttitudeSample` today (type `0x20`, 20‑byte body):

| Field  | Wire type | Python fmt | Bytes | Meaning |
|--------|-----------|------------|-------|---------|
| `seq`  | u32       | `I`        | 4     | per‑sample counter → drop/gap detection |
| `t_ms` | u32       | `I`        | 4     | sensor timestamp, ms since start |
| `roll` | f32       | `f`        | 4     | degrees |
| `pitch`| f32       | `f`        | 4     | degrees |
| `yaw`  | f32       | `f`        | 4     | degrees |

**Type‑id ranges** (`include/serial_link/messages.hpp` / `codec/messages/__init__.py`):

| Range        | Domain |
|--------------|--------|
| `0x01`–`0x0F`| register command/control requests |
| `0x20`–`0x2F`| **attitude / motion telemetry** ← new streaming types go here |
| `0x80`–`0x8F`| register responses |
| `0xFF`       | Nak (error reply) |

**Type ⇄ codec‑primitive mapping** — this is the crux of keeping the two languages in sync.
A Python `Field(name, fmt)` maps to a C++ `put_*` (encode) / `get_*` (decode) pair from
[`common.hpp`](../include/serial_link/common.hpp):

| Python `struct` fmt | Wire type | C++ encode (`put_*`) | C++ decode (`get_*`) |
|---------------------|-----------|----------------------|----------------------|
| `B`                 | u8        | `put_u8(b, v)`       | `p[i]`               |
| `H`                 | u16       | `put_u16be(b, v)`    | `get_u16be(p+i)`     |
| `I`                 | u32       | `put_u32be(b, v)`    | `get_u32be(p+i)`     |
| `f`                 | f32       | `put_f32be(b, v)`    | `get_f32be(p+i)`     |

> Only these four helpers exist today. If your new field needs another type — `i` (int32),
> `q` (int64), `d` (float64), `h` (int16) — add the matching `put_*`/`get_*` pair to
> [`common.hpp`](../include/serial_link/common.hpp) first. (Signed ints can reuse the unsigned
> put/get with a cast; `double` needs an 8‑byte variant of the float helpers.)

---

## Change A — add a field to `AttitudeSample`

Worked example: append a board‑temperature field **`temp_c` (f32, °C)**. The body grows
20 → 24 bytes. Substitute your own field/type; for several fields, repeat each step per field.

### 1. Python — the codec (the source of truth)

Add one `Field` to `FIELDS` in
[`tools/serial_link/codec/messages/attitude.py`](../tools/serial_link/codec/messages/attitude.py).
That is the **entire** codec change — the metaclass recompiles the `struct` and pack/unpack
follow automatically:

```python
    FIELDS = (
        Field("seq", "I"),      # u32
        Field("t_ms", "I"),     # u32
        Field("roll", "f"),     # f32: degrees
        Field("pitch", "f"),    # f32: degrees
        Field("yaw", "f"),      # f32: degrees
        Field("temp_c", "f"),   # NEW — f32: board temperature, °C
    )
```

### 2. C++ — mirror the codec (three files)

**[`include/serial_link/messages.hpp`](../include/serial_link/messages.hpp)** — add the member
and extend `operator==` (and bump the “20‑byte body” comment to 24):

```cpp
    float yaw = 0;            // degrees
    float temp_c = 0;        // NEW — board temperature, °C

    Bytes encode() const;
    static AttitudeSample unpack_body(const std::uint8_t* body, std::size_t n);
    bool operator==(const AttitudeSample& o) const {
        return seq == o.seq && t_ms == o.t_ms && roll == o.roll &&
               pitch == o.pitch && yaw == o.yaw && temp_c == o.temp_c;   // NEW
    }
```

**[`src/messages.cpp`](../src/messages.cpp)** — `encode()` appends the field; `unpack_body()`
reads it and the expected size grows by 4:

```cpp
Bytes AttitudeSample::encode() const {
    Bytes b;
    b.reserve(25);                 // was 21 (1 + 24)
    put_u8(b, TYPE_ID);
    put_u32be(b, seq);
    put_u32be(b, t_ms);
    put_f32be(b, roll);
    put_f32be(b, pitch);
    put_f32be(b, yaw);
    put_f32be(b, temp_c);          // NEW
    return b;
}

AttitudeSample AttitudeSample::unpack_body(const std::uint8_t* p, std::size_t n) {
    require_size("AttitudeSample", n, 24);   // was 20
    AttitudeSample m;
    m.seq = get_u32be(p);
    m.t_ms = get_u32be(p + 4);
    m.roll = get_f32be(p + 8);
    m.pitch = get_f32be(p + 12);
    m.yaw = get_f32be(p + 16);
    m.temp_c = get_f32be(p + 20);            // NEW
    return m;
}
```

**[`src/codec.cpp`](../src/codec.cpp)** — extend the `AttitudeSample` branch of `to_string()`
so logs show the field.

### 3. Producers and consumers (both languages)

The field is now on the wire; make the apps populate and surface it. Anywhere an
`AttitudeSample` is **constructed**, set the new field; anywhere it is **read**, use it.

| Where | Python | C++ | Edit |
|-------|--------|-----|------|
| Synth source (Python‑only sim) | [`sensor_sim.py`](../tools/apps/sensor_sim/sensor_sim.py) | — | set `temp_c=…` in the `AttitudeSample(...)` it builds (extend the motion model) |
| CSV replay (TX) | [`attitude_publisher.py`](../tools/apps/attitude_publisher/attitude_publisher.py) | [`apps/attitude_publisher/main.cpp`](../apps/attitude_publisher/main.cpp) | set the field from the CSV row |
| Live display (RX) | [`monitor.py`](../tools/apps/monitor/monitor.py) `format_line` | [`apps/monitor/main.cpp`](../apps/monitor/main.cpp) `format_line` | add it to the readout line |

### 4. CSV logging — keep the recording round‑trippable

`monitor --log` records samples and `attitude_publisher` replays them, so the CSV must carry the
new column or a recording loses it on replay. Both `attitude_log` implementations must stay in
lock‑step (same columns, same order, same formatting — the C++ file notes it matches Python
“byte‑for‑byte”).

**Python — [`tools/apps/attitude_log.py`](../tools/apps/attitude_log.py):**
- `AttitudeCsvLogger.FIELDS` — add `"temp_c"`.
- `AttitudeCsvLogger.write()` — append `f"{sample.temp_c:.4f}"` (match column order).
- `AttitudeRow` namedtuple — add `temp_c`.
- `read_attitude_csv()` — parse it (`float(r["temp_c"])`). Keep it **optional** for backward
  compatibility with old CSVs unless the field is truly required — mirror how `seq`/`t_ms`
  default to `None`, and only add to `REQUIRED_COLUMNS` if a missing value can’t be defaulted.

**C++ — [`attitude_log.hpp`](../include/serial_link/attitude_log.hpp) + [`attitude_log.cpp`](../src/attitude_log.cpp):**
- `AttitudeRow` struct — add `float temp_c = 0;`.
- Header string in the `AttitudeCsvLogger` ctor — add `,temp_c`.
- `write()` `fprintf` — add a `,%.4f` and the argument (same order as Python).
- `read_attitude_csv()` — look the column up and parse it (optional, like `seq`/`t_ms`).

### 5. Tests

Update the codec round‑trip/layout tests, which pin the exact byte layout, in **both**
[`tools/tests/test_attitude_codec.py`](../tools/tests/test_attitude_codec.py) and
[`tests/test_attitude_codec.cpp`](../tests/test_attitude_codec.cpp) — the byte‑size asserts
(20→24, 21→25) and the `>IIfff…` expected‑bytes will fail until updated. Also touch the CSV
logger tests (`test_attitude_log.py`) and, if you changed pacing/producers, the stream/publisher
tests.

---

## Change B — add a new telemetry message type

Use this when the new data is its own packet rather than more fields on `AttitudeSample`
(e.g. a lower‑rate GPS fix, or raw IMU alongside the fused attitude). Pick the next free id in
**`0x20`–`0x2F`** (say `0x21`). This is **additive**: a receiver that doesn’t know `0x21`
raises `UnknownMessage` / counts it as an unknown frame and keeps going — old and new can
coexist, unlike Change A.

### Python (declare + register)

1. **New module** `tools/serial_link/codec/messages/imu.py` — one class, same shape as
   [`attitude.py`](../tools/serial_link/codec/messages/attitude.py). Set `TYPE_ID = 0x21` and
   the `FIELDS`. (For a trailing variable‑length list, add a single `ARRAY = Field(...)` — see
   [`read_response.py`](../tools/serial_link/codec/messages/read_response.py).)
2. **Register it** by importing in
   [`codec/messages/__init__.py`](../tools/serial_link/codec/messages/__init__.py) (the import
   itself registers the `TYPE_ID`) and adding it to that file’s `__all__`.
3. **Re‑export** (optional convenience) in
   [`codec/__init__.py`](../tools/serial_link/codec/__init__.py) and
   [`serial_link/__init__.py`](../tools/serial_link/__init__.py) so callers can
   `from serial_link import Imu`.

### C++ (declare + implement + register)

1. **Declare** the struct in [`messages.hpp`](../include/serial_link/messages.hpp): `TYPE_ID`,
   the fields, `encode()`, `static unpack_body()`, `operator==` — copy the `AttitudeSample`
   struct as a template.
2. **Implement** `encode()` / `unpack_body()` in [`messages.cpp`](../src/messages.cpp) using the
   `put_*`/`get_*` helpers (match the Python `FIELDS` order exactly).
3. **Register** in [`codec.hpp`](../include/serial_link/codec.hpp): add the type to the
   `Message` variant, `using Message = std::variant<…, Imu>;`.
4. **Dispatch** in [`codec.cpp`](../src/codec.cpp): add a `case Imu::TYPE_ID:` to `decode()` and
   an `Imu` branch to `to_string()`.

### Producers / consumers / tests

Emit the new message where it originates and handle it where consumed (e.g. `monitor` currently
ignores non‑`AttitudeSample` frames — add an `isinstance` / `std::holds_alternative<Imu>` branch
if it should react). Add a new codec test pair mirroring the attitude tests.

---

## Verify

Run both suites, then the cross‑language interop check — a size/layout mismatch shows up
immediately as bad frames on the receiver.

```bash
# Python
cd tools && python -m pytest -q

# C++
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
ctest --test-dir build --output-on-failure

# Interop canary (two terminals) — Python source, C++ receiver: expect 0 bad frames
./build/rs422-monitor --tcp 127.0.0.1:5555 --listen        # term 1
cd tools && python -m apps.sensor_sim --tcp 127.0.0.1:5555  # term 2
```

See [simulation-testing.md](simulation-testing.md) for the full no‑hardware menu.

---

## Checklist

**Change A — new field on `AttitudeSample`:**

- [ ] Python `attitude.py` — add `Field`
- [ ] C++ `messages.hpp` — member + `operator==` (+ size comment)
- [ ] C++ `messages.cpp` — `encode()` + `unpack_body()` (`require_size`, offset)
- [ ] C++ `codec.cpp` — `to_string()`
- [ ] Producers set it: `sensor_sim.py`, `attitude_publisher.py`, `attitude_publisher/main.cpp`
- [ ] Consumers show it: `monitor.py`, `monitor/main.cpp`
- [ ] CSV both sides: `attitude_log.py` + `attitude_log.hpp`/`.cpp` (header, write, row, read)
- [ ] Tests both sides: `test_attitude_codec.{py,cpp}`, `test_attitude_log.py`
- [ ] New non‑`B/H/I/f` type? add `put_*`/`get_*` to `common.hpp` first
- [ ] Both suites green + interop canary clean

**Change B — new message type (`0x20`–`0x2F`):**

- [ ] Python: new module, import in `messages/__init__.py`, re‑exports
- [ ] C++: struct in `messages.hpp`, impl in `messages.cpp`, add to `Message` variant in
      `codec.hpp`, `case` + `to_string()` in `codec.cpp`
- [ ] Emit/handle in the relevant apps
- [ ] New codec test pair
- [ ] Both suites green + interop canary clean
