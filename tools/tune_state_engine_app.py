"""Compatibility wrapper for the old tuning-app entrypoint.

The black-box interaction app now lives in tools/plant_pet_app.py.
"""

from __future__ import annotations

import sys
from pathlib import Path


TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from plant_pet_app import analyze_payload, main  # noqa: E402,F401


if __name__ == "__main__":
    main()
