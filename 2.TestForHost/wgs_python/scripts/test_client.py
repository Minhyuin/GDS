import sys
from pathlib import Path
import struct


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT))

from source.modules.client import wgs_client  # noqa: E402


class FakeTransport:
    def __init__(self, *args, **kwargs):
        self.written = b""

    @property
    def is_open(self):
        return True

    def open(self):
        pass

    def close(self):
        pass

    def write(self, data):
        self.written = bytes(data)

    def read_frame(self, timeout):
        return 0x02, bytes([0x01, 0x00]), bytes.fromhex("A5 02 02 00 01 00")

    def flush_input(self):
        pass


def test_client_sync_sends_sync_frame():
    original_transport = wgs_client.SerialTransport
    wgs_client.SerialTransport = FakeTransport
    try:
        client = wgs_client.WgsClient("COM_TEST")
        reply = client.sync()
    finally:
        wgs_client.SerialTransport = original_transport

    expected = bytes.fromhex("A5 01 01 00 01")
    if client.last_tx != expected:
        raise AssertionError(
            "sync() should send the SYNC command frame\n"
            f"expected: {expected.hex(' ').upper()}\n"
            f"actual:   {client.last_tx.hex(' ').upper()}"
        )
    if not reply.ok:
        raise AssertionError(f"sync() should return an OK ACK, got {reply.summary()}")


def test_client_point_set_sends_ack_pulse_width():
    original_transport = wgs_client.SerialTransport
    wgs_client.SerialTransport = FakeTransport
    try:
        client = wgs_client.WgsClient("COM_TEST")
        reply = client.point_set("CH2", 1.5, ack_pulse_us=50.0)
    finally:
        wgs_client.SerialTransport = original_transport

    expected = bytes.fromhex("A5 01 0A 00 10 02") + struct.pack("<f", 1.5) + struct.pack("<f", 50.0)
    if client.last_tx != expected:
        raise AssertionError(
            "point_set() should send ACK pulse width\n"
            f"expected: {expected.hex(' ').upper()}\n"
            f"actual:   {client.last_tx.hex(' ').upper()}"
        )
    if not reply.ok:
        raise AssertionError(f"point_set() should return an OK ACK, got {reply.summary()}")


def run_all():
    tests = [test_client_sync_sends_sync_frame, test_client_point_set_sends_ack_pulse_width]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")


if __name__ == "__main__":
    run_all()
