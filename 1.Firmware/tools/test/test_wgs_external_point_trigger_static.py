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


def assert_order(text, first, second, label):
    first_index = text.find(first)
    second_index = text.find(second)
    if first_index < 0 or second_index < 0 or first_index >= second_index:
        raise AssertionError(f"{label}: expected {first!r} before {second!r}")


def text_between(text, start, end):
    start_index = text.index(start)
    end_index = text.index(end, start_index)
    return text[start_index:end_index]


def test_cubemx_external_pulse_and_timer_irqs_are_enabled():
    ioc = read("GDS.ioc")
    main_h = read("Core/Inc/main.h")
    gpio_c = read("Core/Src/gpio.c")
    it_c = read("Core/Src/stm32f1xx_it.c")

    assert_contains(ioc, "PB0.GPIO_Label=EXT_PULSE_IN", "external pulse label")
    assert_contains(ioc, "PB0.Signal=GPXTI0", "external pulse EXTI line")
    assert_contains(ioc, "NVIC.EXTI0_IRQn=true\\:5\\:0", "EXTI0 priority")
    assert_contains(ioc, "NVIC.TIM4_IRQn=true\\:5\\:0", "TIM4 IRQ priority")
    assert_contains(ioc, "NVIC.TIM5_IRQn=true\\:5\\:0", "TIM5 IRQ priority")
    assert_contains(main_h, "EXT_PULSE_IN_Pin", "EXT pulse pin define")
    assert_contains(gpio_c, "GPIO_MODE_IT_RISING", "external pulse rising edge")
    assert_contains(it_c, "EXTI0_IRQHandler", "EXTI0 IRQ handler")
    assert_contains(it_c, "TIM4_IRQHandler", "TIM4 IRQ handler")
    assert_contains(it_c, "TIM5_IRQHandler", "TIM5 IRQ handler")


def test_point_commands_arm_external_trigger_instead_of_pulsing_immediately():
    player = read("user/app/WGS/output/DacWavePlayer.hpp")
    output_mode = read("user/app/WGS/output/OutputMode.hpp")

    assert_contains(player, "armPointPulse", "single point should arm external trigger")
    assert_contains(player, "firePendingPointPulseFromIsr", "EXTI should fire armed point pulse")
    assert_contains(player, "completePointPulseFromIsr", "TIM update should clean point pulse")
    assert_contains(player, "prepareSinglePulse", "point command should preconfigure one-pulse")

    hold_channel = text_between(player, "APP::WGS::PROTOCOL::Result_s holdChannel", "  APP::WGS::PROTOCOL::Result_s holdChannels")
    hold_channels = text_between(player, "APP::WGS::PROTOCOL::Result_s holdChannels", "  APP::WGS::PROTOCOL::Result_s setPointPulseWidth")
    assert_not_contains(hold_channel, "pulseTimer(", "POINT_SET should not pulse before external trigger")
    assert_not_contains(hold_channels, "pulseTimer(", "POINT2_SET should not pulse before external trigger")
    assert_contains(hold_channel, "const bool wasBusy = isPointPulseBusy();", "POINT_SET should remember busy state")
    assert_contains(hold_channels, "const bool wasBusy = isPointPulseBusy();", "POINT2_SET should remember busy state")
    assert_order(hold_channel, "setDacValue", "if (wasBusy)", "POINT_SET should update DAC before reporting BUSY")
    assert_order(hold_channels, "setDacValue", "if (wasBusy)", "POINT2_SET should update DAC before reporting BUSY")
    assert_contains(hold_channel, "return APP::WGS::PROTOCOL::Result_s::busy();", "POINT_SET should still report BUSY")
    assert_contains(hold_channels, "return APP::WGS::PROTOCOL::Result_s::busy();", "POINT2_SET should still report BUSY")

    point_set_handler = text_between(output_mode, "const APP::WGS::PROTOCOL::PointSetCommand_s", "  APP::WGS::PROTOCOL::Result_s\n  handle(OutputContext_s<Player_s> &_ctx,\n         const APP::WGS::PROTOCOL::Point2SetCommand_s")
    point2_set_handler = text_between(output_mode, "const APP::WGS::PROTOCOL::Point2SetCommand_s", "  APP::WGS::PROTOCOL::Result_s\n  handle(OutputContext_s<Player_s> &_ctx,\n         const APP::WGS::PROTOCOL::PointPulseWidthSetCommand_s")
    assert_not_contains(point_set_handler, "isPointPulseBusy()", "POINT_SET mode handler should let player update DAC while busy")
    assert_not_contains(point2_set_handler, "isPointPulseBusy()", "POINT2_SET mode handler should let player update DAC while busy")
    assert_contains(output_mode, "isPointPulseBusy()", "non-point output commands should reject while point trigger is busy")
    assert_contains(output_mode, "Result_s::busy()", "busy point trigger should return BUSY")


