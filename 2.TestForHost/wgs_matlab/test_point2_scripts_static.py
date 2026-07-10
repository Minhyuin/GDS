from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent
POINT2_SCRIPTS = [
    PROJECT_ROOT / "wgs_uart_demo.m",
    PROJECT_ROOT / "wgs_point_sine_demo.m",
    PROJECT_ROOT / "wgs_point2_snake.m",
    PROJECT_ROOT / "wgs_point2_batch_snake.m",
    PROJECT_ROOT / "wgs_point2_fifo_snake.m",
]


def test_point_scripts_use_point2_set_only():
    for script_path in POINT2_SCRIPTS:
        text = script_path.read_text(encoding="utf-8")
        if "pointSet" in text or "POINT_SET" in text:
            raise AssertionError(f"{script_path.name} still uses POINT_SET")
        if "point2Set" not in text and "POINT2_SET" not in text and "POINT2_FIFO" not in text:
            raise AssertionError(f"{script_path.name} does not use POINT2 output")


def test_snake_sets_ack_pulse_width_after_ping():
    text = (PROJECT_ROOT / "wgs_point2_snake.m").read_text(encoding="utf-8")
    required = "client.pointPulseWidthSet(ackPulseUs)"
    if required not in text:
        raise AssertionError("wgs_point2_snake.m does not apply ackPulseUs")

    ping_index = text.index('wgs.printReply("PING", reply);')
    pulse_index = text.index(required)
    first_point_index = text.index("client.point2Set(")
    if not ping_index < pulse_index < first_point_index:
        raise AssertionError("ackPulseUs should be set after PING and before point output")


def test_snake_uses_point2_no_reply_for_scan_loop():
    text = (PROJECT_ROOT / "wgs_point2_snake.m").read_text(encoding="utf-8")

    required = "client.point2SetNoReply(xPositions(idx), yPosition);"
    if required not in text:
        raise AssertionError("wgs_point2_snake.m should keep the non-batched per-point sender")

    if "point2SetNoReplyBatch(" in text:
        raise AssertionError("wgs_point2_snake.m should not use the batched sender")

    if "reply = client.point2Set(xPositions(idx), yPosition);" in text:
        raise AssertionError("wgs_point2_snake.m should not wait for each scan POINT2_SET reply")

    if 'wgs.printReply("INIT XY", reply);' not in text:
        raise AssertionError("wgs_point2_snake.m should still print setup replies outside the scan loop")


def test_batch_snake_uses_point2_no_reply_batch_for_scan_loop():
    text = (PROJECT_ROOT / "wgs_point2_batch_snake.m").read_text(encoding="utf-8")

    required = "client.point2SetNoReplyBatch("
    if required not in text:
        raise AssertionError("wgs_point2_batch_snake.m should use the batched POINT2_SET_NO_ACK sender")

    required_options = [
        '"PointRateHz", pointRateHz',
        '"BatchPointCount", batchPointCount',
        '"ProgressFcn", progressFcn',
    ]
    for needle in required_options:
        if needle not in text:
            raise AssertionError(f"wgs_point2_batch_snake.m should pass batch sender option {needle}")

    if 'wgs.printReply("INIT XY", reply);' not in text:
        raise AssertionError("wgs_point2_batch_snake.m should still print setup replies outside the scan loop")

    if "client.point2SetNoReply(xPositions(idx), yPosition);" in text:
        raise AssertionError("wgs_point2_batch_snake.m should not write one point per MATLAB loop iteration")

    if "reply = client.point2Set(xPositions(idx), yPosition);" in text:
        raise AssertionError("wgs_point2_batch_snake.m should not wait for each scan POINT2_SET reply")


def test_matlab_client_no_reply_methods_use_no_ack_payloads():
    text = (PROJECT_ROOT / "+wgs" / "Client.m").read_text(encoding="utf-8")
    constants = (PROJECT_ROOT / "+wgs" / "+internal" / "constants.m").read_text(encoding="utf-8")

    required = [
        "CMD_POINT_SET_NO_ACK",
        "CMD_POINT2_SET_NO_ACK",
        "buildPointSetNoAckPayload",
        "buildPoint2SetNoAckPayload",
        "function pointSetNoAck",
        "function point2SetNoAck",
        "function stats = point2SetNoReplyBatch",
        "buildPoint2NoAckFrames",
        "writeFrameBatches",
        "function reply = pointFifoClear",
        "function reply = pointFifoAppend",
        "function reply = pointFifoStart",
        "function reply = pointFifoStop",
        "buildPointFifoAppendPayload",
        "function reply = point2FifoClear",
        "function reply = point2FifoAppend",
        "function reply = point2FifoStart",
        "function reply = point2FifoStop",
        "function stats = point2FifoStream",
        "buildPoint2FifoAppendPayload",
    ]
    for needle in required:
        if needle not in text and needle not in constants:
            raise AssertionError(f"MATLAB NO_ACK client support missing {needle}")

    if "pointSetNoReply(obj, channel, voltage)\n            obj.sendCommandNoReply(obj.buildPointSetPayload" in text:
        raise AssertionError("pointSetNoReply() should not send the ACK POINT_SET payload")
    if "point2SetNoReply(obj, ch1Voltage, ch2Voltage)\n            obj.sendCommandNoReply(obj.buildPoint2SetPayload" in text:
        raise AssertionError("point2SetNoReply() should not send the ACK POINT2_SET payload")


def test_fifo_snake_uses_point2_fifo_stream():
    script_path = PROJECT_ROOT / "wgs_point2_fifo_snake.m"
    if not script_path.exists():
        raise AssertionError("wgs_point2_fifo_snake.m should exist")

    text = script_path.read_text(encoding="utf-8")
    required = [
        "client.point2FifoStream(",
        '"AppendPointCount", appendPointCount',
        '"PreloadPointCount", preloadPointCount',
        '"StopAfterSend", false',
        '"ProgressFcn", progressFcn',
        "client.point2FifoStop()",
    ]
    for needle in required:
        if needle not in text:
            raise AssertionError(f"wgs_point2_fifo_snake.m missing {needle}")

    forbidden = [
        "point2SetNoReplyBatch(",
        "point2SetNoReply(xPositions(idx)",
        "reply = client.point2Set(xPositions(idx)",
    ]
    for needle in forbidden:
        if needle in text:
            raise AssertionError(f"wgs_point2_fifo_snake.m should not use legacy point stream {needle}")


def run_all():
    test_point_scripts_use_point2_set_only()
    test_snake_sets_ack_pulse_width_after_ping()
    test_snake_uses_point2_no_reply_for_scan_loop()
    test_batch_snake_uses_point2_no_reply_batch_for_scan_loop()
    test_matlab_client_no_reply_methods_use_no_ack_payloads()
    test_fifo_snake_uses_point2_fifo_stream()
    print("PASS test_point_scripts_use_point2_set_only")
    print("PASS test_snake_sets_ack_pulse_width_after_ping")
    print("PASS test_snake_uses_point2_no_reply_for_scan_loop")
    print("PASS test_batch_snake_uses_point2_no_reply_batch_for_scan_loop")
    print("PASS test_matlab_client_no_reply_methods_use_no_ack_payloads")
    print("PASS test_fifo_snake_uses_point2_fifo_stream")


if __name__ == "__main__":
    run_all()
