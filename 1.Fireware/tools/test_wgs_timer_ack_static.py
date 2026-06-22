from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def assert_contains(text, needle, label):
    if needle not in text:
        raise AssertionError(f"{label}: expected to find {needle!r}")


def assert_not_contains(text, needle, label):
    if needle in text:
        raise AssertionError(f"{label}: did not expect to find {needle!r}")


def test_wgs_uses_tim4_tim5_for_dac_output():
    wgs_hpp = read("user/app/WGS/WGS.hpp")
    assert_contains(wgs_hpp, "Tim4HandleProvider", "WGS CH1 timer provider")
    assert_contains(wgs_hpp, "Tim5HandleProvider", "WGS CH2 timer provider")
    assert_not_contains(wgs_hpp, "Tim6HandleProvider", "removed TIM6 provider")
    assert_not_contains(wgs_hpp, "Tim7HandleProvider", "removed TIM7 provider")


def test_timer_proxy_knows_ack_pwm_channels_and_width():
    proxy = read("user/bsp/TIM/TimProxy.hpp")
    assert_not_contains(proxy, "ACK_PULSE_WIDTH_NS", "fixed ACK pulse width")
    assert_contains(proxy, "ackPulseTicksForPeriod", "sequence ACK should derive from timer period")
    assert_contains(proxy, "pulseOnce(float _pulseWidthUs)", "point ACK pulse width API")
    assert_contains(proxy, "TIM_CHANNEL_1", "TIM4 ACK first channel")
    assert_contains(proxy, "TIM_CHANNEL_2", "TIM4 ACK second channel")
    assert_contains(proxy, "TIM_CHANNEL_3", "TIM5 ACK first channel")
    assert_contains(proxy, "TIM_CHANNEL_4", "TIM5 ACK second channel")
    assert_contains(proxy, "pulseOnce", "single point ACK pulse API")
    assert_contains(proxy, "startContinuous", "sequence ACK PWM API")


def test_dac_player_uses_ack_pwm_for_point_and_sequence_modes():
    player = read("user/app/WGS/output/DacWavePlayer.hpp")
    assert_contains(player, "pulseOnce(_ackPulseUs)", "POINT_SET should emit host-width ACK pulse")
    assert_contains(player, "startContinuous", "SEQUENCE_SET should run ACK PWM continuously")
    assert_contains(player, "stopContinuous", "channel stop should stop ACK PWM")


def test_point_set_protocol_carries_ack_pulse_width():
    command = read("user/app/WGS/protocol/Command.hpp")
    wgs_cc = read("user/app/WGS/WGS.cc")
    output_mode = read("user/app/WGS/output/OutputMode.hpp")

    assert_contains(command, "POINT_SET_PAYLOAD_SIZE = 10", "POINT_SET payload includes ack pulse width")
    assert_contains(command, "ackPulseUs", "POINT_SET command stores ACK pulse width")
    assert_contains(wgs_cc, "MIN_POINT_ACK_PULSE_US", "POINT_SET ACK pulse minimum")
    assert_contains(wgs_cc, "MAX_POINT_ACK_PULSE_US", "POINT_SET ACK pulse maximum")
    assert_contains(wgs_cc, "readFloat32Le(_frame.payload, 6)", "POINT_SET ACK pulse decode")
    assert_contains(output_mode, "_cmd.ackPulseUs", "POINT_SET ACK pulse dispatch")


def run_all():
    tests = [
        test_wgs_uses_tim4_tim5_for_dac_output,
        test_timer_proxy_knows_ack_pwm_channels_and_width,
        test_dac_player_uses_ack_pwm_for_point_and_sequence_modes,
        test_point_set_protocol_carries_ack_pulse_width,
    ]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")


if __name__ == "__main__":
    run_all()