def test_busy_is_reported_as_ack_busy_none():
    result_hpp = read("user/app/WGS/protocol/Result.hpp")
    wgs_cc = read("user/app/WGS/WGS.cc")

    assert_contains(result_hpp, "static constexpr Result_s busy()", "Result should represent ACK/BUSY")
    assert_contains(result_hpp, "isBusy()", "Result should expose busy state")
    assert_contains(wgs_cc, "AckStatus_e::BUSY", "busy result should send ACK/BUSY")
    assert_contains(wgs_cc, "ErrorCode_e::NONE", "ACK/BUSY detail should be NONE")


def test_tim_proxy_has_nonblocking_prepared_one_pulse_path():
    proxy = read("user/bsp/TIM/TimProxy.hpp")

    assert_contains(proxy, "prepareSinglePulse", "one-pulse should be prepared outside EXTI ISR")
    assert_contains(proxy, "firePreparedSinglePulseFromIsr", "EXTI ISR should only start prepared pulse")
    assert_contains(proxy, "completePreparedSinglePulseFromIsr", "TIM IRQ should clean prepared pulse")
    assert_contains(proxy, "disableUpdateInterrupt", "sequence mode should keep TIM update IRQ disabled")
    assert_contains(proxy, "enableUpdateInterrupt", "point firing should briefly enable TIM update IRQ")

    fire_path = text_between(proxy, "static HAL_StatusTypeDef firePreparedSinglePulseFromIsr", "  static bool completePreparedSinglePulseFromIsr")
    assert_not_contains(fire_path, "waitForOnePulseComplete", "EXTI fire path should not busy-wait")
    assert_not_contains(fire_path, "HAL_TIM_Base_Start", "EXTI fire path should not call HAL start")

    start_continuous = text_between(proxy, "static HAL_StatusTypeDef startContinuous", "  static HAL_StatusTypeDef stop()")
    assert_contains(start_continuous, "disableUpdateInterrupt", "sequence continuous mode should not produce TIM IRQs")


def test_exti_and_tim_callbacks_bridge_to_wgs_fast_path():
    wgs_hpp = read("user/app/WGS/WGS.hpp")
    wgs_cc = read("user/app/WGS/WGS.cc")
    main_c = read("Core/Src/main.c")

    assert_contains(wgs_hpp, "onExternalPointTriggerFromIsr", "WGS should handle external point trigger")
    assert_contains(wgs_hpp, "onPointPulseTimerUpdateFromIsr", "WGS should handle point pulse timer cleanup")
    assert_contains(wgs_hpp, "notifyWgsExternalPointTriggerFromIsr", "C bridge for EXTI")
    assert_contains(wgs_hpp, "notifyWgsTim4UpdateFromIsr", "C bridge for TIM4 update")
    assert_contains(wgs_hpp, "notifyWgsTim5UpdateFromIsr", "C bridge for TIM5 update")
    assert_contains(wgs_cc, "HAL_GPIO_EXTI_Callback", "GPIO EXTI callback should be implemented")
    assert_contains(wgs_cc, "EXT_PULSE_IN_Pin", "EXTI callback should filter EXT_PULSE_IN")
    assert_contains(wgs_cc, "firePendingPointPulseFromIsr", "EXTI should fire pending point pulse")
    assert_contains(main_c, "notifyWgsTim4UpdateFromIsr", "TIM4 callback should reach WGS")
    assert_contains(main_c, "notifyWgsTim5UpdateFromIsr", "TIM5 callback should reach WGS")


