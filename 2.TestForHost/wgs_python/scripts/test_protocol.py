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


from source.modules.protocol.constants import AckStatus, ErrorCode, FrameType  # noqa: E402
from source.modules.protocol.reply import parse_reply  # noqa: E402


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


def test_raw_nack_unsupported_command_reply():
    raw = bytes.fromhex("A5 03 02 00 80 03")
    reply = parse_reply(raw[1], raw[4:], raw)
    if reply.summary() != "NACK status=ERROR detail=UNSUPPORTED_COMMAND":
        raise AssertionError(f"unexpected raw NACK summary: {reply.summary()}")
    if reply.raw != raw:
        raise AssertionError(f"unexpected raw NACK bytes: {reply.raw.hex(' ').upper()}")


def test_ack_status_firmware_contract():
    expected = [
        ("ACCEPTED", 0x00),
        ("DONE", 0x01),
        ("BUSY", 0x02),
        ("ERROR", 0x80),
    ]
    actual = [(status.name, int(status)) for status in AckStatus]
    if actual != expected:
        raise AssertionError(f"ACK status contract mismatch; expected: {expected}; actual: {actual}")

    for name, value in expected:
        status = AckStatus(value)
        if status.name != name or int(status) != value:
            raise AssertionError(f"ACK status {name}: expected {value:#04x}, got {int(status):#04x}")
        frame_type = FrameType.NACK if status == AckStatus.ERROR else FrameType.ACK
        raw = bytes([0xA5, int(frame_type), 2, 0, value, int(ErrorCode.NONE)])
        reply = parse_reply(frame_type, raw[4:], raw)
        if reply.status != status or reply.status.name != name:
            raise AssertionError(f"ACK status parse mismatch for {name}: {reply.summary()}")


def test_error_code_firmware_contract():
    expected = [
        ("NONE", 0x00),
        ("CRC_ERROR", 0x01),
        ("BAD_LENGTH", 0x02),
        ("UNSUPPORTED_COMMAND", 0x03),
        ("INVALID_STATE", 0x04),
        ("OUT_OF_RANGE", 0x05),
        ("BUFFER_FULL", 0x06),
        ("BAD_OFFSET", 0x07),
        ("WAVE_CRC_MISMATCH", 0x08),
        ("DMA_BUSY", 0x09),
        ("UNDERRUN", 0x0A),
        ("TRANSPORT_BUSY", 0x0B),
    ]
    actual = [(code.name, int(code)) for code in ErrorCode]
    if actual != expected:
        raise AssertionError(f"error code contract mismatch; expected: {expected}; actual: {actual}")

    for name, value in expected:
        code = ErrorCode(value)
        raw = bytes([0xA5, int(FrameType.NACK), 2, 0, int(AckStatus.ERROR), value])
        reply = parse_reply(FrameType.NACK, raw[4:], raw)
        expected_summary = f"NACK status=ERROR detail={name}"
        if reply.detail != code or reply.detail.name != name or reply.summary() != expected_summary:
            raise AssertionError(
                f"error code parse/name mismatch for {name}: expected {expected_summary}, got {reply.summary()}"
            )


def run_all():
    tests = [
        test_raw_nack_unsupported_command_reply,
        test_ack_status_firmware_contract,
        test_error_code_firmware_contract,
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
