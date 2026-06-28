"""End-to-end C2: a c2-style host issues requests, device_sim answers, over a
real socket pair. Also covers link.request timeout."""

import socket
import threading

from serial_link import (
    FramedLink, LoopbackTransport,
    ReadRegister, WriteRegister, ReadResponse, WriteAck, Nak,
)
from apps.device_sim.registers import RegisterStore, respond


def _run_device(link, store):
    for rf in link.receive():     # returns when the peer (host) closes -> EOF
        if not rf.ok:
            continue
        reply = respond(store, rf.message)
        if reply is not None:
            link.send(reply)


def test_c2_read_write_cycle_over_loopback():
    a, b = socket.socketpair()
    host = FramedLink(LoopbackTransport(a))
    dev = FramedLink(LoopbackTransport(b))
    store = RegisterStore(size=64, seed={0x10: 0x1111})

    t = threading.Thread(target=_run_device, args=(dev, store), daemon=True)
    t.start()

    # read seeded register
    r = host.request(ReadRegister(addr=0x10, count=1), timeout=2)
    assert r.ok and isinstance(r.message, ReadResponse)
    assert r.message.values == [0x1111]

    # write then read back
    w = host.request(WriteRegister(addr=0x20, value=0xBEEF), timeout=2)
    assert isinstance(w.message, WriteAck) and w.message.value == 0xBEEF
    r2 = host.request(ReadRegister(addr=0x20, count=1), timeout=2)
    assert r2.message.values == [0xBEEF]

    # multi-register read
    r3 = host.request(ReadRegister(addr=0x10, count=3), timeout=2)
    assert len(r3.message.values) == 3

    # bad address -> Nak
    bad = host.request(ReadRegister(addr=100, count=10), timeout=2)
    assert isinstance(bad.message, Nak)

    # close the host so the device's receive() hits EOF and the thread exits
    host.close()
    t.join(timeout=2)
    dev.close()


def test_request_timeout_returns_none():
    # No peer ever replies; the request should time out and return None.
    a, b = socket.socketpair()
    host = FramedLink(LoopbackTransport(a))
    reply = host.request(ReadRegister(addr=0, count=1), timeout=0.2)
    assert reply is None
    host.close()
    b.close()
