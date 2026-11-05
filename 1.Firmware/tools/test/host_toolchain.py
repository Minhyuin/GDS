from __future__ import annotations

import os
import shutil
from pathlib import Path


def resolve_cxx() -> Path:
    """Find the host compiler, honoring CXX before PATH."""
    configured = os.environ.get("CXX")
    if configured:
        resolved = shutil.which(configured)
        if resolved is None:
            candidate = Path(configured).expanduser()
            if candidate.is_file():
                resolved = str(candidate)
        if resolved is None:
            raise RuntimeError(
                f"CXX is set to {configured!r}, but no executable was found"
            )
        return Path(resolved).resolve()

    for name in ("g++", "clang++"):
        resolved = shutil.which(name)
        if resolved is not None:
            return Path(resolved).resolve()

    raise RuntimeError(
        "No host C++ compiler found. Set CXX or add g++/clang++ to PATH."
    )


def compiler_env(cxx: Path) -> dict[str, str]:
    """Add compiler-adjacent runtime directories to PATH."""
    env = os.environ.copy()
    search_dirs = (
        cxx.parent,
        cxx.parent / "cpp",
        cxx.parent.parent / "usr" / "bin",
    )
    prefix = os.pathsep.join(str(path) for path in search_dirs if path.is_dir())
    if prefix:
        env["PATH"] = prefix + os.pathsep + env.get("PATH", "")
    return env
