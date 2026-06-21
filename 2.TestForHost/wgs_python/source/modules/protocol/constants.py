from enum import IntEnum


SOF = 0xA5
PERIOD_SAMPLE_COUNT = 1000
MAX_VOLTAGE = 3.3
MIN_VOLTAGE = 0.0


class FrameType(IntEnum):
    COMMAND = 0x01
    ACK = 0x02
    NACK = 0x03
    EVENT = 0x04
    STATUS = 0x05


class CommandType(IntEnum):
    PING = 0x00
    POINT_SET = 0x10
    SEQUENCE_SET = 0x20
    TIMER_SET = 0x40


class Channel(IntEnum):
    CH1 = 0x01
    CH2 = 0x02


class AckStatus(IntEnum):
    IDLE = 0x00
    DONE = 0x01
    BUSY = 0x02
    ERROR = 0x03


class ErrorCode(IntEnum):
    NONE = 0x00
    BAD_FRAME = 0x01
    BAD_COMMAND = 0x02
    BAD_LENGTH = 0x03
    INVALID_STATE = 0x04
    NO_SPACE = 0x05


def normalize_channel(channel):
    if isinstance(channel, Channel):
        return channel
    if isinstance(channel, int):
        return Channel(channel)
    text = str(channel).strip().upper()
    aliases = {
        "1": Channel.CH1,
        "CH1": Channel.CH1,
        "X": Channel.CH1,
        "DAC1": Channel.CH1,
        "DAC_OUT1": Channel.CH1,
        "2": Channel.CH2,
        "CH2": Channel.CH2,
        "Y": Channel.CH2,
        "DAC2": Channel.CH2,
        "DAC_OUT2": Channel.CH2,
    }
    if text not in aliases:
        raise ValueError(f"Unsupported channel: {channel!r}")
    return aliases[text]
