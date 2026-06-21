from ..protocol.codec import ping_frame, point_set_frame, sequence_set_frame, timer_set_frame
from ..protocol.frame import bytes_to_hex
from ..protocol.reply import parse_reply
from ..transport.serial_transport import SerialTransport


class WgsClient:
    def __init__(self, port, baudrate=115200, timeout=1.0, read_timeout=0.05, write_timeout=3.0):
        self.transport = SerialTransport(
            port=port,
            baudrate=baudrate,
            read_timeout=read_timeout,
            write_timeout=write_timeout,
        )
        self.reply_timeout = float(timeout)
        self.last_tx = b""
        self.last_rx = b""

    @property
    def is_open(self):
        return self.transport.is_open

    def open(self):
        self.transport.open()

    def close(self):
        self.transport.close()

    def ping(self):
        return self.send_command(ping_frame())

    def point_set(self, channel, voltage):
        return self.send_command(point_set_frame(channel, voltage))

    def point_set_no_reply(self, channel, voltage):
        self.send_command_no_reply(point_set_frame(channel, voltage))

    def sequence_set(self, channel, samples):
        return self.send_command(sequence_set_frame(channel, samples))

    def timer_set(self, channel, sample_rate_hz):
        return self.send_command(timer_set_frame(channel, sample_rate_hz))

    def send_command(self, frame):
        self.last_tx = bytes(frame)
        self.transport.write(self.last_tx)
        frame_type, payload, raw = self.transport.read_frame(self.reply_timeout)
        self.last_rx = raw
        return parse_reply(frame_type, payload, raw)

    def send_command_no_reply(self, frame):
        self.last_tx = bytes(frame)
        self.last_rx = b""
        self.transport.write(self.last_tx)

    def flush_input(self):
        self.transport.flush_input()

    def last_tx_hex(self):
        return bytes_to_hex(self.last_tx)

    def last_rx_hex(self):
        return bytes_to_hex(self.last_rx)
