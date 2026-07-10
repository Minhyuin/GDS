import struct
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT))

from source.modules.protocol.codec import (  # noqa: E402
    ping_frame,
    point_fifo_append_frame,
    point_fifo_clear_frame,
    point_fifo_start_frame,
    point_fifo_stop_frame,
    point_set_no_ack_frame,
    point_set_frame,
    point2_fifo_append_frame,
    point2_fifo_clear_frame,
    point2_fifo_start_frame,
    point2_fifo_stop_frame,
    point2_set_no_ack_frame,
    point2_set_frame,
    point_pulse_width_set_frame,
    sequence_set_frame,
    sync_frame,
    timer_set_frame,
    waveform_config_frame,
)


def assert_equal(actual, expected, label):
    if actual != expected:
        raise AssertionError(
            f"{label}\nexpected: {expected.hex(' ').upper()}\nactual:   {actual.hex(' ').upper()}"
        )


def test_ping_frame():
    assert_equal(ping_frame(), bytes.fromhex("A5 01 01 00 00"), "PING frame")


def test_sync_frame():
    assert_equal(sync_frame(), bytes.fromhex("A5 01 01 00 01"), "SYNC frame")


def test_point_set_ch1_1v5_frame():
    expected = bytes.fromhex("A5 01 06 00 10 01") + struct.pack("<f", 1.5)
    assert_equal(point_set_frame("CH1", 1.5), expected, "POINT_SET CH1 1.5V frame")


def test_point_set_no_ack_ch1_1v5_frame():
    expected = bytes.fromhex("A5 01 06 00 12 01") + struct.pack("<f", 1.5)
    assert_equal(point_set_no_ack_frame("CH1", 1.5), expected, "POINT_SET_NO_ACK CH1 1.5V frame")


def test_point2_set_dual_voltage_frame():
    expected = bytes.fromhex("A5 01 09 00 11") + struct.pack("<f", 1.5) + struct.pack("<f", 2.5)
    assert_equal(point2_set_frame(1.5, 2.5), expected, "POINT2_SET CH1/CH2 voltage frame")


def test_point2_set_no_ack_dual_voltage_frame():
    expected = bytes.fromhex("A5 01 09 00 13") + struct.pack("<f", 1.5) + struct.pack("<f", 2.5)
    assert_equal(point2_set_no_ack_frame(1.5, 2.5), expected, "POINT2_SET_NO_ACK CH1/CH2 voltage frame")


def test_point_fifo_control_frames():
    assert_equal(point_fifo_clear_frame("CH1"), bytes.fromhex("A5 01 02 00 14 01"), "POINT_FIFO_CLEAR CH1 frame")
    assert_equal(point_fifo_start_frame("CH1"), bytes.fromhex("A5 01 02 00 16 01"), "POINT_FIFO_START CH1 frame")
    assert_equal(point_fifo_stop_frame("CH1"), bytes.fromhex("A5 01 02 00 17 01"), "POINT_FIFO_STOP CH1 frame")


def test_point_fifo_append_two_points_frame():
    expected = bytes.fromhex("A5 01 0C 00 15 02 02 00")
    expected += struct.pack("<f", 1.5)
    expected += struct.pack("<f", 1.25)
    assert_equal(
        point_fifo_append_frame("CH2", [1.5, 1.25]),
        expected,
        "POINT_FIFO_APPEND CH2 two points frame",
    )


def test_point2_fifo_control_frames():
    assert_equal(point2_fifo_clear_frame(), bytes.fromhex("A5 01 01 00 20"), "POINT2_FIFO_CLEAR frame")
    assert_equal(point2_fifo_start_frame(), bytes.fromhex("A5 01 01 00 22"), "POINT2_FIFO_START frame")
    assert_equal(point2_fifo_stop_frame(), bytes.fromhex("A5 01 01 00 23"), "POINT2_FIFO_STOP frame")


def test_point2_fifo_append_two_points_frame():
    expected = bytes.fromhex("A5 01 13 00 21 02 00")
    expected += struct.pack("<f", 1.5) + struct.pack("<f", 2.5)
    expected += struct.pack("<f", 1.25) + struct.pack("<f", 2.25)
    assert_equal(
        point2_fifo_append_frame([1.5, 1.25], [2.5, 2.25]),
        expected,
        "POINT2_FIFO_APPEND two points frame",
    )


def test_point_pulse_width_set_50us_frame():
    expected = bytes.fromhex("A5 01 05 00 50") + struct.pack("<f", 50.0)
    assert_equal(point_pulse_width_set_frame(50.0), expected, "POINT_PULSE_WIDTH_SET 50us frame")


def test_timer_set_ch2_100khz_frame():
    expected = bytes.fromhex("A5 01 06 00 40 02") + struct.pack("<f", 100000.0)
    assert_equal(timer_set_frame("CH2", 100000.0), expected, "TIMER_SET CH2 100kHz frame")


def test_waveform_config_ch2_256_points_12v5hz_frame():
    expected = bytes.fromhex("A5 01 08 00 30 02 00 01") + struct.pack("<f", 12.5)
    assert_equal(
        waveform_config_frame("CH2", 256, 12.5),
        expected,
        "WAVEFORM_CONFIG CH2 256Sa 12.5Hz frame",
    )


def test_sequence_set_1000_points_frame_size():
    frame = sequence_set_frame("CH2", [1.65] * 1000)
    expected_length = 1 + 1 + 2 + 1 + 1 + 2 + 1000 * 4
    if len(frame) != expected_length:
        raise AssertionError(f"SEQUENCE_SET frame length expected {expected_length}, got {len(frame)}")
    assert_equal(frame[:4], bytes.fromhex("A5 01 A4 0F"), "SEQUENCE_SET header")
    assert_equal(frame[4:8], bytes.fromhex("60 02 E8 03"), "SEQUENCE_SET payload prefix")


def run_all():
    tests = [
        test_ping_frame,
        test_sync_frame,
        test_point_set_ch1_1v5_frame,
        test_point_set_no_ack_ch1_1v5_frame,
        test_point2_set_dual_voltage_frame,
        test_point2_set_no_ack_dual_voltage_frame,
        test_point_fifo_control_frames,
        test_point_fifo_append_two_points_frame,
        test_point2_fifo_control_frames,
        test_point2_fifo_append_two_points_frame,
        test_point_pulse_width_set_50us_frame,
        test_timer_set_ch2_100khz_frame,
        test_waveform_config_ch2_256_points_12v5hz_frame,
        test_sequence_set_1000_points_frame_size,
    ]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")


if __name__ == "__main__":
    run_all()
