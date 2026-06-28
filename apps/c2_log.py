"""CSV logging for register command & control.

One row per transaction, written from either side: the host (`c2`) logs the
request it sent and the reply it got; the device (`device_sim`) logs the request
it received and the reply it sent. Rows are appended (an audit trail across runs)
and flushed immediately so nothing is lost on Ctrl-C.

Columns: ts_unix, role, op, addr, arg, result, detail
  role    "host" | "device"
  op      "read" | "write" | "?"   (? = a non-request message)
  addr    register address (hex)
  arg     read: count   write: value
  result  "ok" | "nak" | "timeout" | "error" | "other"
  detail  read ok: the values   write ok: status   nak: reason
"""

import csv
import os
import time

from serial_link import ReadRegister, WriteRegister, ReadResponse, WriteAck, Nak


class C2CsvLogger:
    FIELDS = ["ts_unix", "role", "op", "addr", "arg", "result", "detail"]

    def __init__(self, path):
        self.path = path
        new = not (os.path.exists(path) and os.path.getsize(path) > 0)
        self._f = open(path, "a", newline="")
        self._w = csv.writer(self._f)
        if new:
            self._w.writerow(self.FIELDS)
            self._f.flush()
        self.count = 0

    def log_txn(self, role, request, reply, result=None, ts=None):
        """Record one transaction.

        ``request`` is the request message; ``reply`` is the reply message, or
        ``None`` (e.g. a timeout). ``result`` overrides the derived outcome
        (used for "timeout"/"error" on the host side).
        """
        if isinstance(request, ReadRegister):
            op, addr, arg = "read", request.addr, request.count
        elif isinstance(request, WriteRegister):
            op, addr, arg = "write", request.addr, request.value
        else:
            op, addr, arg = "?", 0, 0

        detail = ""
        if result is None:
            if reply is None:
                result = "timeout"
            elif isinstance(reply, Nak):
                result, detail = "nak", reply.reason()
            elif isinstance(reply, ReadResponse):
                result = "ok"
                detail = " ".join(f"0x{v:04X}" for v in reply.values)
            elif isinstance(reply, WriteAck):
                result, detail = "ok", f"status={reply.status}"
            else:
                result, detail = "other", repr(reply)

        self._w.writerow([
            f"{ts if ts is not None else time.time():.6f}",
            role, op, f"0x{addr:04X}", arg, result, detail,
        ])
        self._f.flush()
        self.count += 1

    def close(self):
        try:
            self._f.flush()
            self._f.close()
        except (OSError, ValueError):
            pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
