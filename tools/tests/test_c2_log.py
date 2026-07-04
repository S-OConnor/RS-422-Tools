import csv

from serial_link import ReadRegister, WriteRegister, ReadResponse, WriteAck, Nak
from apps.c2_log import C2CsvLogger


def _rows(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def test_logs_each_transaction_type(tmp_path):
    path = tmp_path / "c2.csv"
    with C2CsvLogger(str(path)) as log:
        log.log_txn("host", ReadRegister(addr=0x10, count=2),
                    ReadResponse(addr=0x10, values=[0x0011, 0x0022]), ts=1.0)
        log.log_txn("host", WriteRegister(addr=0x20, value=0xBEEF),
                    WriteAck(addr=0x20, value=0xBEEF, status=0), ts=2.0)
        log.log_txn("host", ReadRegister(addr=100, count=5),
                    Nak(code=2, addr=100), ts=3.0)
        log.log_txn("host", ReadRegister(addr=0, count=1), None, ts=4.0)
        assert log.count == 4

    rows = _rows(path)
    assert [r["result"] for r in rows] == ["ok", "ok", "nak", "timeout"]
    assert rows[0]["op"] == "read" and rows[0]["addr"] == "0x0010"
    assert rows[0]["detail"] == "0x0011 0x0022"
    assert rows[1]["op"] == "write" and rows[1]["detail"] == "status=0"
    assert rows[2]["detail"] == "bad address"
    assert rows[0]["ts_unix"] == "1.000000"


def test_appends_across_logger_instances(tmp_path):
    path = tmp_path / "audit.csv"
    with C2CsvLogger(str(path)) as log:
        log.log_txn("host", ReadRegister(addr=1, count=1),
                    ReadResponse(addr=1, values=[7]), ts=1.0)
    # a second run appends rather than truncating, and writes no second header
    with C2CsvLogger(str(path)) as log:
        log.log_txn("device", WriteRegister(addr=2, value=9),
                    WriteAck(addr=2, value=9, status=0), ts=2.0)

    rows = _rows(path)
    assert len(rows) == 2
    assert rows[0]["role"] == "host" and rows[1]["role"] == "device"


def test_device_role_logged(tmp_path):
    path = tmp_path / "dev.csv"
    with C2CsvLogger(str(path)) as log:
        log.log_txn("device", WriteRegister(addr=5, value=0x00FF),
                    WriteAck(addr=5, value=0x00FF, status=0), ts=1.0)
    assert _rows(path)[0]["role"] == "device"
