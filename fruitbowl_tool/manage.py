"""
Manage Pack logic for the Fruitbowl Resource Pack Tool.
Scan all models in the pack, get model details, and delete models.
No GUI dependency — pure logic only.
"""

import json
import os
import re

from .constants import (
    TEXTURE_DIR, MODEL_DIR, FB_ITEMS_DIR, MC_ITEMS_DIR,
    MODEL_LIST, ITEM_TO_HEADING,
)


def _parse_model_list(pack_root: str) -> dict[str, dict[str, str]]:
    """
    Parse model list.txt into a lookup: model_name -> {"author": str, "display": str}.
    Matches entries like '3 = Arys Bag (Brok)' by normalizing the display name.
    Returns a dict keyed by sanitized model name approximation AND by threshold per heading.
    """
    list_path = os.path.join(pack_root, MODEL_LIST)
    if not os.path.exists(list_path):
        return {}

    # Build reverse heading map: heading -> [item_ids]
    heading_to_items: dict[str, list[str]] = {}
    for item_id, heading in ITEM_TO_HEADING.items():
        heading_to_items.setdefault(heading, []).append(item_id)

    result: dict[str, dict[str, str]] = {}

    with open(list_path, "r", encoding="utf-8") as f:
        lines = f.read().splitlines()

    current_heading = ""
    for line in lines:
        stripped = line.strip()

        # Heading line like "Stone Button:"
        if stripped.endswith(":") and not re.match(r"^\d+\s*=", stripped):
            current_heading = stripped[:-1]
            continue

        # Entry line like "3 = Arys Bag (Brok)"
        m = re.match(r"^(\d+)\s*=\s*(.+)$", stripped)
        if not m:
            continue

        threshold = int(m.group(1))
        rest = m.group(2).strip()

        # Extract author from parentheses at end
        author = ""
        author_match = re.match(r"^(.+?)\s*\(([^)]+)\)\s*$", rest)
        if author_match:
            display = author_match.group(1).strip()
            author = author_match.group(2).strip()
        else:
            display = rest

        # Store keyed by (heading, threshold) so we can look up later
        key = f"{current_heading}|{threshold}"
        result[key] = {"author": author, "display": display, "heading": current_heading}

    return result


def _find_author(model_list_data: dict, heading: str, threshold: int) -> str:
    """Look up author from parsed model list data."""
    key = f"{heading}|{threshold}"
    entry = model_list_data.get(key, {})
    return entry.get("author", "")


def _heading_for_item(mc_item_id: str) -> str:
    """Get the model list.txt heading for a given MC item ID."""
    return ITEM_TO_HEADING.get(mc_item_id, mc_item_id.replace("_", " ").title())


def scan_all_models(pack_root: str) -> list[dict]:
    """
    Scan the entire pack and return a list of model info dicts.
    Each dict has: item_type, model_name, threshold, author, display_name,
                   has_texture, has_model, has_item_def.
    Source of truth is the dispatch JSONs in minecraft/items/.
    """
    models = []
    mc_items_dir = os.path.join(pack_root, MC_ITEMS_DIR)

    if not os.path.isdir(mc_items_dir):
        return models

    # Parse model list.txt once for author lookup
    model_list_data = _parse_model_list(pack_root)

    for filename in sorted(os.listdir(mc_items_dir)):
        if not filename.endswith(".json"):
            continue

        mc_item_id = os.path.splitext(filename)[0]
        filepath = os.path.join(mc_items_dir, filename)

        try:
            with open(filepath, "r", encoding="utf-8") as f:
                data = json.load(f)
        except (json.JSONDecodeError, OSError):
            continue

        # Only process range_dispatch files (skip non-fruitbowl items)
        model_data = data.get("model", {})
        if model_data.get("type") != "minecraft:range_dispatch":
            continue
        if model_data.get("property") != "minecraft:custom_model_data":
            continue

        entries = model_data.get("entries", [])
        heading = _heading_for_item(mc_item_id)

        for entry in entries:
            threshold = entry.get("threshold", 0)
            model_ref = entry.get("model", {}).get("model", "")

            # Only include fruitbowl models
            if not model_ref.startswith("fruitbowl:item/"):
                continue

            model_name = model_ref.replace("fruitbowl:item/", "")

            # Check which files exist
            has_texture = os.path.exists(
                os.path.join(pack_root, TEXTURE_DIR, f"{model_name}.png"))
            has_model = os.path.exists(
                os.path.join(pack_root, MODEL_DIR, f"{model_name}.json"))
            has_item_def = os.path.exists(
                os.path.join(pack_root, FB_ITEMS_DIR, f"{model_name}.json"))

            author = _find_author(model_list_data, heading, threshold)
            display_name = model_name.replace("_", " ").title()

            models.append({
                "item_type": mc_item_id,
                "model_name": model_name,
                "threshold": threshold,
                "author": author,
                "display_name": display_name,
                "heading": heading,
                "has_texture": has_texture,
                "has_model": has_model,
                "has_item_def": has_item_def,
            })

    return models


