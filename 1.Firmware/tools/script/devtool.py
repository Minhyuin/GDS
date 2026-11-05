"""GDS flash helper used by .vscode/tasks.json."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ELF = ROOT / "build" / "GDS.elf"
OPENOCD_DIR = ROOT / "tools" / "openocd"
JLINK_DEVICE = "STM32F103ZE"
OPENOCD_TARGET = "target/stm32f1x.cfg"
JLINK_SPEED_KHZ = 4000

OPENOCD_CFG = {
    "stlink": OPENOCD_DIR / "stlink.cfg",
    "daplink": OPENOCD_DIR / "dap.cfg",
}


def fail(message: str) -> None:
    raise SystemExit(f"[devtool] ERROR: {message}")


def find_jlink() -> Path:
    if os.name == "nt":
        import winreg

        for hive in (winreg.HKEY_CURRENT_USER, winreg.HKEY_LOCAL_MACHINE):
            try:
                with winreg.OpenKey(hive, r"SOFTWARE\SEGGER\J-Link") as key:
                    install_path = Path(winreg.QueryValueEx(key, "InstallPath")[0])
                commander = install_path / "JLink.exe"
                if commander.is_file():
                    return commander
            except OSError:
                pass
        for variable in ("ProgramFiles", "ProgramFiles(x86)"):
            if root := os.environ.get(variable):
                commander = Path(root) / "SEGGER" / "JLink" / "JLink.exe"
                if commander.is_file():
                    return commander
    elif commander := shutil.which("JLinkExe"):
        return Path(commander)

    fail("SEGGER J-Link Commander not found; install J-Link software first")


def find_openocd() -> Path:
    if command := shutil.which("openocd"):
        return Path(command)
    fail("openocd not found in PATH")


def flash_jlink(dry_run: bool) -> None:
    with tempfile.NamedTemporaryFile("w", suffix=".jlink", delete=False) as file:
        file.write(
            f"si SWD\n"
            f"speed {JLINK_SPEED_KHZ}\n"
            f"device {JLINK_DEVICE}\n"
            f"exec SetSkipDebugDeInit = 1\n"
            f"loadfile {ELF.as_posix()}\n"
            f"r\n"
            f"g\n"
            f"exit\n"
        )
        script = Path(file.name)

    argv = [
        find_jlink(),
        "-if", "SWD",
        "-speed", str(JLINK_SPEED_KHZ),
        "-autoconnect", "1",
        "-CommanderScript", script.as_posix(),
    ]
    try:
        run(argv, dry_run)
    finally:
        script.unlink(missing_ok=True)


def flash_openocd(probe: str, dry_run: bool) -> None:
    argv = [
        find_openocd(),
        "-f", OPENOCD_CFG[probe].as_posix(),
        "-f", OPENOCD_TARGET,
    ]
    if probe == "daplink":
        argv += ["-c", "reset_config none srst_nogate"]
    argv += ["-c", f"program {ELF.as_posix()} verify reset", "-c", "shutdown"]
    run(argv, dry_run)


def run(argv: list[Path | str], dry_run: bool) -> None:
    command = [str(arg) for arg in argv]
    if dry_run:
        print("[devtool] DRY-RUN: " + " ".join(command))
        return
    result = subprocess.run(command, cwd=ROOT)
    if result.returncode:
        fail(f"flash failed (rc={result.returncode})")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("flash",))
    parser.add_argument("probe", choices=("jlink", "stlink", "daplink"))
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    if not ELF.is_file():
        fail(f"{ELF} not found; run the 'build' task first")

    if args.probe == "jlink":
        flash_jlink(args.dry_run)
    else:
        flash_openocd(args.probe, args.dry_run)
    print(f"[devtool] flash {args.probe}: DRY-RUN OK" if args.dry_run else f"[devtool] flash {args.probe}: OK")


if __name__ == "__main__":
    main()
