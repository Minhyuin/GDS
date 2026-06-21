import traceback

import pyqtgraph as pg
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QComboBox,
    QDoubleSpinBox,
    QFormLayout,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QPlainTextEdit,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)

from ..client import WgsClient
from ..protocol.constants import PERIOD_SAMPLE_COUNT
from ..transport import list_serial_ports
from ..waveform import generate_waveform


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("WGS Host Debugger")
        self.resize(1180, 760)
        self.client = None
        self.preview_values = generate_waveform("sine")
        self._build_ui()
        self.refresh_ports()
        self.update_preview()

    def _build_ui(self):
        root = QWidget()
        layout = QGridLayout(root)
        layout.setColumnStretch(0, 0)
        layout.setColumnStretch(1, 1)

        left = QVBoxLayout()
        left.addWidget(self._build_connection_box())
        left.addWidget(self._build_point_box())
        left.addWidget(self._build_timer_box())
        left.addWidget(self._build_sequence_box())
        left.addStretch(1)

        right = QVBoxLayout()
        self.plot = pg.PlotWidget()
        self.plot.setBackground("w")
        self.plot.showGrid(x=True, y=True, alpha=0.3)
        self.plot.setLabel("left", "Voltage", units="V")
        self.plot.setLabel("bottom", "Sample")
        self.curve = self.plot.plot(pen=pg.mkPen("#2563EB", width=2))
        right.addWidget(self.plot, 3)

        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setMaximumBlockCount(1000)
        right.addWidget(self.log, 2)

        layout.addLayout(left, 0, 0)
        layout.addLayout(right, 0, 1)
        self.setCentralWidget(root)

    def _build_connection_box(self):
        box = QGroupBox("Connection")
        layout = QGridLayout(box)
        self.port_combo = QComboBox()
        self.baud_combo = QComboBox()
        self.baud_combo.addItems(["115200", "230400", "460800", "921600"])
        self.refresh_button = QPushButton("Refresh")
        self.connect_button = QPushButton("Connect")
        self.ping_button = QPushButton("Ping")
        self.ping_button.setEnabled(False)

        layout.addWidget(QLabel("Port"), 0, 0)
        layout.addWidget(self.port_combo, 0, 1)
        layout.addWidget(self.refresh_button, 0, 2)
        layout.addWidget(QLabel("Baud"), 1, 0)
        layout.addWidget(self.baud_combo, 1, 1)
        layout.addWidget(self.connect_button, 1, 2)
        layout.addWidget(self.ping_button, 2, 1, 1, 2)

        self.refresh_button.clicked.connect(self.refresh_ports)
        self.connect_button.clicked.connect(self.toggle_connection)
        self.ping_button.clicked.connect(self.ping)
        return box

    def _build_point_box(self):
        box = QGroupBox("Point Set")
        layout = QFormLayout(box)
        self.point_channel = self._channel_combo()
        self.point_voltage = self._voltage_spin(1.65)
        self.point_send_button = QPushButton("Send Point")
        self.point_send_button.setEnabled(False)
        layout.addRow("Channel", self.point_channel)
        layout.addRow("Voltage", self.point_voltage)
        layout.addRow(self.point_send_button)
        self.point_send_button.clicked.connect(self.send_point)
        return box

    def _build_timer_box(self):
        box = QGroupBox("Timer")
        layout = QFormLayout(box)
        self.timer_channel = self._channel_combo()
        self.sample_rate = QDoubleSpinBox()
        self.sample_rate.setRange(1.0, 1_000_000.0)
        self.sample_rate.setDecimals(1)
        self.sample_rate.setSingleStep(1000.0)
        self.sample_rate.setValue(10000.0)
        self.wave_freq = QDoubleSpinBox()
        self.wave_freq.setRange(0.001, 1000.0)
        self.wave_freq.setDecimals(3)
        self.wave_freq.setSingleStep(1.0)
        self.wave_freq.setValue(10.0)
        self.apply_sample_rate_button = QPushButton("Set Sample Rate")
        self.apply_wave_freq_button = QPushButton("Set Wave Frequency")
        self.apply_sample_rate_button.setEnabled(False)
        self.apply_wave_freq_button.setEnabled(False)
        layout.addRow("Channel", self.timer_channel)
        layout.addRow("Sample Rate Hz", self.sample_rate)
        layout.addRow("Wave Frequency Hz", self.wave_freq)
        layout.addRow(self.apply_sample_rate_button)
        layout.addRow(self.apply_wave_freq_button)
        self.apply_sample_rate_button.clicked.connect(self.set_sample_rate)
        self.apply_wave_freq_button.clicked.connect(self.set_wave_frequency)
        return box

    def _build_sequence_box(self):
        box = QGroupBox("Sequence Set")
        layout = QFormLayout(box)
        self.sequence_channel = self._channel_combo()
        self.waveform_kind = QComboBox()
        self.waveform_kind.addItems(["sine", "cosine", "square", "triangle", "constant"])
        self.points = QSpinBox()
        self.points.setRange(1, PERIOD_SAMPLE_COUNT)
        self.points.setValue(PERIOD_SAMPLE_COUNT)
        self.center = self._voltage_spin(1.65)
        self.amplitude = self._voltage_spin(1.2)
        self.phase = QDoubleSpinBox()
        self.phase.setRange(-360.0, 360.0)
        self.phase.setDecimals(1)
        self.phase.setValue(0.0)
        self.preview_button = QPushButton("Preview")
        self.sequence_send_button = QPushButton("Send Sequence")
        self.sequence_send_button.setEnabled(False)

        layout.addRow("Channel", self.sequence_channel)
        layout.addRow("Waveform", self.waveform_kind)
        layout.addRow("Points", self.points)
        layout.addRow("Center V", self.center)
        layout.addRow("Amplitude V", self.amplitude)
        layout.addRow("Phase deg", self.phase)
        button_row = QHBoxLayout()
        button_row.addWidget(self.preview_button)
        button_row.addWidget(self.sequence_send_button)
        layout.addRow(button_row)

        self.preview_button.clicked.connect(self.update_preview)
        self.sequence_send_button.clicked.connect(self.send_sequence)
        for widget in [self.waveform_kind, self.points, self.center, self.amplitude, self.phase]:
            signal = getattr(widget, "valueChanged", None) or getattr(widget, "currentTextChanged")
            signal.connect(self.update_preview)
        return box

    def _channel_combo(self):
        combo = QComboBox()
        combo.addItem("CH1", "CH1")
        combo.addItem("CH2", "CH2")
        return combo

    def _voltage_spin(self, value):
        spin = QDoubleSpinBox()
        spin.setRange(0.0, 3.3)
        spin.setDecimals(4)
        spin.setSingleStep(0.05)
        spin.setValue(value)
        spin.setSuffix(" V")
        return spin

    def refresh_ports(self):
        current = self.port_combo.currentText()
        self.port_combo.clear()
        ports = list_serial_ports()
        self.port_combo.addItems(ports)
        if current:
            index = self.port_combo.findText(current)
            if index >= 0:
                self.port_combo.setCurrentIndex(index)

    def toggle_connection(self):
        if self.client and self.client.is_open:
            self.client.close()
            self.client = None
            self._set_connected(False)
            self.append_log("Disconnected")
            return

        port = self.port_combo.currentText()
        if not port:
            QMessageBox.warning(self, "No Port", "No serial port selected.")
            return
        try:
            self.client = WgsClient(port, int(self.baud_combo.currentText()))
            self.client.open()
            self._set_connected(True)
            self.append_log(f"Connected to {port} @ {self.baud_combo.currentText()}")
        except Exception as exc:
            self.client = None
            self._set_connected(False)
            self.show_error(exc)

    def _set_connected(self, connected):
        self.connect_button.setText("Disconnect" if connected else "Connect")
        for button in [
            self.ping_button,
            self.point_send_button,
            self.apply_sample_rate_button,
            self.apply_wave_freq_button,
            self.sequence_send_button,
        ]:
            button.setEnabled(connected)

    def ping(self):
        self._run_command("PING", lambda: self.client.ping())

    def send_point(self):
        channel = self.point_channel.currentData()
        voltage = self.point_voltage.value()
        self._run_command(f"POINT_SET {channel} {voltage:.4f}V", lambda: self.client.point_set(channel, voltage))

    def set_sample_rate(self):
        channel = self.timer_channel.currentData()
        rate = self.sample_rate.value()
        self._run_command(f"TIMER_SET {channel} {rate:.1f}Hz", lambda: self.client.timer_set(channel, rate))

    def set_wave_frequency(self):
        channel = self.timer_channel.currentData()
        rate = self.wave_freq.value() * PERIOD_SAMPLE_COUNT
        self.sample_rate.setValue(rate)
        self._run_command(
            f"TIMER_SET {channel} wave={self.wave_freq.value():.3f}Hz sample={rate:.1f}Hz",
            lambda: self.client.timer_set(channel, rate),
        )

    def send_sequence(self):
        channel = self.sequence_channel.currentData()
        self.update_preview()
        self._run_command(
            f"SEQUENCE_SET {channel} {len(self.preview_values)} points",
            lambda: self.client.sequence_set(channel, self.preview_values),
        )

    def update_preview(self):
        self.preview_values = generate_waveform(
            self.waveform_kind.currentText(),
            self.points.value(),
            self.center.value(),
            self.amplitude.value(),
            self.phase.value(),
        )
        self.curve.setData(self.preview_values)
        self.plot.setYRange(0.0, 3.3, padding=0.04)

    def _run_command(self, label, fn):
        if not self.client or not self.client.is_open:
            QMessageBox.warning(self, "Disconnected", "Serial port is not connected.")
            return
        try:
            reply = fn()
            self.append_log(f"> {label}")
            self.append_log(f"TX: {self.client.last_tx_hex()}")
            self.append_log(f"RX: {self.client.last_rx_hex()}")
            self.append_log(f"< {reply.summary()}")
            if not reply.ok:
                QMessageBox.warning(self, "WGS Reply", reply.summary())
        except Exception as exc:
            self.show_error(exc)

    def append_log(self, text):
        self.log.appendPlainText(text)
        self.log.verticalScrollBar().setValue(self.log.verticalScrollBar().maximum())

    def show_error(self, exc):
        self.append_log(str(exc))
        self.append_log(traceback.format_exc())
        QMessageBox.critical(self, "Error", str(exc))

    def closeEvent(self, event):
        if self.client:
            self.client.close()
        event.accept()
