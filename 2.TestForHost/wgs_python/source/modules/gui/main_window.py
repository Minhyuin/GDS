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
from ..protocol.constants import (
    MAX_POINT_ACK_PULSE_US,
    MAX_SAMPLE_RATE_HZ,
    MAX_WAVEFORM_LENGTH_SA,
    MIN_POINT_ACK_PULSE_US,
    PERIOD_SAMPLE_COUNT,
)
from ..transport import list_serial_ports
from ..waveform import generate_waveform


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("WGS Host Debugger")
        self.resize(1180, 760)
        self.client = None
        self._last_valid_waveform_length = PERIOD_SAMPLE_COUNT
        self._last_valid_waveform_frequency = 10.0
        self._updating_waveform_config = False
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
        self.baud_combo.addItems(["921600", "460800", "230400", "115200"])
        self.refresh_button = QPushButton("Refresh")
        self.connect_button = QPushButton("Connect")
        self.ping_button = QPushButton("Ping")
        self.sync_button = QPushButton("Sync")
        self.ping_button.setEnabled(False)
        self.sync_button.setEnabled(False)

        layout.addWidget(QLabel("Port"), 0, 0)
        layout.addWidget(self.port_combo, 0, 1)
        layout.addWidget(self.refresh_button, 0, 2)
        layout.addWidget(QLabel("Baud"), 1, 0)
        layout.addWidget(self.baud_combo, 1, 1)
        layout.addWidget(self.connect_button, 1, 2)
        layout.addWidget(self.ping_button, 2, 1)
        layout.addWidget(self.sync_button, 2, 2)

        self.refresh_button.clicked.connect(self.refresh_ports)
        self.connect_button.clicked.connect(self.toggle_connection)
        self.ping_button.clicked.connect(self.ping)
        self.sync_button.clicked.connect(self.sync)
        return box

    def _build_point_box(self):
        box = QGroupBox("Point Set")
        layout = QFormLayout(box)
        self.point_channel = self._channel_combo()
        self.point_voltage = self._voltage_spin(1.65)
        self.point_ack_pulse = self._ack_pulse_spin()
        self.point_send_button = QPushButton("Send Point")
        self.point_send_button.setEnabled(False)
        layout.addRow("Channel", self.point_channel)
        layout.addRow("Voltage", self.point_voltage)
        layout.addRow("ACK Width", self.point_ack_pulse)
        layout.addRow(self.point_send_button)
        self.point_send_button.clicked.connect(self.send_point)
        return box

    def _build_timer_box(self):
        box = QGroupBox("Waveform Config")
        layout = QFormLayout(box)
        self.waveform_length = QSpinBox()
        self.waveform_length.setRange(1, MAX_WAVEFORM_LENGTH_SA)
        self.waveform_length.setValue(PERIOD_SAMPLE_COUNT)
        self.waveform_length.setSuffix(" Sa")
        self.waveform_frequency = QDoubleSpinBox()
        self.waveform_frequency.setRange(0.001, MAX_SAMPLE_RATE_HZ)
        self.waveform_frequency.setDecimals(3)
        self.waveform_frequency.setSingleStep(1.0)
        self.waveform_frequency.setValue(10.0)
        self.waveform_frequency.setSuffix(" Hz")
        self.computed_sample_rate = QDoubleSpinBox()
        self.computed_sample_rate.setRange(0.0, MAX_SAMPLE_RATE_HZ)
        self.computed_sample_rate.setDecimals(1)
        self.computed_sample_rate.setSuffix(" Sa/s")
        self.computed_sample_rate.setReadOnly(True)
        self.computed_sample_rate.setButtonSymbols(QDoubleSpinBox.NoButtons)
        layout.addRow("Length", self.waveform_length)
        layout.addRow("Frequency", self.waveform_frequency)
        layout.addRow("Sample Rate", self.computed_sample_rate)
        self.waveform_length.valueChanged.connect(self._on_waveform_config_changed)
        self.waveform_frequency.valueChanged.connect(self._on_waveform_config_changed)
        self._update_sample_rate_display()
        return box

    def _build_sequence_box(self):
        box = QGroupBox("Sequence Set")
        layout = QFormLayout(box)
        self.sequence_channel = self._channel_combo()
        self.waveform_kind = QComboBox()
        self.waveform_kind.addItems(["sine", "cosine", "square", "triangle", "constant"])
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
        layout.addRow("Center V", self.center)
        layout.addRow("Amplitude V", self.amplitude)
        layout.addRow("Phase deg", self.phase)
        button_row = QHBoxLayout()
        button_row.addWidget(self.preview_button)
        button_row.addWidget(self.sequence_send_button)
        layout.addRow(button_row)

        self.preview_button.clicked.connect(self.update_preview)
        self.sequence_send_button.clicked.connect(self.send_sequence)
        for widget in [self.waveform_kind, self.center, self.amplitude, self.phase]:
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

    def _ack_pulse_spin(self):
        spin = QDoubleSpinBox()
        spin.setRange(MIN_POINT_ACK_PULSE_US, MAX_POINT_ACK_PULSE_US)
        spin.setDecimals(1)
        spin.setSingleStep(1.0)
        spin.setValue(5.0)
        spin.setSuffix(" us")
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
            self.sync_button,
            self.point_send_button,
            self.sequence_send_button,
        ]:
            button.setEnabled(connected)

    def ping(self):
        self._run_command("PING", lambda: self.client.ping())

    def sync(self):
        self._run_command("SYNC", lambda: self.client.sync())

    def send_point(self):
        channel = self.point_channel.currentData()
        voltage = self.point_voltage.value()
        ack_pulse_us = self.point_ack_pulse.value()
        self._run_command(
            f"POINT_SET {channel} {voltage:.4f}V ACK={ack_pulse_us:.1f}us",
            lambda: self.client.point_set(channel, voltage, ack_pulse_us),
        )

    def send_sequence(self):
        channel = self.sequence_channel.currentData()
        self.update_preview()
        frequency_hz = self.waveform_frequency.value()
        self._run_command(
            f"SEQUENCE_SET {channel} {len(self.preview_values)} points {frequency_hz:.3f}Hz",
            lambda: self.client.sequence_set(channel, self.preview_values, frequency_hz),
        )

    def update_preview(self):
        self.preview_values = generate_waveform(
            self.waveform_kind.currentText(),
            self.waveform_length.value(),
            self.center.value(),
            self.amplitude.value(),
            self.phase.value(),
        )
        self.curve.setData(self.preview_values)
        self.plot.setYRange(0.0, 3.3, padding=0.04)

    def _on_waveform_config_changed(self):
        if self._updating_waveform_config:
            return

        length = self.waveform_length.value()
        frequency = self.waveform_frequency.value()
        if length * frequency > MAX_SAMPLE_RATE_HZ:
            self._updating_waveform_config = True
            self.waveform_length.setValue(self._last_valid_waveform_length)
            self.waveform_frequency.setValue(self._last_valid_waveform_frequency)
            self._updating_waveform_config = False
            self._update_sample_rate_display()
            self.update_preview()
            self.append_log(f"Waveform config rejected: sample rate exceeds {MAX_SAMPLE_RATE_HZ:g} Sa/s")
            return

        self._last_valid_waveform_length = length
        self._last_valid_waveform_frequency = frequency
        self._update_sample_rate_display()
        self.update_preview()

    def _update_sample_rate_display(self):
        self.computed_sample_rate.setValue(self.waveform_length.value() * self.waveform_frequency.value())

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
