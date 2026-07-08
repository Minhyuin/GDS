from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


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


def test_wgs_signals_buzzer_after_valid_protocol_command_decode():
    app_manager_hpp = read("user/app/AppManager.hpp")
    app_manager_cpp = read("user/app/AppManager.cpp")
    wgs_cc = read("user/app/WGS/WGS.cc")

    assert_contains(app_manager_hpp, "BuzzerTask", "AppManager should own the buzzer task")
    assert_contains(app_manager_cpp, "buzzer.init()", "AppManager should initialize buzzer before WGS use")
    assert_contains(wgs_cc, "Buzzer.hpp", "WGS should include buzzer signal API")
    assert_contains(wgs_cc, "notifyProtocolCommandReceived()", "WGS should notify buzzer on valid commands")


def run_all():
    tests = [
        test_buzzer_task_uses_pc0_and_blocks_on_counting_semaphore,
        test_wgs_signals_buzzer_after_valid_protocol_command_decode,
    ]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")


if __name__ == "__main__":
    run_all()
