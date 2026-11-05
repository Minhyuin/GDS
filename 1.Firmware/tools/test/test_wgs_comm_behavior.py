from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from host_toolchain import compiler_env, resolve_cxx


ROOT = Path(__file__).resolve().parents[2]
TEST_DIR = ROOT / "tools" / "test"
CXXFLAGS = os.environ.get("CXXFLAGS", "")


def compile_and_run(source: str) -> tuple[bool, str]:
    cxx = resolve_cxx()
    with tempfile.TemporaryDirectory(prefix="gds-comm-") as tmp:
        exe = Path(tmp) / f"{Path(source).stem}.exe"
        cmd = [
            str(cxx),
            "-std=c++23",
            "-Wall",
            "-Wextra",
            "-Werror",
            *shlex.split(CXXFLAGS),
            "-static",
            f"-I{ROOT / 'user' / 'app' / 'WGS' / 'comm'}",
            f"-I{ROOT / 'user' / 'app' / 'WGS' / 'protocol'}",
            f"-I{ROOT / 'user' / 'app' / 'WGS' / 'output'}",
            str(TEST_DIR / source),
            "-o",
            str(exe),
        ]
        env = compiler_env(cxx)
        built = subprocess.run(
            cmd, cwd=ROOT, env=env, text=True, capture_output=True
        )
        if built.returncode:
            return False, built.stdout + built.stderr
        run = subprocess.run(
            [str(exe)], cwd=ROOT, env=env, text=True, capture_output=True
        )
        return run.returncode == 0, run.stdout + run.stderr


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


def test_real_templates_fix_comm_behavior():
    ok, output = compile_and_run("test_wgs_comm_behavior.cpp")
    print(output, end="")
    assert ok


def test_protocol_service_has_no_parser_dependency():
    ok, output = compile_and_run("test_wgs_comm_behavior_protocol.cpp")
    print(output, end="")
    assert ok


def test_wgs_glue_uses_task_side_rx_recovery_and_no_tx_completion():
    wgs_hpp = read("user/app/WGS/WGS.hpp")
    wgs_cc = read("user/app/WGS/WGS.cc")
    transport = read("user/app/WGS/comm/UsartDmaTransport.hpp")
    proxy = read("user/bsp/UART/UsartProxy.hpp")

    assert "using Parser = PROTOCOL::ProtocolParser" in wgs_hpp
    assert "ProtocolService<Transport, PROTOCOL::GdsFrameV1Policy_s>" in wgs_hpp
    assert "onTxCompleteFromIsr" not in wgs_hpp
    assert "notifyWgsTxCompleteFromIsr" not in wgs_cc
    assert "HAL_UART_TxCpltCallback" not in wgs_cc
    assert "TX_BUSY" not in transport

    error_callback = between(
        wgs_cc,
        'extern "C" void HAL_UART_ErrorCallback',
        "\n}",
    )
    assert "HAL_UARTEx_ReceiveToIdle_DMA" not in error_callback
    assert "notifyWgsRxErrorFromIsr" in error_callback

    recovery = wgs_cc[wgs_cc.index("recoverRx") : wgs_cc.index("void WGSManager::handleFrame")]
    assert "parser_.reset();" in recovery
    assert "transport_.restart()" in recovery
    assert ".ok()" in recovery

    assert "__HAL_DMA_DISABLE_IT(&rxDma(), DMA_IT_HT)" not in proxy


def test_rx_snapshot_is_single_irq_critical_section_and_recovery_is_outer_loop_only():
    wgs_cc = read("user/app/WGS/WGS.cc")
    transport = read("user/app/WGS/comm/UsartDmaTransport.hpp")
    proxy = read("user/bsp/UART/UsartProxy.hpp")

    assert "COPY_ATTEMPTS" not in transport
    assert "isrEventSequence_" not in transport
    assert "SNAPSHOT_ATTEMPTS" not in transport
    assert "Proxy::irqSave()" in transport
    assert "Proxy::irqRestore(" in transport
    assert "__get_PRIMASK()" in proxy
    assert "__disable_irq();" in proxy
    assert "__set_PRIMASK(" in proxy
    assert "RX_BUFFER_SIZE / 2U" in transport

    process_rx = between(
        wgs_cc,
        "void WGSManager::processRx()",
        "void WGSManager::handleFrame",
    )
    assert "recoverRx()" not in process_rx
    assert "hasPendingError()" in process_rx

    task_loop = between(
        wgs_cc,
        "void WGSManager::taskLoop()",
        "PROTOCOL::Result_s WGSManager::recoverRx()",
    )
    assert "processRx();" in task_loop
    process_index = task_loop.index("processRx();")
    wait_guard = task_loop.index(
        "if (transport_.hasPendingError())", process_index
    )
    assert process_index < wait_guard < task_loop.index("ulTaskNotifyTake")


def test_hal_timebase_is_configured_for_blocking_uart_timeouts():
    timebase = read("Core/Src/stm32f1xx_hal_timebase_tim.c")
    main_c = read("Core/Src/main.c")
    assert "htim2.Init.Period = (1000000U / 1000U) - 1U;" in timebase
    assert "HAL_IncTick();" in main_c
    assert "if (htim->Instance == TIM2)" in main_c
