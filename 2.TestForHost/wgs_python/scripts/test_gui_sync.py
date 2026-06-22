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
        self.last_tx = bytes.fromhex("A5 01 01 00 01")
        self.last_rx = bytes.fromhex("A5 02 02 00 01 00")

    def sync(self):
        self.sync_calls += 1
        return FakeReply()

    def point_set(self, channel, voltage, ack_pulse_us=5.0):
        self.point_calls.append((channel, voltage, ack_pulse_us))
        self.last_tx = bytes.fromhex("A5 01 0A 00 10 01 00 00 C0 3F 00 00 48 42")
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


def test_point_ack_width_is_sent_to_client():
    app_instance()
    window = main_window.MainWindow()
    window.client = FakeClient()
    window._set_connected(True)

    if not hasattr(window, "point_ack_pulse"):
        raise AssertionError("MainWindow should expose a point_ack_pulse control")

    window.point_channel.setCurrentIndex(0)
    window.point_voltage.setValue(1.5)
    window.point_ack_pulse.setValue(50.0)
    window.send_point()

    if window.client.point_calls != [("CH1", 1.5, 50.0)]:
        raise AssertionError(f"Expected POINT_SET ACK width call, got {window.client.point_calls!r}")

    log_text = window.log.toPlainText()
    if "POINT_SET CH1 1.5000V ACK=50.0us" not in log_text:
        raise AssertionError("POINT_SET ACK width should be written to the GUI log")

    window.close()


def run_all():
    tests = [
        test_sync_button_is_available_when_connected_and_calls_client_sync,
        test_point_ack_width_is_sent_to_client,
    ]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")


if __name__ == "__main__":
    run_all()
