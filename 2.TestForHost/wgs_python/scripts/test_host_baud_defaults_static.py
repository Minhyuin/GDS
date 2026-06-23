import re
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = PROJECT_ROOT.parents[1]
sys.path.insert(0, str(PROJECT_ROOT))


DEFAULT_BAUD = "921600"


def read(relative_path):
    return (REPO_ROOT / relative_path).read_text(encoding="utf-8")


def assert_contains(text, needle, label):
    if needle not in text:
        raise AssertionError(f"{label}: expected to find {needle!r}")


def assert_not_contains(text, needle, label):
    if needle in text:
        raise AssertionError(f"{label}: did not expect to find {needle!r}")


def test_python_client_and_transport_default_to_921600():
    client = read("2.TestForHost/wgs_python/source/modules/client/wgs_client.py")
    transport = read("2.TestForHost/wgs_python/source/modules/transport/serial_transport.py")

    assert_contains(client, f"baudrate={DEFAULT_BAUD}", "Python WgsClient default baud")
    assert_contains(transport, f"baudrate={DEFAULT_BAUD}", "Python SerialTransport default baud")


def test_python_gui_selects_921600_by_default():
    gui = read("2.TestForHost/wgs_python/source/modules/gui/main_window.py")

    assert_contains(gui, f'"{DEFAULT_BAUD}"', "Python GUI baud options")
    match = re.search(r"self\.baud_combo\.addItems\(\[(.*?)\]\)", gui, re.S)
    if match is None:
        raise AssertionError("Python GUI baud combo should use addItems")
    first_item = re.search(r'"(\d+)"', match.group(1))
    if first_item is None or first_item.group(1) != DEFAULT_BAUD:
        actual = first_item.group(1) if first_item is not None else "<none>"
        raise AssertionError(f"Python GUI default baud should be {DEFAULT_BAUD}, got {actual}")


def test_matlab_client_default_to_921600():
    client = read("2.TestForHost/wgs_matlab/+wgs/Client.m")

    assert_contains(client, f"BaudRate = {DEFAULT_BAUD}", "MATLAB Client default baud")


def test_user_facing_examples_do_not_keep_old_default_baud():
    paths = [
        "2.TestForHost/wgs_matlab/README.md",
        "2.TestForHost/wgs_matlab/wgs_uart_demo.m",
        "2.TestForHost/wgs_matlab/wgs_point_sine_demo.m",
        "2.TestForHost/wgs_python/README.md",
    ]

    for path in paths:
        text = read(path)
        assert_contains(text, DEFAULT_BAUD, f"{path} should mention current default baud")
        assert_not_contains(text, "115200", f"{path} should not mention old default baud")


def run_all():
    tests = [
        test_python_client_and_transport_default_to_921600,
        test_python_gui_selects_921600_by_default,
        test_matlab_client_default_to_921600,
        test_user_facing_examples_do_not_keep_old_default_baud,
    ]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")


if __name__ == "__main__":
    run_all()
