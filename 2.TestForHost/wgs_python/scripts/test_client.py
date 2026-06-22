import sys
from pathlib import Path
import struct


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT))

from source.modules.client import wgs_client  # noqa: E402


class FakeTransport:
    def __init__(self, *args, **kwargs):
        self.written = b""
        self.writes = []

    @property
    def is_open(self):
        return True

    def open(self):
        pass

    def close(self):
        pass

    def write(self, data):
        self.written = bytes(data)
        self.writes.append(self.written)

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


def test_client_sequence_set_configures_length_and_frequency_first():
    original_transport = wgs_client.SerialTransport
    wgs_client.SerialTransport = FakeTransport
    try:
        client = wgs_client.WgsClient("COM_TEST")
        reply = client.sequence_set("CH2", [1.0, 2.0, 3.0], frequency_hz=10.0)
        writes = list(client.transport.writes)
    finally:
        wgs_client.SerialTransport = original_transport

    expected_config = bytes.fromhex("A5 01 08 00 30 02 03 00") + struct.pack("<f", 10.0)
    expected_sequence = bytes.fromhex("A5 01 10 00 20 02 03 00")
    expected_sequence += struct.pack("<f", 1.0) + struct.pack("<f", 2.0) + struct.pack("<f", 3.0)
    if writes != [expected_config, expected_sequence]:
        actual = [item.hex(" ").upper() for item in writes]
        expected = [expected_config.hex(" ").upper(), expected_sequence.hex(" ").upper()]
        raise AssertionError(f"sequence_set() should configure before sequence\nexpected: {expected}\nactual:   {actual}")
    if not reply.ok:
        raise AssertionError(f"sequence_set() should return an OK ACK, got {reply.summary()}")


def run_all():
    tests = [
        test_client_sync_sends_sync_frame,
        test_client_point_set_sends_ack_pulse_width,
        test_client_sequence_set_configures_length_and_frequency_first,
    ]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")


if __name__ == "__main__":
    run_all()
