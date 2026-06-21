import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT))

from source.modules.protocol.codec import sequence_set_frame  # noqa: E402
from source.modules.transport.serial_transport import SerialTransport  # noqa: E402


def test_sequence_write_timeout_covers_wire_time():
    baudrate = 115200
    frame = sequence_set_frame("CH2", [1.65] * 1000)
    transport = SerialTransport("COM_TEST", baudrate=baudrate, read_timeout=0.05, write_timeout=0.2)

    wire_time_seconds = len(frame) * 10.0 / baudrate
    write_timeout = transport.write_timeout_for(frame)

    if write_timeout < wire_time_seconds * 2.0:
        raise AssertionError(
            "write timeout is too short for a 1000-point SEQUENCE_SET frame\n"
            f"frame bytes: {len(frame)}\n"
            f"wire time: {wire_time_seconds:.3f}s\n"
            f"write timeout: {write_timeout:.3f}s"
        )


def run_all():
    tests = [test_sequence_write_timeout_covers_wire_time]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")


if __name__ == "__main__":
    run_all()
