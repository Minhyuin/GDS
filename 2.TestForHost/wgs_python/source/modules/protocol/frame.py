import struct

from .constants import FrameType, SOF


def build_frame(frame_type, payload):
    payload = bytes(payload)
    if len(payload) > 0xFFFF:
        raise ValueError("Payload is too large for uint16 length")
    return bytes([SOF, int(FrameType(frame_type))]) + struct.pack("<H", len(payload)) + payload


def bytes_to_hex(data):
    return bytes(data).hex(" ").upper()