def delete_model(pack_root: str, mc_item_id: str, model_name: str,
                 threshold: int) -> list[tuple[str, str]]:
    """
    Delete a model from the pack. Removes up to 5 artifacts:
    1. Texture PNG
    2. Model JSON
    3. Fruitbowl item def JSON
    4. Entry from minecraft item dispatch JSON
    5. Entry from model list.txt

    Returns a list of (tag, message) log tuples.
    """
    log = []

    # 1 — Texture
    tex_path = os.path.join(pack_root, TEXTURE_DIR, f"{model_name}.png")
    if os.path.exists(tex_path):
        os.remove(tex_path)
        log.append(("success", f"✓ Deleted texture: {model_name}.png"))
    else:
        log.append(("warn", f"⚠ Texture not found: {model_name}.png"))

    # 2 — Model JSON
    model_path = os.path.join(pack_root, MODEL_DIR, f"{model_name}.json")
    if os.path.exists(model_path):
        os.remove(model_path)
        log.append(("success", f"✓ Deleted model: {model_name}.json"))
    else:
        log.append(("warn", f"⚠ Model file not found: {model_name}.json"))

    # 3 — Fruitbowl item def
    item_path = os.path.join(pack_root, FB_ITEMS_DIR, f"{model_name}.json")
    if os.path.exists(item_path):
        os.remove(item_path)
        log.append(("success", f"✓ Deleted item def: {model_name}.json"))
    else:
        log.append(("warn", f"⚠ Item def not found: {model_name}.json"))

    # 4 — Remove from dispatch JSON
    mc_item_path = os.path.join(pack_root, MC_ITEMS_DIR, f"{mc_item_id}.json")
    if os.path.exists(mc_item_path):
        try:
            with open(mc_item_path, "r", encoding="utf-8") as f:
                data = json.load(f)
            entries = data["model"]["entries"]
            original_count = len(entries)
            model_ref = f"fruitbowl:item/{model_name}"
            data["model"]["entries"] = [
                e for e in entries
                if e.get("model", {}).get("model") != model_ref
            ]
            removed = original_count - len(data["model"]["entries"])
            if removed:
                with open(mc_item_path, "w", encoding="utf-8") as f:
                    json.dump(data, f, indent=2)
                    f.write("\n")
                log.append(("success", f"✓ Removed from {mc_item_id}.json dispatch"))
            else:
                log.append(("warn", f"⚠ Not found in {mc_item_id}.json dispatch"))
        except Exception as e:
            log.append(("error", f"✗ Error updating {mc_item_id}.json: {e}"))
    else:
        log.append(("warn", f"⚠ Dispatch file not found: {mc_item_id}.json"))

    # 5 — Remove from model list.txt
    log.extend(_remove_from_model_list(pack_root, mc_item_id, threshold))

    return log


def _remove_from_model_list(pack_root: str, mc_item_id: str,
                            threshold: int) -> list[tuple[str, str]]:
    """Remove a threshold entry from model list.txt."""
    list_path = os.path.join(pack_root, MODEL_LIST)
    if not os.path.exists(list_path):
        return [("warn", "⚠ model list.txt not found")]

    with open(list_path, "r", encoding="utf-8") as f:
        lines = f.read().splitlines()

    heading = _heading_for_item(mc_item_id)
    heading_pattern = f"{heading}:"

    # Find the heading
    heading_idx = None
    for i, line in enumerate(lines):
        if line.strip() == heading_pattern:
            heading_idx = i
            break

    if heading_idx is None:
        return [("warn", f"⚠ Heading '{heading}' not found in model list.txt")]

    # Find section boundaries
    section_end = len(lines)
    for i in range(heading_idx + 1, len(lines)):
        stripped = lines[i].strip()
        if stripped and not re.match(r"^\d+\s*=", stripped):
            section_end = i
            break

    # Find and remove the threshold line
    removed = False
    for i in range(heading_idx + 1, section_end):
        if re.match(rf"^{threshold}\s*=", lines[i].strip()):
            lines.pop(i)
            removed = True
            break

    if not removed:
        return [("warn", f"⚠ Threshold {threshold} not found under '{heading}' in model list.txt")]

    with open(list_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    return [("success", f"✓ Removed threshold {threshold} from model list.txt")]
