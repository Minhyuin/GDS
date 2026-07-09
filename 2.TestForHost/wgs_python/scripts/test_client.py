import struct
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT))

from source.modules.client import wgs_client  # noqa: E402


class FakeTransport:
    def __init__(self, *args, **kwargs):
        self.written = b""
        self.writes = []
        self.read_count = 0

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
        self.read_count += 1
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


def test_client_point_set_sends_voltage_only():
    original_transport = wgs_client.SerialTransport
    wgs_client.SerialTransport = FakeTransport
    try:
        client = wgs_client.WgsClient("COM_TEST")
        reply = client.point_set("CH2", 1.5)
    finally:
        wgs_client.SerialTransport = original_transport

    expected = bytes.fromhex("A5 01 06 00 10 02") + struct.pack("<f", 1.5)
    if client.last_tx != expected:
        raise AssertionError(
            "point_set() should send only channel voltage\n"
            f"expected: {expected.hex(' ').upper()}\n"
            f"actual:   {client.last_tx.hex(' ').upper()}"
        )
    if not reply.ok:
        raise AssertionError(f"point_set() should return an OK ACK, got {reply.summary()}")


def test_client_point_set_no_reply_sends_no_ack_command_without_reading():
    original_transport = wgs_client.SerialTransport
    wgs_client.SerialTransport = FakeTransport
    try:
        client = wgs_client.WgsClient("COM_TEST")
        result = client.point_set_no_reply("CH2", 1.5)
    finally:
        wgs_client.SerialTransport = original_transport

    expected = bytes.fromhex("A5 01 06 00 12 02") + struct.pack("<f", 1.5)
    if client.last_tx != expected:
        raise AssertionError(
            "point_set_no_reply() should send POINT_SET_NO_ACK\n"
            f"expected: {expected.hex(' ').upper()}\n"
            f"actual:   {client.last_tx.hex(' ').upper()}"
        )
    if result is not None:
        raise AssertionError("point_set_no_reply() should not return a reply")
    if client.transport.read_count != 0:
        raise AssertionError("point_set_no_reply() should not read a firmware reply")


def test_client_point_set_no_ack_alias_sends_no_ack_command_without_reading():
    original_transport = wgs_client.SerialTransport
    wgs_client.SerialTransport = FakeTransport
    try:
        client = wgs_client.WgsClient("COM_TEST")
        result = client.point_set_no_ack("CH2", 1.5)
    finally:
        wgs_client.SerialTransport = original_transport

    expected = bytes.fromhex("A5 01 06 00 12 02") + struct.pack("<f", 1.5)
    if client.last_tx != expected:
        raise AssertionError(
            "point_set_no_ack() should send POINT_SET_NO_ACK\n"
            f"expected: {expected.hex(' ').upper()}\n"
            f"actual:   {client.last_tx.hex(' ').upper()}"
        )
    if result is not None:
        raise AssertionError("point_set_no_ack() should not return a reply")
    if client.transport.read_count != 0:
        raise AssertionError("point_set_no_ack() should not read a firmware reply")


def test_client_point2_set_sends_dual_voltage_frame():
    original_transport = wgs_client.SerialTransport
    wgs_client.SerialTransport = FakeTransport
    try:
        client = wgs_client.WgsClient("COM_TEST")
        reply = client.point2_set(1.5, 2.5)
    finally:
        wgs_client.SerialTransport = original_transport

    expected = bytes.fromhex("A5 01 09 00 11") + struct.pack("<f", 1.5) + struct.pack("<f", 2.5)
    if client.last_tx != expected:
        raise AssertionError(
            "point2_set() should send CH1 and CH2 voltages\n"
            f"expected: {expected.hex(' ').upper()}\n"
            f"actual:   {client.last_tx.hex(' ').upper()}"
        )
    if not reply.ok:
        raise AssertionError(f"point2_set() should return an OK ACK, got {reply.summary()}")


def test_client_point2_set_no_reply_sends_no_ack_command_without_reading():
    original_transport = wgs_client.SerialTransport
    wgs_client.SerialTransport = FakeTransport
    try:
        client = wgs_client.WgsClient("COM_TEST")
        result = client.point2_set_no_reply(1.5, 2.5)
    finally:
        wgs_client.SerialTransport = original_transport

    expected = bytes.fromhex("A5 01 09 00 13") + struct.pack("<f", 1.5) + struct.pack("<f", 2.5)
    if client.last_tx != expected:
        raise AssertionError(
            "point2_set_no_reply() should send POINT2_SET_NO_ACK\n"
            f"expected: {expected.hex(' ').upper()}\n"
            f"actual:   {client.last_tx.hex(' ').upper()}"
        )
    if result is not None:
        raise AssertionError("point2_set_no_reply() should not return a reply")
    if client.transport.read_count != 0:
        raise AssertionError("point2_set_no_reply() should not read a firmware reply")


def test_client_point2_set_no_ack_alias_sends_no_ack_command_without_reading():
    original_transport = wgs_client.SerialTransport
    wgs_client.SerialTransport = FakeTransport
    try:
        client = wgs_client.WgsClient("COM_TEST")
        result = client.point2_set_no_ack(1.5, 2.5)
    finally:
        wgs_client.SerialTransport = original_transport

    expected = bytes.fromhex("A5 01 09 00 13") + struct.pack("<f", 1.5) + struct.pack("<f", 2.5)
    if client.last_tx != expected:
        raise AssertionError(
            "point2_set_no_ack() should send POINT2_SET_NO_ACK\n"
            f"expected: {expected.hex(' ').upper()}\n"
            f"actual:   {client.last_tx.hex(' ').upper()}"
        )
    if result is not None:
        raise AssertionError("point2_set_no_ack() should not return a reply")
    if client.transport.read_count != 0:
        raise AssertionError("point2_set_no_ack() should not read a firmware reply")


def test_client_point_pulse_width_set_sends_global_width():
    original_transport = wgs_client.SerialTransport
    wgs_client.SerialTransport = FakeTransport
    try:
        client = wgs_client.WgsClient("COM_TEST")
        reply = client.point_pulse_width_set(50.0)
    finally:
        wgs_client.SerialTransport = original_transport

    expected = bytes.fromhex("A5 01 05 00 50") + struct.pack("<f", 50.0)
    if client.last_tx != expected:
        raise AssertionError(
            "point_pulse_width_set() should send the standalone pulse width frame\n"
            f"expected: {expected.hex(' ').upper()}\n"
            f"actual:   {client.last_tx.hex(' ').upper()}"
        )
    if not reply.ok:
        raise AssertionError(f"point_pulse_width_set() should return an OK ACK, got {reply.summary()}")


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
        test_client_point_set_sends_voltage_only,
        test_client_point_set_no_reply_sends_no_ack_command_without_reading,
        test_client_point_set_no_ack_alias_sends_no_ack_command_without_reading,
        test_client_point2_set_sends_dual_voltage_frame,
        test_client_point2_set_no_reply_sends_no_ack_command_without_reading,
        test_client_point2_set_no_ack_alias_sends_no_ack_command_without_reading,
        test_client_point_pulse_width_set_sends_global_width,
        test_client_sequence_set_configures_length_and_frequency_first,
    ]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")


if __name__ == "__main__":
    run_all()
