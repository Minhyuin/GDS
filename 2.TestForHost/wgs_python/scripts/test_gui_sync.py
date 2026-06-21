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
        self.last_tx = bytes.fromhex("A5 01 01 00 01")
        self.last_rx = bytes.fromhex("A5 02 02 00 01 00")

    def sync(self):
        self.sync_calls += 1
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


def run_all():
    tests = [test_sync_button_is_available_when_connected_and_calls_client_sync]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")


if __name__ == "__main__":
    run_all()
