from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def assert_contains(text, needle, label):
    if needle not in text:
        raise AssertionError(f"{label}: expected to find {needle!r}")


def assert_not_contains(text, needle, label):
    if needle in text:
        raise AssertionError(f"{label}: did not expect to find {needle!r}")


def text_between(text, start, end):
    start_index = text.index(start)
    end_index = text.index(end, start_index)
    return text[start_index:end_index]


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


def test_point_ack_uses_hardware_one_pulse_mode():
    proxy = read("user/bsp/TIM/TimProxy.hpp")
    pulse_once = text_between(proxy, "static HAL_StatusTypeDef pulseOnce", "  static void resetCounter")
    assert_contains(proxy, "TIM_CR1_OPM", "POINT_SET ACK should use timer one-pulse mode")
    assert_contains(proxy, "startSinglePulseDirect", "POINT_SET ACK should use low-overhead direct timer start")
    assert_contains(proxy, "waitForOnePulseComplete", "POINT_SET ACK should wait for hardware one-pulse completion")
    assert_not_contains(pulse_once, "HAL_TIM_Base_Start(&timer())", "POINT_SET ACK should not include HAL start overhead in pulse width")
    assert_not_contains(proxy, "while (__HAL_TIM_GET_COUNTER(&timer()) < pulseTicks)", "POINT_SET ACK should not rely on software stopping at compare")


def test_dac_player_uses_ack_pwm_for_point_and_sequence_modes():
    player = read("user/app/WGS/output/DacWavePlayer.hpp")
    assert_contains(player, "prepareSinglePulse(_ackPulseUs)", "POINT_SET should prepare host-width point pulse")
    assert_contains(player, "firePendingPointPulseFromIsr", "external pulse should fire prepared point pulse")
    assert_contains(player, "startContinuous", "SEQUENCE_SET should run ACK PWM continuously")
    assert_contains(player, "stopContinuous", "channel stop should stop ACK PWM")


def test_reset_enters_idle_low_without_ack_continuous_output():
    player = read("user/app/WGS/output/DacWavePlayer.hpp")

    assert_contains(player, "enterIdleLow()", "reset should use an explicit idle-low DAC state")
    assert_contains(player, "setDacValue(DAC_CHANNEL_1, 0U)", "reset should drive CH1 to minimum DAC code")
    assert_contains(player, "setDacValue(DAC_CHANNEL_2, 0U)", "reset should drive CH2 to minimum DAC code")
    assert_contains(player, "startDac(DAC_CHANNEL_1)", "reset should enable CH1 static DAC output")
    assert_contains(player, "startDac(DAC_CHANNEL_2)", "reset should enable CH2 static DAC output")
    assert_contains(player, "sequencePlayingCh1_", "player should track CH1 sequence-active state")
    assert_contains(player, "sequencePlayingCh2_", "player should track CH2 sequence-active state")
    assert_contains(player, "canSync()", "SYNC should be gated by active sequence state")
    assert_contains(player, "if (!canSync())", "idle SYNC should not start DMA/timer ACK output")


def test_point_set_protocol_uses_voltage_only_and_exposes_point_modes():
    command = read("user/app/WGS/protocol/Command.hpp")
    wgs_cc = read("user/app/WGS/WGS.cc")
    output_mode = read("user/app/WGS/output/OutputMode.hpp")

    assert_contains(command, "POINT_SET_PAYLOAD_SIZE = 6", "POINT_SET payload should only include voltage")
    assert_contains(command, "POINT2_SET = 0x11", "POINT2_SET command id")
    assert_contains(command, "POINT_SET_NO_ACK = 0x12", "POINT_SET_NO_ACK command id")
    assert_contains(command, "POINT2_SET_NO_ACK = 0x13", "POINT2_SET_NO_ACK command id")
    assert_contains(command, "POINT2_SET_PAYLOAD_SIZE = 9", "POINT2_SET payload size")
    assert_contains(command, "POINT_PULSE_WIDTH_SET = 0x50", "point pulse width command id")
    assert_contains(command, "POINT_PULSE_WIDTH_DEFAULT_US = 10.0F", "default point pulse width")
    assert_contains(wgs_cc, "POINT2_SET_PAYLOAD_SIZE", "POINT2_SET decode path")
    assert_contains(command, "replyEnabled{true}", "point commands should default to sending replies")
    assert_contains(wgs_cc, "commandType != PROTOCOL::CommandType_e::POINT_SET_NO_ACK", "POINT_SET_NO_ACK should suppress replies")
    assert_contains(wgs_cc, "commandType != PROTOCOL::CommandType_e::POINT2_SET_NO_ACK", "POINT2_SET_NO_ACK should suppress replies")
    assert_contains(wgs_cc, "shouldSendResult(command)", "WGS should skip sendResult for NO_ACK point commands")
    assert_contains(wgs_cc, "POINT_PULSE_WIDTH_SET_PAYLOAD_SIZE", "point pulse width decode path")
    assert_contains(output_mode, "Point2SetCommand_s", "POINT2_SET dispatch")
    assert_contains(output_mode, "PointPulseWidthSetCommand_s", "point pulse width dispatch")


def test_waveform_config_protocol_and_dynamic_length():
    command = read("user/app/WGS/protocol/Command.hpp")
    wgs_cc = read("user/app/WGS/WGS.cc")
    store_hpp = read("user/app/WGS/output/WaveformStore.hpp")
    store_cpp = read("user/app/WGS/output/WaveformStore.cpp")
    player = read("user/app/WGS/output/DacWavePlayer.hpp")
    output_mode = read("user/app/WGS/output/OutputMode.hpp")

    assert_contains(command, "WAVEFORM_CONFIG = 0x30", "waveform config command id")
    assert_contains(command, "WAVEFORM_CONFIG_PAYLOAD_SIZE = 8", "waveform config payload size")
    assert_contains(command, "WaveformConfigCommand_s", "waveform config command type")
    assert_contains(command, "frequencyHz", "waveform frequency field")
    assert_contains(wgs_cc, "readFloat32Le(_frame.payload, 4)", "waveform frequency decode")
    assert_contains(output_mode, "configureWaveform", "waveform config dispatch")
    assert_contains(store_hpp, "activeLength", "per-channel active waveform length")
    assert_contains(store_cpp, "std::min(_samples.size(), activeLength(_channel))", "sequence truncates to active length")
    assert_contains(player, "channelSamples(_channel)", "DMA uses active channel span")


def run_all():
    tests = [
        test_wgs_uses_tim4_tim5_for_dac_output,
        test_timer_proxy_knows_ack_pwm_channels_and_width,
        test_point_ack_uses_hardware_one_pulse_mode,
        test_dac_player_uses_ack_pwm_for_point_and_sequence_modes,
        test_reset_enters_idle_low_without_ack_continuous_output,
        test_point_set_protocol_uses_voltage_only_and_exposes_point_modes,
        test_waveform_config_protocol_and_dynamic_length,
    ]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")


if __name__ == "__main__":
    run_all()
