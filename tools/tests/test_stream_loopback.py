"""End-to-end: sensor_sim streams through a loopback; the monitor side decodes
it back, with contiguous seq and attitude matching the motion model."""

import socket

import pytest

from serial_link import FramedLink, LoopbackTransport, AttitudeSample
from apps.sensor_sim import stream, attitude_at


def test_stream_decodes_with_contiguous_seq_and_model_values():
    a, b = socket.socketpair()
    tx = FramedLink(LoopbackTransport(a))
    rx = FramedLink(LoopbackTransport(b))

    rate, n = 10.0, 25
    # Run the sensor flat-out (no real-time pacing) into one end of the pipe.
    stream(tx, rate=rate, count=n, pace=False)
    tx.close()  # EOF so rx.receive() terminates cleanly

    samples = []
    for rf in rx.receive():
        assert rf.ok and isinstance(rf.message, AttitudeSample)
        samples.append(rf.message)
    rx.close()

    assert len(samples) == n
    # seq is contiguous 0..n-1
    assert [s.seq for s in samples] == list(range(n))
    # values match the deterministic motion model at t = seq / rate
    for s in samples:
        roll, pitch, yaw = attitude_at(s.seq / rate)
        assert s.roll == pytest.approx(roll, abs=1e-2)
        assert s.pitch == pytest.approx(pitch, abs=1e-2)
        assert s.yaw == pytest.approx(yaw, abs=1e-2)
        assert s.t_ms == round((s.seq / rate) * 1000.0)


def test_monitor_format_line_smoke():
    from apps.monitor import format_line
    s = AttitudeSample(seq=7, t_ms=700, roll=-12.3, pitch=4.5, yaw=178.9)
    line = format_line(s, rate_hz=10.0, drops=0)
    assert "seq=7" in line and "178.9" in line and "rate=10.0Hz" in line
    assert "\n" not in line