def test_point2_fifo_protocol_and_external_trigger_path():
    command = read("user/app/WGS/protocol/Command.hpp")
    wgs_cc = read("user/app/WGS/WGS.cc")
    player = read("user/app/WGS/output/DacWavePlayer.hpp")
    output_mode = read("user/app/WGS/output/OutputMode.hpp")

    assert_contains(command, "POINT2_FIFO_CLEAR = 0x14", "POINT2 FIFO clear command id")
    assert_contains(command, "POINT2_FIFO_APPEND = 0x15", "POINT2 FIFO append command id")
    assert_contains(command, "POINT2_FIFO_START = 0x16", "POINT2 FIFO start command id")
    assert_contains(command, "POINT2_FIFO_STOP = 0x17", "POINT2 FIFO stop command id")
    assert_contains(command, "POINT2_FIFO_APPEND_HEADER_SIZE = 3", "POINT2 FIFO append header size")
    assert_contains(command, "POINT2_FIFO_POINT_SIZE = 8", "POINT2 FIFO point size")
    assert_contains(command, "POINT2_FIFO_APPEND_MAX_POINTS = 500", "POINT2 FIFO append max points")
    assert_contains(command, "Point2FifoPoint_s", "POINT2 FIFO point type")
    assert_contains(command, "Point2FifoAppendCommand_s", "POINT2 FIFO append command type")

    assert_contains(wgs_cc, "POINT2_FIFO_APPEND_HEADER_SIZE", "POINT2 FIFO decode path")
    assert_contains(wgs_cc, "Point2FifoAppendCommand_s", "POINT2 FIFO decoded command")

    assert_contains(player, "POINT2_FIFO_CAPACITY = 1024", "POINT2 FIFO capacity")
    assert_contains(player, "appendPoint2Fifo", "POINT2 FIFO append API")
    assert_contains(player, "startPoint2Fifo", "POINT2 FIFO start API")
    assert_contains(player, "stopPoint2Fifo", "POINT2 FIFO stop API")
    assert_contains(player, "firePoint2FifoFromIsr", "EXTI should consume FIFO points")
    assert_contains(player, "clearPoint2Fifo()", "STOP should clear queued FIFO points")
    assert_contains(player, "point2FifoEnabled_", "player should track FIFO enable state")
    assert_order(player, "firePoint2FifoFromIsr", "uint8_t mask = POINT_PULSE_NONE", "FIFO should be checked before legacy pending pulse")

    assert_contains(output_mode, "Point2FifoClearCommand_s", "POINT2 FIFO clear dispatch")
    assert_contains(output_mode, "Point2FifoAppendCommand_s", "POINT2 FIFO append dispatch")
    assert_contains(output_mode, "Point2FifoStartCommand_s", "POINT2 FIFO start dispatch")
    assert_contains(output_mode, "Point2FifoStopCommand_s", "POINT2 FIFO stop dispatch")
    assert_contains(output_mode, "isPoint2FifoActive()", "non-FIFO commands should reject while FIFO is active")


def test_readme_documents_external_trigger_limitations():
    readme = read("user/app/WGS/README.md")

    assert_contains(readme, "EXT_PULSE_IN", "README should document external trigger input")
    assert_contains(readme, "ACK / BUSY / NONE", "README should document busy reply")
    assert_contains(readme, "仍会更新 DAC", "README should document point DAC updates while busy")
    assert_contains(readme, "不会重新 armed", "README should document point busy does not re-arm")
    assert_contains(readme, "5 kHz", "README should mention external pulse frequency")
    assert_contains(readme, "200 us", "README should mention the 5 kHz period")
    assert_contains(readme, "不做输入毛刺", "README should document no glitch filtering")


def run_all():
    tests = [
        test_cubemx_external_pulse_and_timer_irqs_are_enabled,
        test_point_commands_arm_external_trigger_instead_of_pulsing_immediately,
        test_busy_is_reported_as_ack_busy_none,
        test_tim_proxy_has_nonblocking_prepared_one_pulse_path,
        test_exti_and_tim_callbacks_bridge_to_wgs_fast_path,
        test_point2_fifo_protocol_and_external_trigger_path,
        test_readme_documents_external_trigger_limitations,
    ]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")


if __name__ == "__main__":
    run_all()
