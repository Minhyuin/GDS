from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from host_toolchain import compiler_env, resolve_cxx

ROOT = Path(__file__).resolve().parents[2]
INCLUDE = [
    ROOT / "tools" / "test" / "support",
    ROOT / "user" / "bsp" / "TIM",
    ROOT / "user" / "bsp" / "DAC",
    ROOT / "user" / "app" / "WGS" / "output",
    ROOT / "user" / "app" / "WGS" / "protocol",
]
CASES = [
    "stop",
    "frequency",
    "cancel",
    "busy",
    "fifo_channel",
    "dac_direct",
    "sequence",
    "fifo_stop",
    "mode_exclusion",
    "active_length",
    "invalid_input",
    "fifo_capacity",
    "repeat_start",
    "other_channel",
    "running_frequency",
    "fifo_stop_channel",
]


def test_output_behavior():
    cxx = resolve_cxx()
    with tempfile.TemporaryDirectory(prefix="gds-output-") as tmp:
        exe = Path(tmp) / "output.exe"
        subprocess.run(
            [
                str(cxx),
                "-std=c++23",
                "-static",
                *shlex.split(os.environ.get("CXXFLAGS", "")),
                "-Wall",
                "-Wextra",
                *[f"-I{path}" for path in INCLUDE],
                str(ROOT / "tools" / "test" / "wgs_output_behavior.cpp"),
                str(ROOT / "user" / "app" / "WGS" / "output" / "WaveformStore.cpp"),
                str(ROOT / "user" / "bsp" / "DAC" / "DacProxy.cpp"),
                "-o",
                str(exe),
            ],
            check=True,
            env=compiler_env(cxx),
        )
        failed = []
        for case in CASES:
            result = subprocess.run([str(exe), case], env=compiler_env(cxx))
            if result.returncode:
                failed.append((case, result.returncode))
        if failed:
            raise SystemExit(f"FAILED: {failed}")


if __name__ == "__main__":
    test_output_behavior()
