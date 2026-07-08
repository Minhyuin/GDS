from ..protocol.codec import (
    ping_frame,
    point_set_frame,
    point2_set_frame,
    point_pulse_width_set_frame,
    sequence_set_frame,
    sync_frame,
    timer_set_frame,
    waveform_config_frame,
)
from ..protocol.frame import bytes_to_hex
from ..protocol.reply import parse_reply
from ..transport.serial_transport import SerialTransport


class WgsClient:
    def __init__(self, port, baudrate=921600, timeout=1.0, read_timeout=0.05, write_timeout=3.0):
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

    def sync(self):
        return self.send_command(sync_frame())

    def point_set(self, channel, voltage):
        return self.send_command(point_set_frame(channel, voltage))

    def point_set_no_reply(self, channel, voltage):
        self.send_command_no_reply(point_set_frame(channel, voltage))

    def point2_set(self, ch1_voltage, ch2_voltage):
        return self.send_command(point2_set_frame(ch1_voltage, ch2_voltage))

    def point2_set_no_reply(self, ch1_voltage, ch2_voltage):
        self.send_command_no_reply(point2_set_frame(ch1_voltage, ch2_voltage))

    def point_pulse_width_set(self, pulse_width_us):
        return self.send_command(point_pulse_width_set_frame(pulse_width_us))

    def waveform_config(self, channel, length_sa, frequency_hz):
        return self.send_command(waveform_config_frame(channel, length_sa, frequency_hz))

    def sequence_set(self, channel, samples, frequency_hz):
        values = list(samples)
        config_reply = self.waveform_config(channel, len(values), frequency_hz)
        if not config_reply.ok:
            return config_reply
        return self.send_command(sequence_set_frame(channel, values))

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
