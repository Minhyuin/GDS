import struct

from .constants import CommandType, FrameType, PERIOD_SAMPLE_COUNT, normalize_channel
from .frame import build_frame


def ping_frame():
    return build_frame(FrameType.COMMAND, bytes([CommandType.PING]))


def sync_frame():
    return build_frame(FrameType.COMMAND, bytes([CommandType.SYNC]))


def point_set_frame(channel, voltage):
    ch = normalize_channel(channel)
    payload = bytes([CommandType.POINT_SET, ch]) + struct.pack("<f", float(voltage))
    return build_frame(FrameType.COMMAND, payload)


def sequence_set_frame(channel, samples):
    ch = normalize_channel(channel)
    values = [float(sample) for sample in samples]
    if not 1 <= len(values) <= PERIOD_SAMPLE_COUNT:
        raise ValueError(f"SEQUENCE_SET requires 1..{PERIOD_SAMPLE_COUNT} samples")
    payload = bytes([CommandType.SEQUENCE_SET, ch]) + struct.pack("<H", len(values))
    payload += b"".join(struct.pack("<f", value) for value in values)
    return build_frame(FrameType.COMMAND, payload)


def timer_set_frame(channel, sample_rate_hz):
    ch = normalize_channel(channel)
    payload = bytes([CommandType.TIMER_SET, ch]) + struct.pack("<f", float(sample_rate_hz))
    return build_frame(FrameType.COMMAND, payload)
