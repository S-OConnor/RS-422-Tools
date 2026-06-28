"""Tests for the file-replay publisher and the CSV reader it uses."""

import socket

import pytest

from serial_link import FramedLink, LoopbackTransport, AttitudeSample
from apps.attitude_log import AttitudeCsvLogger, read_attitude_csv, AttitudeRow
from apps.attitude_publisher import publish


def _write_csv(path, samples):
    with AttitudeCsvLogger(str(path)) as log:
        for i, s in enumerate(samples):
            log.write(s, rx_unix=1000.0 + i)


def test_reader_roundtrips_logger_format(tmp_path):
    path = tmp_path / "rec.csv"
    samples = [
        AttitudeSample(seq=0, t_ms=0, roll=1.0, pitch=-2.0, yaw=3.0),
        AttitudeSample(seq=1, t_ms=100, roll=12.5, pitch=-3.25, yaw=180.0),
    ]
    _write_csv(path, samples)
    rows = read_attitude_csv(str(path))
    assert [r.seq for r in rows] == [0, 1]
    assert rows[1].t_ms == 100
    assert rows[1].roll == 12.5 and rows[1].yaw == 180.0


def test_reader_optional_columns(tmp_path):
    # only roll/pitch/yaw present -> seq/t_ms come back None
    path = tmp_path / "min.csv"
    path.write_text("roll,pitch,yaw\n1.0,2.0,3.0\n4.0,5.0,6.0\n")
    rows = read_attitude_csv(str(path))
    assert rows[0] == AttitudeRow(seq=None, t_ms=None, roll=1.0, pitch=2.0, yaw=3.0)


def test_reader_missing_required_column_raises(tmp_path):
    path = tmp_path / "bad.csv"
    path.write_text("roll,pitch\n1.0,2.0\n")
    with pytest.raises(ValueError):
        read_attitude_csv(str(path))


def test_reader_empty_raises(tmp_path):
    path = tmp_path / "empty.csv"
    path.write_text("roll,pitch,yaw\n")
    with pytest.raises(ValueError):
        read_attitude_csv(str(path))


def test_publish_over_loopback_preserves_values_and_seq(tmp_path):
    path = tmp_path / "rec.csv"
    samples = [
        AttitudeSample(seq=10, t_ms=0, roll=1.0, pitch=2.0, yaw=3.0),
        AttitudeSample(seq=11, t_ms=100, roll=-4.5, pitch=6.25, yaw=359.9),
        AttitudeSample(seq=12, t_ms=200, roll=7.0, pitch=-8.0, yaw=90.0),
    ]
    _write_csv(path, samples)
    rows = read_attitude_csv(str(path))

    a, b = socket.socketpair()
    tx = FramedLink(LoopbackTransport(a))
    rx = FramedLink(LoopbackTransport(b))

    n = publish(tx, rows, rate=100.0, pace=False)
    tx.close()
    assert n == 3

    got = [rf.message for rf in rx.receive()]
    rx.close()
    assert len(got) == 3
    # single pass preserves the file's seq
    assert [m.seq for m in got] == [10, 11, 12]
    for m, s in zip(got, samples):
        assert m.roll == pytest.approx(s.roll, abs=1e-3)
        assert m.yaw == pytest.approx(s.yaw, abs=1e-3)


def test_loop_renumbers_seq_monotonically(tmp_path):
    path = tmp_path / "min.csv"
    path.write_text("roll,pitch,yaw\n1.0,2.0,3.0\n4.0,5.0,6.0\n")
    rows = read_attitude_csv(str(path))

    a, b = socket.socketpair()
    tx = FramedLink(LoopbackTransport(a))
    rx = FramedLink(LoopbackTransport(b))

    # loop a 2-row file, capped at 5 samples
    n = publish(tx, rows, rate=100.0, loop=True, count=5, pace=False)
    tx.close()
    assert n == 5

    got = [rf.message for rf in rx.receive()]
    rx.close()
    assert [m.seq for m in got] == [0, 1, 2, 3, 4]   # monotonic across loops


def test_realtime_requires_t_ms(tmp_path):
    path = tmp_path / "min.csv"
    path.write_text("roll,pitch,yaw\n1.0,2.0,3.0\n")
    rows = read_attitude_csv(str(path))
    a, b = socket.socketpair()
    tx = FramedLink(LoopbackTransport(a))
    with pytest.raises(ValueError):
        publish(tx, rows, realtime=True, pace=False)
    tx.close()
    b.close()
