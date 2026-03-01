"""
Settings persistence for the Fruitbowl Resource Pack Tool.
Stores user preferences (pack path, custom headings) in a JSON file.
"""

import json
import os
import sys


def get_settings_path() -> str:
    if sys.platform == "win32":
        base = os.environ.get("APPDATA", os.path.expanduser("~"))
    else:
        base = os.path.expanduser("~")
    return os.path.join(base, ".fruitbowl_tool.json")


def load_settings() -> dict:
    path = get_settings_path()
    if os.path.exists(path):
        try:
            with open(path, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            pass
    return {}


def save_settings(settings: dict):
    path = get_settings_path()
    try:
        with open(path, "w", encoding="utf-8") as f:
            json.dump(settings, f, indent=2)
    except Exception:
        pass
