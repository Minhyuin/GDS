import time

import serial
from serial.tools import list_ports

from ..protocol.constants import SOF


def list_serial_ports():
    return [port.device for port in list_ports.comports()]


class SerialTransport:
    def __init__(self, port, baudrate=115200, timeout=0.2):
        self.port = port
        self.baudrate = int(baudrate)
        self.timeout = float(timeout)
        self._serial = None

    @property
    def is_open(self):
        return self._serial is not None and self._serial.is_open

    def open(self):
        if self.is_open:
            return
        self._serial = serial.Serial(
            port=self.port,
            baudrate=self.baudrate,
            timeout=self.timeout,
            write_timeout=self.timeout,
            dsrdtr=False,
            rtscts=False,
        )
        self._serial.dtr = False
        self._serial.rts = False
        time.sleep(0.05)
        self.flush_input()

    def close(self):
        if self._serial is not None:
            self._serial.close()
            self._serial = None

    def write(self, data):
        self._require_open()
        self._serial.write(bytes(data))
        self._serial.flush()

    def flush_input(self):
        if self.is_open:
            self._serial.reset_input_buffer()

    def read_frame(self, timeout=1.0):
        self._require_open()
        deadline = time.monotonic() + float(timeout)
        while time.monotonic() < deadline:
            first = self._serial.read(1)
            if not first:
                continue
            if first[0] == SOF:
                header_tail = self._read_exact(3, deadline)
                frame_type = header_tail[0]
                payload_length = int.from_bytes(header_tail[1:3], "little")
                payload = self._read_exact(payload_length, deadline)
                raw = first + header_tail + payload
                return frame_type, payload, raw
        raise TimeoutError("Timed out waiting for WGS reply SOF")

    def _read_exact(self, size, deadline):
        data = bytearray()
        while len(data) < size and time.monotonic() < deadline:
            chunk = self._serial.read(size - len(data))
            if chunk:
                data.extend(chunk)
        if len(data) != size:
            raise TimeoutError(f"Timed out reading {size} bytes, got {len(data)}")
        return bytes(data)

    def _require_open(self):
        if not self.is_open:
            raise RuntimeError("Serial port is not open")
