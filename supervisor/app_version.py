# SPDX-License-Identifier: GPL-3.0-or-later
"""The same metadata supplies the UI, package and supervisor version."""
import json
from pathlib import Path
import re

directory = Path(__file__).resolve().parent
metadata = directory / "app-builder.json"
if not metadata.exists(): metadata = directory.parent / "app-builder.json"
if metadata.stat().st_size > 65536: raise RuntimeError("Invalid app version metadata")
APP_VERSION = json.loads(metadata.read_text())["version"]
if not isinstance(APP_VERSION, str) or not re.fullmatch(r"\d+\.\d+\.\d+", APP_VERSION):
    raise RuntimeError("Invalid app version metadata")
