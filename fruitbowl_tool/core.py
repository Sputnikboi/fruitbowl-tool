"""
Core logic for the Fruitbowl Resource Pack Tool.
All functions here are pure logic with no GUI dependency.
"""

import base64
import json
import os
import re

from .constants import (
    TEXTURE_DIR, MODEL_DIR, FB_ITEMS_DIR, MC_ITEMS_DIR, ATLAS_DIR,
    MODEL_LIST, KNOWN_ITEMS, ITEM_TO_HEADING, BLOCK_ITEM_FALLBACKS,
)


def sanitize_name(name: str) -> str:
    """Lowercase, replace spaces/special chars with underscores."""
    name = os.path.splitext(os.path.basename(name))[0]
    name = name.lower().strip()
    name = re.sub(r"[^a-z0-9_]", "_", name)
    name = re.sub(r"_+", "_", name).strip("_")
    return name


def display_name_from_model(model_name: str) -> str:
    """Convert model_name like 'jets_helm' to display 'Jets Helm'."""
    return model_name.replace("_", " ").title()


def load_bbmodel(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def _find_primary_texture_index(bbmodel: dict) -> int:
    """Find which texture index is most used by element faces."""
    usage: dict[int, int] = {}
    for el in bbmodel.get("elements", []):
        for face_data in el.get("faces", {}).values():
            tex = face_data.get("texture")
            if tex is not None:
                usage[tex] = usage.get(tex, 0) + 1
    if not usage:
        return 0
    return max(usage, key=usage.get)


def extract_texture_png(bbmodel: dict) -> bytes:
    """Pull the most-used embedded texture's base64 PNG data."""
    textures = bbmodel.get("textures", [])
    if not textures:
        raise ValueError("No textures found in .bbmodel file.")

    idx = _find_primary_texture_index(bbmodel)
    if idx >= len(textures):
        idx = 0  # fallback to first if index is out of range

    source = textures[idx].get("source", "")
    if not source.startswith("data:image/png;base64,"):
        raise ValueError(f"Texture [{idx}] is not an embedded base64 PNG.")
    b64 = source.split(",", 1)[1]
    return base64.b64decode(b64)


def build_model_json(bbmodel: dict, model_name: str) -> dict:
    """Convert .bbmodel → Minecraft resource pack model JSON."""
    res = bbmodel.get("resolution", {"width": 16, "height": 16})
    texture_size = [res["width"], res["height"]]

    # UV scaling: bbmodel stores pixel coordinates, MC expects 0-16 based coords
    uv_scale_x = 16.0 / res["width"]
    uv_scale_y = 16.0 / res["height"]

    # Find the primary texture and map all references to it
    primary_idx = _find_primary_texture_index(bbmodel)
    textures = {
        str(primary_idx): f"fruitbowl:item/{model_name}",
        "particle": f"fruitbowl:item/{model_name}",
    }
    # Also map any other texture indices used by faces to the same texture
    for el in bbmodel.get("elements", []):
        for face_data in el.get("faces", {}).values():
            tex = face_data.get("texture")
            if tex is not None and str(tex) not in textures:
                textures[str(tex)] = f"fruitbowl:item/{model_name}"

    # Convert elements
    elements = []
    for el in bbmodel.get("elements", []):
        inflate = el.get("inflate", 0)
        el_from = [round(c - inflate, 4) for c in el["from"]]
        el_to = [round(c + inflate, 4) for c in el["to"]]

        entry = {
            "from": el_from,
            "to": el_to,
            "faces": {},
        }

        # Rotation — MC only supports one axis per element
        if "rotation" in el and "origin" in el:
            rot = el["rotation"]
            origin = el["origin"]
            axes = ["x", "y", "z"]
            for i, axis in enumerate(axes):
                if rot[i] != 0:
                    entry["rotation"] = {
                        "angle": rot[i],
                        "axis": axis,
                        "origin": origin,
                    }
                    break

        # Faces
        for face_name, face_data in el.get("faces", {}).items():
            raw_uv = face_data["uv"]
            face = {
                "uv": [
                    raw_uv[0] * uv_scale_x,
                    raw_uv[1] * uv_scale_y,
                    raw_uv[2] * uv_scale_x,
                    raw_uv[3] * uv_scale_y,
                ],
            }
            if "texture" in face_data and face_data["texture"] is not None:
                face["texture"] = f"#{face_data['texture']}"
            if face_data.get("rotation"):
                face["rotation"] = face_data["rotation"]
            if face_data.get("tintindex") is not None:
                face["tintindex"] = face_data["tintindex"]
            entry["faces"][face_name] = face

        if el.get("shade") is False:
            entry["shade"] = False

        elements.append(entry)

    model = {
        "credit": "Made with Blockbench",
        "texture_size": texture_size,
        "textures": textures,
        "elements": elements,
    }

    if "display" in bbmodel:
        model["display"] = bbmodel["display"]

    if bbmodel.get("groups"):
        model["groups"] = bbmodel["groups"]

    return model


def build_fruitbowl_item_json(model_name: str) -> dict:
    return {
        "model": {
            "type": "minecraft:model",
            "model": f"fruitbowl:item/{model_name}",
        }
    }


def update_minecraft_item(mc_item_path: str, model_name: str, mc_item_id: str) -> tuple[int, bool]:
    """
    Add a new entry to the minecraft item's range_dispatch JSON.
    Returns (threshold, already_existed).
    """
    if os.path.exists(mc_item_path):
        with open(mc_item_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        entries = data["model"]["entries"]
        next_threshold = max((e["threshold"] for e in entries), default=0) + 1
    else:
        # Block-type items need a block model fallback, not item model
        fallback_model = BLOCK_ITEM_FALLBACKS.get(
            mc_item_id, f"minecraft:item/{mc_item_id}")
        data = {
            "model": {
                "type": "minecraft:range_dispatch",
                "property": "minecraft:custom_model_data",
                "index": 0,
                "fallback": {
                    "type": "minecraft:model",
                    "model": fallback_model,
                },
                "entries": [],
            }
        }
        entries = data["model"]["entries"]
        next_threshold = 1

    for e in entries:
        m = e.get("model", {})
        # Check both simple models and condition/select wrappers
        if m.get("model") == f"fruitbowl:item/{model_name}":
            return e["threshold"], True
        # Check inside condition wrappers (trident/bow)
        for key in ("on_true", "on_false"):
            if m.get(key, {}).get("model") == f"fruitbowl:item/{model_name}":
                return e["threshold"], True

    # Items that need special dispatch entries
    if mc_item_id == "trident":
        # Trident needs using_item condition so throwing orientation works
        new_entry = {
            "threshold": next_threshold,
            "model": {
                "type": "minecraft:condition",
                "property": "minecraft:using_item",
                "on_true": {
                    "type": "minecraft:model",
                    "model": f"fruitbowl:item/{model_name}",
                },
                "on_false": {
                    "type": "minecraft:model",
                    "model": f"fruitbowl:item/{model_name}",
                },
            },
        }
    elif mc_item_id == "bow":
        # Bow needs using_item condition so pulling state works
        new_entry = {
            "threshold": next_threshold,
            "model": {
                "type": "minecraft:condition",
                "property": "minecraft:using_item",
                "on_true": {
                    "type": "minecraft:model",
                    "model": f"fruitbowl:item/{model_name}",
                },
                "on_false": {
                    "type": "minecraft:model",
                    "model": f"fruitbowl:item/{model_name}",
                },
            },
        }
    else:
        new_entry = {
            "threshold": next_threshold,
            "model": {
                "type": "minecraft:model",
                "model": f"fruitbowl:item/{model_name}",
            },
        }

    entries.append(new_entry)

    with open(mc_item_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
        f.write("\n")

    return next_threshold, False


def ensure_atlas(pack_root: str) -> list[tuple[str, str]]:
    """
    Ensure the blocks atlas includes an item/ directory source for custom textures.
    Custom textures must be in the blocks atlas ONLY — having them in both the
    blocks and items atlases causes 'Multiple atlases used in model' errors for
    block items like stone_button.
    Also removes item/ from the items atlas if present to prevent duplicates.
    """
    log = []
    atlas_dir = os.path.join(pack_root, ATLAS_DIR)
    os.makedirs(atlas_dir, exist_ok=True)

    # ── Ensure blocks.json atlas has the item/ directory source ──────
    blocks_atlas_path = os.path.join(atlas_dir, "blocks.json")
    if os.path.exists(blocks_atlas_path):
        with open(blocks_atlas_path, "r", encoding="utf-8") as f:
            blocks_atlas = json.load(f)
    else:
        blocks_atlas = {"sources": []}

    has_item_in_blocks = any(
        s.get("source") == "item" and s.get("prefix") == "item/"
        for s in blocks_atlas.get("sources", [])
    )

    if not has_item_in_blocks:
        blocks_atlas["sources"].append({
            "type": "minecraft:directory",
            "source": "item",
            "prefix": "item/"
        })
        with open(blocks_atlas_path, "w", encoding="utf-8") as f:
            json.dump(blocks_atlas, f, indent=2)
            f.write("\n")
        log.append(("success", "✓ Fixed blocks atlas — added item/ directory source"))

    # ── Remove item/ from items.json if present (causes duplicates) ──
    items_atlas_path = os.path.join(atlas_dir, "items.json")
    if os.path.exists(items_atlas_path):
        with open(items_atlas_path, "r", encoding="utf-8") as f:
            items_atlas = json.load(f)

        original_count = len(items_atlas.get("sources", []))
        items_atlas["sources"] = [
            s for s in items_atlas.get("sources", [])
            if not (s.get("source") == "item" and s.get("prefix") == "item/")
        ]

        if len(items_atlas["sources"]) < original_count:
            with open(items_atlas_path, "w", encoding="utf-8") as f:
                json.dump(items_atlas, f, indent=2)
                f.write("\n")
            log.append(("success", "✓ Fixed items atlas — removed duplicate item/ source"))

    return log


def check_existing(pack_root: str, model_name: str) -> list[str]:
    """Check which files already exist for this model name."""
    existing = []
    for subdir, ext in [(TEXTURE_DIR, ".png"), (MODEL_DIR, ".json"), (FB_ITEMS_DIR, ".json")]:
        path = os.path.join(pack_root, subdir, f"{model_name}{ext}")
        if os.path.exists(path):
            existing.append(os.path.relpath(path, pack_root))
    return existing


def update_model_list(pack_root: str, mc_item_id: str, model_name: str,
                      threshold: int, author: str = "",
                      heading_override: str = "") -> tuple[str, str]:
    """
    Add or update an entry in model list.txt under the right heading.
    heading_override lets the caller specify a custom heading name.
    Returns a (tag, message) log tuple.
    """
    list_path = os.path.join(pack_root, MODEL_LIST)

    heading = heading_override or ITEM_TO_HEADING.get(mc_item_id)
    if not heading:
        heading = mc_item_id.replace("_", " ").title()

    display = display_name_from_model(model_name)
    if author:
        entry_text = f"{threshold} = {display} ({author})"
    else:
        entry_text = f"{threshold} = {display}"

    # Read existing file or start fresh
    if os.path.exists(list_path):
        with open(list_path, "r", encoding="utf-8") as f:
            content = f.read()
        lines = content.splitlines()
    else:
        lines = []

    # Find the heading line
    heading_pattern = f"{heading}:"
    heading_idx = None
    for i, line in enumerate(lines):
        if line.strip() == heading_pattern:
            heading_idx = i
            break

    if heading_idx is not None:
        # Find the range of lines belonging to this section
        section_end = len(lines)
        for i in range(heading_idx + 1, len(lines)):
            stripped = lines[i].strip()
            if stripped and not re.match(r"^\d+\s*=", stripped):
                section_end = i
                break

        # Check for existing entry with same threshold
        for i in range(heading_idx + 1, section_end):
            if re.match(rf"^{threshold}\s*=", lines[i].strip()):
                lines[i] = entry_text
                with open(list_path, "w", encoding="utf-8") as f:
                    f.write("\n".join(lines) + "\n")
                return ("warn", f"⚠ Updated existing entry in model list: {entry_text}")

        # Append after last entry in section
        insert_at = section_end
        while insert_at > heading_idx + 1 and not lines[insert_at - 1].strip():
            insert_at -= 1
        lines.insert(insert_at, entry_text)
    else:
        # Heading doesn't exist yet — append a new section
        if lines and lines[-1].strip():
            lines.append("")
            lines.append("")
        lines.append(f"{heading}:")
        lines.append(entry_text)

    with open(list_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    return ("success", f"✓ Model list → {entry_text}")


def add_to_pack(bbmodel_path: str, pack_root: str, mc_item_id: str,
                model_name: str | None = None,
                author: str = "",
                heading_override: str = "") -> list[tuple[str, str]]:
    """
    Full pipeline: extract from bbmodel and add to pack.
    Returns a list of (tag, message) tuples.
    """
    log = []

    bbmodel = load_bbmodel(bbmodel_path)

    if not model_name:
        model_name = sanitize_name(os.path.basename(bbmodel_path))
    else:
        model_name = sanitize_name(model_name)

    if not model_name:
        raise ValueError("Could not derive a valid model name.")

    texture_path = os.path.join(pack_root, TEXTURE_DIR, f"{model_name}.png")
    model_path   = os.path.join(pack_root, MODEL_DIR, f"{model_name}.json")
    fb_item_path = os.path.join(pack_root, FB_ITEMS_DIR, f"{model_name}.json")
    mc_item_path = os.path.join(pack_root, MC_ITEMS_DIR, f"{mc_item_id}.json")

    # Ensure the texture atlas is configured
    log.extend(ensure_atlas(pack_root))

    # Check what already exists
    existing = check_existing(pack_root, model_name)
    if existing:
        log.append(("warn", f"⚠ Updating existing model '{model_name}' — overwriting:"))
        for ex in existing:
            log.append(("warn", f"    {ex}"))
    else:
        log.append(("info", f"Adding new model '{model_name}'"))

    # 1 — Texture
    png_data = extract_texture_png(bbmodel)
    os.makedirs(os.path.dirname(texture_path), exist_ok=True)
    with open(texture_path, "wb") as f:
        f.write(png_data)
    log.append(("success", f"✓ Texture → textures/item/{model_name}.png"))

    # 2 — Model JSON
    model_json = build_model_json(bbmodel, model_name)
    os.makedirs(os.path.dirname(model_path), exist_ok=True)
    with open(model_path, "w", encoding="utf-8") as f:
        json.dump(model_json, f, indent=2)
        f.write("\n")
    log.append(("success", f"✓ Model  → models/item/{model_name}.json"))

    # 3 — Fruitbowl item JSON
    fb_item = build_fruitbowl_item_json(model_name)
    os.makedirs(os.path.dirname(fb_item_path), exist_ok=True)
    with open(fb_item_path, "w", encoding="utf-8") as f:
        json.dump(fb_item, f, indent=2)
        f.write("\n")
    log.append(("success", f"✓ Item   → items/{model_name}.json"))

    # 4 — Minecraft item dispatch
    os.makedirs(os.path.dirname(mc_item_path), exist_ok=True)
    threshold, was_duplicate = update_minecraft_item(mc_item_path, model_name, mc_item_id)
    if was_duplicate:
        log.append(("warn", f"⚠ Already in {mc_item_id}.json — keeping threshold {threshold}"))
    else:
        log.append(("success", f"✓ Registered in {mc_item_id}.json → threshold {threshold}"))

    # 5 — Model list
    log.append(update_model_list(pack_root, mc_item_id, model_name, threshold, author, heading_override))

    log.append(("info", f"  → /trigger CustomModelData set {threshold}"))

    return log


def heading_exists_in_list(pack_root: str, heading: str) -> bool:
    """Check if a heading already exists in model list.txt."""
    list_path = os.path.join(pack_root, MODEL_LIST)
    if not os.path.exists(list_path):
        return False
    with open(list_path, "r", encoding="utf-8") as f:
        for line in f:
            if line.strip() == f"{heading}:":
                return True
    return False


def needs_heading_name(pack_root: str, mc_item_id: str) -> bool:
    """Check if this item type needs a custom heading name for model list.txt."""
    if mc_item_id in ITEM_TO_HEADING:
        return False
    fallback = mc_item_id.replace("_", " ").title()
    return not heading_exists_in_list(pack_root, fallback)


def scan_pack_items(pack_root: str) -> list[str]:
    """Scan the pack's minecraft/items dir to find available item types."""
    items_dir = os.path.join(pack_root, MC_ITEMS_DIR)
    items = []
    if os.path.isdir(items_dir):
        for f in sorted(os.listdir(items_dir)):
            if f.endswith(".json"):
                items.append(os.path.splitext(f)[0])
    seen = set(items)
    for item in KNOWN_ITEMS:
        if item not in seen:
            items.append(item)
            seen.add(item)
    return items
