import os
import sys
from pathlib import Path


os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT))

from PySide6.QtWidgets import QApplication  # noqa: E402

from source.modules.gui import main_window  # noqa: E402


class FakeClient:
    def __init__(self):
        self.is_open = True
        self.sync_calls = 0
        self.point_calls = []
        self.point2_calls = []
        self.point_pulse_width_calls = []
        self.sequence_calls = []
        self.last_tx = bytes.fromhex("A5 01 01 00 01")
        self.last_rx = bytes.fromhex("A5 02 02 00 01 00")

    def sync(self):
        self.sync_calls += 1
        return FakeReply()

    def point_set(self, channel, voltage):
        self.point_calls.append((channel, voltage))
        self.last_tx = bytes.fromhex("A5 01 06 00 10 01 00 00 C0 3F")
        return FakeReply()

    def point2_set(self, ch1_voltage, ch2_voltage):
        self.point2_calls.append((ch1_voltage, ch2_voltage))
        self.last_tx = bytes.fromhex("A5 01 09 00 11 00 00 C0 3F 00 00 20 40")
        return FakeReply()

    def point_pulse_width_set(self, pulse_width_us):
        self.point_pulse_width_calls.append(pulse_width_us)
        self.last_tx = bytes.fromhex("A5 01 05 00 50 00 00 48 42")
        return FakeReply()

    def sequence_set(self, channel, samples, frequency_hz):
        self.sequence_calls.append((channel, len(samples), frequency_hz))
        self.last_tx = bytes.fromhex("A5 01 08 00 30 01 03 00 00 00 20 41")
        return FakeReply()

    def close(self):
        pass

    def last_tx_hex(self):
        return self.last_tx.hex(" ").upper()

    def last_rx_hex(self):
        return self.last_rx.hex(" ").upper()


class FakeReply:
    ok = True

    def summary(self):
        return "ACK status=DONE detail=NONE"


def app_instance():
    app = QApplication.instance()
    if app is None:
        app = QApplication([])
    return app


def test_sync_button_is_available_when_connected_and_calls_client_sync():
    app_instance()
    window = main_window.MainWindow()
    window.client = FakeClient()

    window._set_connected(True)
    if not hasattr(window, "sync_button"):
        raise AssertionError("MainWindow should expose a sync_button")
    if window.sync_button.text() != "Sync":
        raise AssertionError(f"Expected Sync button text, got {window.sync_button.text()!r}")
    if not window.sync_button.isEnabled():
        raise AssertionError("Sync button should be enabled when connected")

    window.sync()
    if window.client.sync_calls != 1:
        raise AssertionError(f"Expected one client.sync() call, got {window.client.sync_calls}")

    log_text = window.log.toPlainText()
    if "> SYNC" not in log_text:
        raise AssertionError("SYNC command should be written to the GUI log")

    window.close()


def test_point_set_sends_voltage_only():
    app_instance()
    window = main_window.MainWindow()
    window.client = FakeClient()
    window._set_connected(True)

    window.point_channel.setCurrentIndex(0)
    window.point_voltage.setValue(1.5)
    window.send_point()

    if window.client.point_calls != [("CH1", 1.5)]:
        raise AssertionError(f"Expected POINT_SET voltage-only call, got {window.client.point_calls!r}")

    log_text = window.log.toPlainText()
    if "POINT_SET CH1 1.5000V" not in log_text:
        raise AssertionError("POINT_SET should be written to the GUI log")

    window.close()


def test_point2_and_pulse_width_controls_call_client():
    app_instance()
    window = main_window.MainWindow()
    window.client = FakeClient()
    window._set_connected(True)

    window.point2_ch1_voltage.setValue(1.5)
    window.point2_ch2_voltage.setValue(2.5)
    window.send_point2()

    window.point_pulse_width.setValue(50.0)
    window.send_point_pulse_width()

    if window.client.point2_calls != [(1.5, 2.5)]:
        raise AssertionError(f"Expected POINT2_SET call, got {window.client.point2_calls!r}")
    if window.client.point_pulse_width_calls != [50.0]:
        raise AssertionError(f"Expected pulse width call, got {window.client.point_pulse_width_calls!r}")

    log_text = window.log.toPlainText()
    if "POINT2_SET CH1 1.5000V CH2 2.5000V" not in log_text:
        raise AssertionError("POINT2_SET should be written to the GUI log")
    if "POINT_PULSE_WIDTH_SET 50.0us" not in log_text:
        raise AssertionError("Pulse width set should be written to the GUI log")

    window.close()


def test_waveform_length_frequency_updates_readonly_sample_rate_and_rejects_over_limit():
    app_instance()
    window = main_window.MainWindow()

    if not hasattr(window, "waveform_length"):
        raise AssertionError("MainWindow should expose waveform_length")
    if not hasattr(window, "waveform_frequency"):
        raise AssertionError("MainWindow should expose waveform_frequency")
    if not hasattr(window, "computed_sample_rate"):
        raise AssertionError("MainWindow should expose computed_sample_rate")
    if not window.computed_sample_rate.isReadOnly():
        raise AssertionError("computed_sample_rate should be read-only")

    window.waveform_length.setValue(256)
    window.waveform_frequency.setValue(12.5)
    if window.computed_sample_rate.value() != 3200.0:
        raise AssertionError(f"Expected 3200 Sa/s, got {window.computed_sample_rate.value()}")

    window.waveform_frequency.setValue(500.0)
    if window.waveform_frequency.value() != 12.5:
        raise AssertionError("Over-limit frequency should be rejected and restored")
    if window.computed_sample_rate.value() != 3200.0:
        raise AssertionError("Sample rate should remain at the previous accepted value")

    window.close()


def test_send_sequence_uses_configured_length_and_frequency():
    app_instance()
    window = main_window.MainWindow()
    window.client = FakeClient()
    window._set_connected(True)
    window.waveform_length.setValue(128)
    window.waveform_frequency.setValue(20.0)

    window.send_sequence()
    if window.client.sequence_calls != [("CH1", 128, 20.0)]:
        raise AssertionError(f"Expected configured sequence call, got {window.client.sequence_calls!r}")

    log_text = window.log.toPlainText()
    if "SEQUENCE_SET CH1 128 points 20.000Hz" not in log_text:
        raise AssertionError("Sequence log should include configured length and frequency")

    window.close()


def run_all():
    tests = [
        test_sync_button_is_available_when_connected_and_calls_client_sync,
        test_point_set_sends_voltage_only,
        test_point2_and_pulse_width_controls_call_client,
        test_waveform_length_frequency_updates_readonly_sample_rate_and_rejects_over_limit,
        test_send_sequence_uses_configured_length_and_frequency,
    ]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")


if __name__ == "__main__":
    run_all()
