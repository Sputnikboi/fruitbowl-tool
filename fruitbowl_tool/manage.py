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



# ── Items whose dispatch files are synced copies of a canonical source ────
# These are skipped during scan to avoid duplicates. The canonical source
# is shown instead, grouped under a friendlier display name.
HAT_ITEMS = {"stone_button"}
SYNCED_HELMET_ITEMS = {
    "leather_helmet", "chainmail_helmet", "iron_helmet",
    "golden_helmet", "diamond_helmet", "netherite_helmet", "turtle_helmet",
}


def scan_all_models(pack_root: str) -> list[dict]:
    """
    Scan the entire pack and return a list of model info dicts.
    Each dict has: item_type, model_name, threshold, author, display_name,
                   has_texture, has_model, has_item_def.
    Source of truth is the dispatch JSONs in minecraft/items/.

    Helmet dispatch files are skipped (they're synced copies of stone_button).
    Hat items (stone_button, pale_oak_button) are grouped under a 'Hats' display.
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

        # Skip helmet dispatch files — they're synced copies of hat items
        if mc_item_id in SYNCED_HELMET_ITEMS:
            continue

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

        # Use "Hats" as display item_type for hat items
        display_item_type = "hats" if mc_item_id in HAT_ITEMS else mc_item_id

        for entry in entries:
            threshold = entry.get("threshold", 0)

            # Extract model ref — handle both simple and condition wrappers
            entry_model = entry.get("model", {})
            model_ref = entry_model.get("model", "")
            if not model_ref:
                # Check condition wrappers (trident/bow)
                model_ref = entry_model.get("on_false", {}).get("model", "")
            if not model_ref:
                model_ref = entry_model.get("on_true", {}).get("model", "")

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
                "item_type": display_item_type,
                "real_item_type": mc_item_id,
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


def _count_dispatch_references(pack_root: str, model_name: str,
                               exclude_item_id: str = "") -> int:
    """Count how many dispatch files reference this model, optionally excluding one."""
    mc_items_dir = os.path.join(pack_root, MC_ITEMS_DIR)
    if not os.path.isdir(mc_items_dir):
        return 0

    model_ref = f"fruitbowl:item/{model_name}"
    count = 0

    for filename in os.listdir(mc_items_dir):
        if not filename.endswith(".json"):
            continue
        item_id = os.path.splitext(filename)[0]
        if item_id == exclude_item_id:
            continue

        filepath = os.path.join(mc_items_dir, filename)
        try:
            with open(filepath, "r", encoding="utf-8") as f:
                data = json.load(f)
        except (json.JSONDecodeError, OSError):
            continue

        entries = data.get("model", {}).get("entries", [])
        for e in entries:
            m = e.get("model", {})
            # Check direct model ref
            if m.get("model") == model_ref:
                count += 1
                break
            # Check inside condition wrappers (trident/bow)
            for key in ("on_true", "on_false"):
                if m.get(key, {}).get("model") == model_ref:
                    count += 1
                    break

    return count


def delete_model(pack_root: str, mc_item_id: str, model_name: str,
                 threshold: int) -> list[tuple[str, str]]:
    """
    Delete a model from the pack. Removes up to 5 artifacts:
    1. Entry from minecraft item dispatch JSON
    2. Entry from model list.txt
    3. Texture PNG (only if no other dispatch files reference this model)
    4. Model JSON (only if no other dispatch files reference this model)
    5. Fruitbowl item def JSON (only if no other dispatch files reference this model)

    Returns a list of (tag, message) log tuples.
    """
    log = []

    # 1 — Remove from dispatch JSON (do this first)
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
                and not any(
                    e.get("model", {}).get(k, {}).get("model") == model_ref
                    for k in ("on_true", "on_false")
                )
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

    # 2 — Remove from model list.txt
    log.extend(_remove_from_model_list(pack_root, mc_item_id, threshold))

    # 3-5 — Only delete shared files if no other dispatch files still reference this model
    other_refs = _count_dispatch_references(pack_root, model_name)

    if other_refs > 0:
        log.append(("info", f"ℹ Keeping files — model still used by {other_refs} other item(s)"))
    else:
        # Texture
        tex_path = os.path.join(pack_root, TEXTURE_DIR, f"{model_name}.png")
        if os.path.exists(tex_path):
            os.remove(tex_path)
            log.append(("success", f"✓ Deleted texture: {model_name}.png"))
        else:
            log.append(("warn", f"⚠ Texture not found: {model_name}.png"))

        # Also delete secondary textures (multi-texture models: model_name_1.png etc.)
        tex_dir = os.path.join(pack_root, TEXTURE_DIR)
        if os.path.isdir(tex_dir):
            for f in os.listdir(tex_dir):
                if f.startswith(f"{model_name}_") and f.endswith(".png"):
                    os.remove(os.path.join(tex_dir, f))
                    log.append(("success", f"✓ Deleted texture: {f}"))

        # Model JSON
        model_path = os.path.join(pack_root, MODEL_DIR, f"{model_name}.json")
        if os.path.exists(model_path):
            os.remove(model_path)
            log.append(("success", f"✓ Deleted model: {model_name}.json"))
        else:
            log.append(("warn", f"⚠ Model file not found: {model_name}.json"))

        # Also delete throwing variant if present
        throwing_path = os.path.join(pack_root, MODEL_DIR, f"{model_name}_throwing.json")
        if os.path.exists(throwing_path):
            os.remove(throwing_path)
            log.append(("success", f"✓ Deleted model: {model_name}_throwing.json"))

        # Fruitbowl item def
        item_path = os.path.join(pack_root, FB_ITEMS_DIR, f"{model_name}.json")
        if os.path.exists(item_path):
            os.remove(item_path)
            log.append(("success", f"✓ Deleted item def: {model_name}.json"))
        else:
            log.append(("warn", f"⚠ Item def not found: {model_name}.json"))

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
