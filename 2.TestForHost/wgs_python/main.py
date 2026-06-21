import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))

from source.app import main  # noqa: E402


if __name__ == "__main__":
    raise SystemExit(main())
