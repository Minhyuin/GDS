from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MATLAB_ROOT = ROOT / "wgs_matlab"


def read(path):
    return (MATLAB_ROOT / path).read_text(encoding="utf-8")


def assert_contains(text, needle, label):
    if needle not in text:
        raise AssertionError(f"{label}: expected to find {needle!r}")


def assert_not_contains(text, needle, label):
    if needle in text:
        raise AssertionError(f"{label}: did not expect to find {needle!r}")


def test_matlab_sequence_set_requires_done_ack_before_sequence_payload():
    client = read("+wgs/Client.m")
    constants = read("+wgs/+internal/constants.m")

    assert_contains(constants, "ACK_DONE", "MATLAB constants should expose ACK DONE")
    assert_contains(constants, "ERROR_NONE", "MATLAB constants should expose NONE detail")
    assert_contains(client, "isDoneReply", "MATLAB client should distinguish ACK/DONE from ACK/BUSY")
    assert_contains(client, "reply.status == C.ACK_DONE", "sequenceSet should require DONE status")
    assert_contains(client, "reply.detail == C.ERROR_NONE", "sequenceSet should require NONE detail")
    assert_not_contains(client, "if ~reply.isAck\n                return;", "sequenceSet should not treat ACK/BUSY as success")


def run_all():
    tests = [test_matlab_sequence_set_requires_done_ack_before_sequence_payload]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")


if __name__ == "__main__":
    run_all()
