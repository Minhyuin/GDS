import math
import struct

from .constants import (
    CommandType,
    FrameType,
    MAX_SAMPLE_RATE_HZ,
    MAX_WAVEFORM_LENGTH_SA,
    POINT_PULSE_WIDTH_MAX_US,
    POINT_PULSE_WIDTH_MIN_US,
    PERIOD_SAMPLE_COUNT,
    normalize_channel,
)
from .frame import build_frame


def ping_frame():
    return build_frame(FrameType.COMMAND, bytes([CommandType.PING]))


def sync_frame():
    return build_frame(FrameType.COMMAND, bytes([CommandType.SYNC]))


def point_set_frame(channel, voltage):
    ch = normalize_channel(channel)
    payload = bytes([CommandType.POINT_SET, ch]) + struct.pack("<f", float(voltage))
    return build_frame(FrameType.COMMAND, payload)


def point2_set_frame(ch1_voltage, ch2_voltage):
    payload = bytes([CommandType.POINT2_SET]) + struct.pack("<f", float(ch1_voltage)) + struct.pack("<f", float(ch2_voltage))
    return build_frame(FrameType.COMMAND, payload)


def point_pulse_width_set_frame(pulse_width_us):
    pulse_width_us = float(pulse_width_us)
    if not math.isfinite(pulse_width_us) or not POINT_PULSE_WIDTH_MIN_US <= pulse_width_us <= POINT_PULSE_WIDTH_MAX_US:
        raise ValueError(
            f"POINT_PULSE_WIDTH_SET pulse width must be {POINT_PULSE_WIDTH_MIN_US:g}..{POINT_PULSE_WIDTH_MAX_US:g} us"
        )
    payload = bytes([CommandType.POINT_PULSE_WIDTH_SET]) + struct.pack("<f", pulse_width_us)
    return build_frame(FrameType.COMMAND, payload)


def sequence_set_frame(channel, samples):
    ch = normalize_channel(channel)
    values = [float(sample) for sample in samples]
    if not 1 <= len(values) <= PERIOD_SAMPLE_COUNT:
        raise ValueError(f"SEQUENCE_SET requires 1..{PERIOD_SAMPLE_COUNT} samples")
    payload = bytes([CommandType.SEQUENCE_SET, ch]) + struct.pack("<H", len(values))
    payload += b"".join(struct.pack("<f", value) for value in values)
    return build_frame(FrameType.COMMAND, payload)


def waveform_config_frame(channel, length_sa, frequency_hz):
    ch = normalize_channel(channel)
    length_sa = int(length_sa)
    frequency_hz = float(frequency_hz)
    if not 1 <= length_sa <= MAX_WAVEFORM_LENGTH_SA:
        raise ValueError(f"WAVEFORM_CONFIG length must be 1..{MAX_WAVEFORM_LENGTH_SA} Sa")
    if not math.isfinite(frequency_hz) or frequency_hz <= 0.0:
        raise ValueError("WAVEFORM_CONFIG frequency must be a positive finite value")
    sample_rate_hz = length_sa * frequency_hz
    if sample_rate_hz > MAX_SAMPLE_RATE_HZ:
        raise ValueError(f"WAVEFORM_CONFIG sample rate must be <= {MAX_SAMPLE_RATE_HZ:g} Sa/s")
    payload = bytes([CommandType.WAVEFORM_CONFIG, ch]) + struct.pack("<H", length_sa)
    payload += struct.pack("<f", frequency_hz)
    return build_frame(FrameType.COMMAND, payload)


def timer_set_frame(channel, sample_rate_hz):
    ch = normalize_channel(channel)
    payload = bytes([CommandType.TIMER_SET, ch]) + struct.pack("<f", float(sample_rate_hz))
    return build_frame(FrameType.COMMAND, payload)
