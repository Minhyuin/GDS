from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def assert_contains(text, needle, label):
    if needle not in text:
        raise AssertionError(f"{label}: expected to find {needle!r}")


def test_buzzer_task_uses_pc0_and_blocks_on_counting_semaphore():
    buzzer_hpp = read("user/app/Buzzer/Buzzer.hpp")
    buzzer_cpp = read("user/app/Buzzer/Buzzer.cpp")

    assert_contains(buzzer_hpp, "class BuzzerTask", "buzzer task type")
    assert_contains(buzzer_hpp, "StaticSemaphore_t", "static semaphore storage")
    assert_contains(buzzer_hpp, "BEEP_MS", "beep duration constant")
    assert_contains(buzzer_cpp, "xSemaphoreCreateCountingStatic", "counting semaphore API")
    assert_contains(buzzer_cpp, "xSemaphoreTake(semaphore_, portMAX_DELAY)", "buzzer task should block without signals")
    assert_contains(buzzer_cpp, "HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_SET)", "buzzer should drive PC0 high")
    assert_contains(buzzer_cpp, "vTaskDelay(pdMS_TO_TICKS(BEEP_MS))", "buzzer should hold high for a short pulse")
    assert_contains(buzzer_cpp, "HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET)", "buzzer should return PC0 low")


def test_wgs_signals_buzzer_only_for_ping_commands():
    app_manager_hpp = read("user/app/AppManager.hpp")
    app_manager_cpp = read("user/app/AppManager.cpp")
    wgs_cc = read("user/app/WGS/WGS.cc")

    assert_contains(app_manager_hpp, "BuzzerTask", "AppManager should own the buzzer task")
    assert_contains(app_manager_cpp, "buzzer.init()", "AppManager should initialize buzzer before WGS use")
    assert_contains(wgs_cc, "Buzzer.hpp", "WGS should include buzzer signal API")
    assert_contains(wgs_cc, "notifyProtocolCommandReceived()", "WGS should notify buzzer on ping commands")

    ping_branch = "if (std::holds_alternative<PROTOCOL::PingCommand_s>(command))"
    buzzer_call = "APP::BUZZER::notifyProtocolCommandReceived();"
    ping_index = wgs_cc.index(ping_branch)
    buzzer_index = wgs_cc.index(buzzer_call)
    ping_return_index = wgs_cc.index("return;", ping_index)

    if buzzer_index < ping_index:
        raise AssertionError("WGS should not notify the buzzer for every decoded command")
    if buzzer_index > ping_return_index:
        raise AssertionError("WGS should notify the buzzer inside the ping command branch")


def run_all():
    tests = [
        test_buzzer_task_uses_pc0_and_blocks_on_counting_semaphore,
        test_wgs_signals_buzzer_only_for_ping_commands,
    ]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")


if __name__ == "__main__":
    run_all()
