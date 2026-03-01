#!/usr/bin/env python3
"""
Fruitbowl Resource Pack Tool
GUI tool for adding .bbmodel exports to the Fruitbowl Minecraft resource pack.
Supports single-file and batch mode with per-model author tracking.
"""

import base64
import json
import os
import re
import sys
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

# ── Paths relative to pack root ──────────────────────────────────────────────
TEXTURE_DIR  = os.path.join("assets", "fruitbowl", "textures", "item")
MODEL_DIR    = os.path.join("assets", "fruitbowl", "models", "item")
FB_ITEMS_DIR = os.path.join("assets", "fruitbowl", "items")
MC_ITEMS_DIR = os.path.join("assets", "minecraft", "items")
ATLAS_DIR    = os.path.join("assets", "minecraft", "atlases")
MODEL_LIST   = "model list.txt"

# ── Known item types for the dropdown ────────────────────────────────────────
KNOWN_ITEMS = [
    "stone_button",
    "diamond_sword",
    "netherite_sword",
    "wooden_sword",
    "bow",
    "totem_of_undying",
    "goat_horn",
    "apple",
    "cooked_beef",
    "writable_book",
    "diamond_shovel",
    "diamond_axe",
    "diamond_pickaxe",
    "cookie",
    "shield",
    "trident",
    "stick",
    "elytra",
    "baked_potato",
    "golden_carrot",
    "feather",
    "carved_pumpkin",
    "bundle",
    "white_wool",
    "milk_bucket",
    "snowball",
    "salmon",
]

# ── Map from MC item IDs to model list.txt heading names ─────────────────────
ITEM_TO_HEADING = {
    "stone_button":     "Stone Button",
    "diamond_sword":    "Diamond/Netherite Sword",
    "netherite_sword":  "Diamond/Netherite Sword",
    "wooden_sword":     "Wooden Sword",
    "bow":              "Bow",
    "totem_of_undying":  "Totem",
    "goat_horn":        "Goat Horn",
    "apple":            "Apple",
    "cooked_beef":      "Steak",
    "writable_book":    "Book and Quill",
    "diamond_shovel":   "Shovel",
    "diamond_axe":      "Axe",
    "diamond_pickaxe":  "Pickaxe",
    "cookie":           "Cookie",
    "shield":           "Shield",
    "trident":          "Trident",
    "stick":            "Stick",
    "elytra":           "Elytra",
    "baked_potato":     "Baked Potato",
    "golden_carrot":    "Golden Carrot",
    "feather":          "Feather",
    "carved_pumpkin":   "Carved Pumpkin",
    "bundle":           "Bundle",
    "white_wool":       "White Wool",
    "milk_bucket":      "Milk Bucket",
    "snowball":         "Snowball",
    "salmon":           "Salmon",
}


# ═════════════════════════════════════════════════════════════════════════════
# Core logic (no GUI dependency)
# ═════════════════════════════════════════════════════════════════════════════

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


def extract_texture_png(bbmodel: dict) -> bytes:
    """Pull the first embedded texture's base64 PNG data."""
    textures = bbmodel.get("textures", [])
    if not textures:
        raise ValueError("No textures found in .bbmodel file.")
    source = textures[0].get("source", "")
    if not source.startswith("data:image/png;base64,"):
        raise ValueError("First texture is not an embedded base64 PNG.")
    b64 = source.split(",", 1)[1]
    return base64.b64decode(b64)


def build_model_json(bbmodel: dict, model_name: str) -> dict:
    """Convert .bbmodel → Minecraft resource pack model JSON."""
    res = bbmodel.get("resolution", {"width": 16, "height": 16})
    texture_size = [res["width"], res["height"]]

    # UV scaling: bbmodel stores pixel coordinates, MC expects 0-16 based coords
    uv_scale_x = 16.0 / res["width"]
    uv_scale_y = 16.0 / res["height"]

    # Build texture references — faces use the array index, not the id field
    textures = {}
    for idx, tex in enumerate(bbmodel.get("textures", [])):
        textures[str(idx)] = f"fruitbowl:item/{model_name}"
        if tex.get("particle", False):
            textures["particle"] = f"fruitbowl:item/{model_name}"

    # Convert elements
    elements = []
    for el in bbmodel.get("elements", []):
        entry = {
            "from": el["from"],
            "to": el["to"],
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
        next_threshold = max(e["threshold"] for e in entries) + 1
    else:
        data = {
            "model": {
                "type": "minecraft:range_dispatch",
                "property": "minecraft:custom_model_data",
                "index": 0,
                "fallback": {
                    "type": "minecraft:model",
                    "model": f"minecraft:item/{mc_item_id}",
                },
                "entries": [],
            }
        }
        entries = data["model"]["entries"]
        next_threshold = 1

    for e in entries:
        if e["model"].get("model") == f"fruitbowl:item/{model_name}":
            return e["threshold"], True

    entries.append({
        "model": {
            "type": "minecraft:model",
            "model": f"fruitbowl:item/{model_name}",
        },
        "threshold": next_threshold,
    })

    with open(mc_item_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
        f.write("\n")

    return next_threshold, False


def ensure_atlas(pack_root: str) -> list[tuple[str, str]]:
    """Ensure the blocks.json atlas includes an item/ directory source."""
    log = []
    atlas_path = os.path.join(pack_root, ATLAS_DIR, "blocks.json")
    os.makedirs(os.path.dirname(atlas_path), exist_ok=True)

    if os.path.exists(atlas_path):
        with open(atlas_path, "r", encoding="utf-8") as f:
            atlas = json.load(f)
    else:
        atlas = {"sources": []}

    has_item_dir = any(
        s.get("source") == "item" and s.get("prefix") == "item/"
        for s in atlas.get("sources", [])
    )

    if not has_item_dir:
        atlas["sources"].append({
            "type": "minecraft:directory",
            "source": "item",
            "prefix": "item/"
        })
        with open(atlas_path, "w", encoding="utf-8") as f:
            json.dump(atlas, f, indent=2)
            f.write("\n")
        log.append(("success", "✓ Fixed atlas — added item/ directory source to blocks.json"))

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
        # Check if this threshold already has an entry under this heading
        # Find the range of lines belonging to this section
        section_end = len(lines)
        for i in range(heading_idx + 1, len(lines)):
            # A non-empty line that doesn't match "N = ..." is a new heading
            stripped = lines[i].strip()
            if stripped and not re.match(r"^\d+\s*=", stripped):
                section_end = i
                break

        # Check for existing entry with same threshold
        for i in range(heading_idx + 1, section_end):
            if re.match(rf"^{threshold}\s*=", lines[i].strip()):
                # Replace existing line
                lines[i] = entry_text
                with open(list_path, "w", encoding="utf-8") as f:
                    f.write("\n".join(lines) + "\n")
                return ("warn", f"⚠ Updated existing entry in model list: {entry_text}")

        # Append after last entry in section
        insert_at = section_end
        # Back up past blank lines to insert right after last real entry
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
    # Also check if a fallback heading already exists in the file
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


# ═════════════════════════════════════════════════════════════════════════════
# Settings persistence
# ═════════════════════════════════════════════════════════════════════════════

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


# ═════════════════════════════════════════════════════════════════════════════
# GUI
# ═════════════════════════════════════════════════════════════════════════════

class FruitbowlApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Fruitbowl Resource Pack Tool")
        self.root.resizable(False, False)

        self.settings = load_settings()

        # ── Variables ────────────────────────────────────────────────────
        self.bbmodel_path = tk.StringVar()
        self.pack_path = tk.StringVar(value=self.settings.get("pack_path", ""))
        self.model_name = tk.StringVar()
        self.author_name = tk.StringVar()
        self.item_type = tk.StringVar()
        self.available_items: list[str] = list(KNOWN_ITEMS)

        # Batch state: list of {"path": str, "author": str}
        self.batch_entries: list[dict] = []

        # ── Build UI ─────────────────────────────────────────────────────
        self._build_ui()

        if self.pack_path.get() and os.path.isdir(self.pack_path.get()):
            self._refresh_items()

    def _build_ui(self):
        pad = {"padx": 8, "pady": 4}
        wide_pad = {"padx": 8, "pady": (12, 4)}

        # ── Notebook (tabs) ──────────────────────────────────────────────
        self.notebook = ttk.Notebook(self.root)
        self.notebook.grid(sticky="nsew", padx=8, pady=8)

        self.single_tab = ttk.Frame(self.notebook, padding=12)
        self.batch_tab = ttk.Frame(self.notebook, padding=12)
        self.notebook.add(self.single_tab, text="  Single Model  ")
        self.notebook.add(self.batch_tab, text="  Batch Mode  ")

        # ══════════════════════════════════════════════════════════════════
        # SINGLE TAB
        # ══════════════════════════════════════════════════════════════════
        main = self.single_tab

        row = 0
        ttk.Label(main, text="Resource Pack Folder", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        ttk.Entry(main, textvariable=self.pack_path, width=55).grid(
            row=row, column=0, columnspan=2, sticky="ew", **pad)
        ttk.Button(main, text="Browse…", width=10, command=self._browse_pack).grid(
            row=row, column=2, **pad)

        row += 1
        ttk.Label(main, text="BBModel File", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        ttk.Entry(main, textvariable=self.bbmodel_path, width=55).grid(
            row=row, column=0, columnspan=2, sticky="ew", **pad)
        ttk.Button(main, text="Browse…", width=10, command=self._browse_bbmodel).grid(
            row=row, column=2, **pad)

        row += 1
        ttk.Label(main, text="Model Name (leave blank to use filename)", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        ttk.Entry(main, textvariable=self.model_name, width=55).grid(
            row=row, column=0, columnspan=2, sticky="ew", **pad)

        row += 1
        ttk.Label(main, text="Author (optional)", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        ttk.Entry(main, textvariable=self.author_name, width=55).grid(
            row=row, column=0, columnspan=2, sticky="ew", **pad)

        row += 1
        ttk.Label(main, text="Minecraft Item", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        self.item_combo = ttk.Combobox(
            main, textvariable=self.item_type,
            values=self.available_items, width=52, state="normal")
        self.item_combo.grid(row=row, column=0, columnspan=2, sticky="ew", **pad)

        row += 1
        btn_frame = ttk.Frame(main)
        btn_frame.grid(row=row, column=0, columnspan=3, pady=(16, 4))
        ttk.Button(btn_frame, text="  Add to Pack  ", command=self._run_single).pack(side="left", padx=4)

        row += 1
        ttk.Label(main, text="Output", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        self.log_text = tk.Text(main, height=10, width=65, state="disabled",
                                bg="#1e1e1e", fg="#cccccc", font=("Consolas", 9),
                                relief="flat", borderwidth=1)
        self.log_text.grid(row=row, column=0, columnspan=3, **pad)
        self._setup_log_tags(self.log_text)

        # ══════════════════════════════════════════════════════════════════
        # BATCH TAB
        # ══════════════════════════════════════════════════════════════════
        batch = self.batch_tab

        row = 0
        ttk.Label(batch, text="Resource Pack Folder", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        ttk.Entry(batch, textvariable=self.pack_path, width=55).grid(
            row=row, column=0, columnspan=2, sticky="ew", **pad)
        ttk.Button(batch, text="Browse…", width=10, command=self._browse_pack).grid(
            row=row, column=2, **pad)

        row += 1
        ttk.Label(batch, text="Minecraft Item (applied to all files in batch)", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        self.batch_item_combo = ttk.Combobox(
            batch, textvariable=self.item_type,
            values=self.available_items, width=52, state="normal")
        self.batch_item_combo.grid(row=row, column=0, columnspan=2, sticky="ew", **pad)

        row += 1
        ttk.Label(batch, text="BBModel Files  (double-click Author column to edit)",
                   font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)

        # ── Treeview for batch files with author column ──────────────────
        row += 1
        tree_frame = ttk.Frame(batch)
        tree_frame.grid(row=row, column=0, columnspan=2, sticky="nsew", **pad)

        self.batch_tree = ttk.Treeview(
            tree_frame, columns=("name", "author"), show="headings",
            height=9, selectmode="extended")
        self.batch_tree.heading("name", text="File → Model Name")
        self.batch_tree.heading("author", text="Author")
        self.batch_tree.column("name", width=330, minwidth=200)
        self.batch_tree.column("author", width=130, minwidth=80)

        tree_scroll = ttk.Scrollbar(tree_frame, orient="vertical", command=self.batch_tree.yview)
        self.batch_tree.configure(yscrollcommand=tree_scroll.set)
        self.batch_tree.pack(side="left", fill="both", expand=True)
        tree_scroll.pack(side="right", fill="y")

        # Double-click to edit author
        self.batch_tree.bind("<Double-1>", self._batch_edit_author)

        btn_col = ttk.Frame(batch)
        btn_col.grid(row=row, column=2, sticky="n", **pad)
        ttk.Button(btn_col, text="Add Files…", width=12, command=self._batch_add_files).pack(pady=2)
        ttk.Button(btn_col, text="Add Folder…", width=12, command=self._batch_add_folder).pack(pady=2)
        ttk.Button(btn_col, text="Set Author…", width=12, command=self._batch_set_author_selected).pack(pady=(10, 2))
        ttk.Button(btn_col, text="Remove", width=12, command=self._batch_remove).pack(pady=(10, 2))
        ttk.Button(btn_col, text="Clear All", width=12, command=self._batch_clear).pack(pady=2)

        row += 1
        self.batch_count_label = ttk.Label(batch, text="0 files loaded", font=("", 8))
        self.batch_count_label.grid(row=row, column=0, columnspan=2, sticky="w", **pad)

        row += 1
        batch_btn_frame = ttk.Frame(batch)
        batch_btn_frame.grid(row=row, column=0, columnspan=3, pady=(12, 4))
        ttk.Button(batch_btn_frame, text="  Add All to Pack  ", command=self._run_batch).pack(side="left", padx=4)

        row += 1
        ttk.Label(batch, text="Output", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        self.batch_log = tk.Text(batch, height=10, width=65, state="disabled",
                                  bg="#1e1e1e", fg="#cccccc", font=("Consolas", 9),
                                  relief="flat", borderwidth=1)
        self.batch_log.grid(row=row, column=0, columnspan=3, **pad)
        self._setup_log_tags(self.batch_log)

    def _setup_log_tags(self, widget: tk.Text):
        widget.tag_configure("success", foreground="#6bcf6b")
        widget.tag_configure("warn", foreground="#e8c85a")
        widget.tag_configure("error", foreground="#e85a5a")
        widget.tag_configure("info", foreground="#7ab0df")
        widget.tag_configure("header", foreground="#ffffff", font=("Consolas", 9, "bold"))

    # ── Dialogs ──────────────────────────────────────────────────────────

    def _browse_pack(self):
        path = filedialog.askdirectory(title="Select resource pack folder")
        if path:
            if not os.path.exists(os.path.join(path, "pack.mcmeta")):
                messagebox.showwarning(
                    "Not a resource pack",
                    "That folder doesn't contain a pack.mcmeta file.\n"
                    "Make sure you select the root of the resource pack.")
                return
            self.pack_path.set(path)
            self.settings["pack_path"] = path
            save_settings(self.settings)
            self._refresh_items()

    def _browse_bbmodel(self):
        path = filedialog.askopenfilename(
            title="Select .bbmodel file",
            filetypes=[("Blockbench Models", "*.bbmodel"), ("All Files", "*.*")])
        if path:
            self.bbmodel_path.set(path)
            if not self.model_name.get():
                self.model_name.set(sanitize_name(os.path.basename(path)))

    def _refresh_items(self):
        pack = self.pack_path.get()
        if pack and os.path.isdir(pack):
            self.available_items = scan_pack_items(pack)
            self.item_combo["values"] = self.available_items
            self.batch_item_combo["values"] = self.available_items

    # ── Batch list management ────────────────────────────────────────────

    def _batch_add_files(self):
        paths = filedialog.askopenfilenames(
            title="Select .bbmodel files",
            filetypes=[("Blockbench Models", "*.bbmodel"), ("All Files", "*.*")])
        if paths:
            existing_paths = {e["path"] for e in self.batch_entries}
            for p in paths:
                if p not in existing_paths:
                    self.batch_entries.append({"path": p, "author": ""})
            self._batch_refresh_tree()

    def _batch_add_folder(self):
        folder = filedialog.askdirectory(title="Select folder containing .bbmodel files")
        if folder:
            existing_paths = {e["path"] for e in self.batch_entries}
            for f in sorted(os.listdir(folder)):
                if f.lower().endswith(".bbmodel"):
                    full = os.path.join(folder, f)
                    if full not in existing_paths:
                        self.batch_entries.append({"path": full, "author": ""})
            self._batch_refresh_tree()

    def _batch_remove(self):
        selected = self.batch_tree.selection()
        if not selected:
            return
        # Get indices to remove (iid is the string index we set)
        indices = sorted([int(iid) for iid in selected], reverse=True)
        for i in indices:
            self.batch_entries.pop(i)
        self._batch_refresh_tree()

    def _batch_clear(self):
        self.batch_entries.clear()
        self._batch_refresh_tree()

    def _batch_set_author_selected(self):
        """Set author for all selected rows via a dialog."""
        selected = self.batch_tree.selection()
        if not selected:
            messagebox.showinfo("No selection", "Select one or more rows first.")
            return

        # Simple dialog
        dialog = tk.Toplevel(self.root)
        dialog.title("Set Author")
        dialog.resizable(False, False)
        dialog.grab_set()

        ttk.Label(dialog, text=f"Author for {len(selected)} selected model(s):",
                  padding=(12, 12, 12, 4)).pack()
        author_var = tk.StringVar()
        entry = ttk.Entry(dialog, textvariable=author_var, width=30)
        entry.pack(padx=12, pady=4)
        entry.focus_set()

        def apply():
            author = author_var.get().strip()
            for iid in selected:
                idx = int(iid)
                self.batch_entries[idx]["author"] = author
            self._batch_refresh_tree()
            dialog.destroy()

        entry.bind("<Return>", lambda e: apply())
        btn_frame = ttk.Frame(dialog)
        btn_frame.pack(pady=(8, 12))
        ttk.Button(btn_frame, text="Apply", command=apply).pack(side="left", padx=4)
        ttk.Button(btn_frame, text="Cancel", command=dialog.destroy).pack(side="left", padx=4)

    def _batch_edit_author(self, event):
        """Double-click on a row to edit its author inline."""
        region = self.batch_tree.identify("region", event.x, event.y)
        if region != "cell":
            return
        column = self.batch_tree.identify_column(event.x)
        if column != "#2":  # Only edit the Author column
            return
        iid = self.batch_tree.identify_row(event.y)
        if not iid:
            return

        idx = int(iid)
        bbox = self.batch_tree.bbox(iid, column)
        if not bbox:
            return

        # Create inline entry widget
        current_val = self.batch_entries[idx]["author"]
        entry_var = tk.StringVar(value=current_val)
        entry = ttk.Entry(self.batch_tree, textvariable=entry_var, width=15)
        entry.place(x=bbox[0], y=bbox[1], width=bbox[2], height=bbox[3])
        entry.focus_set()
        entry.select_range(0, "end")

        def finish(event=None):
            self.batch_entries[idx]["author"] = entry_var.get().strip()
            entry.destroy()
            self._batch_refresh_tree()

        entry.bind("<Return>", finish)
        entry.bind("<FocusOut>", finish)
        entry.bind("<Escape>", lambda e: entry.destroy())

    def _batch_refresh_tree(self):
        self.batch_tree.delete(*self.batch_tree.get_children())
        for i, entry in enumerate(self.batch_entries):
            basename = os.path.basename(entry["path"])
            model_name = sanitize_name(basename)
            display = f"{basename}  →  {model_name}"
            self.batch_tree.insert("", "end", iid=str(i),
                                    values=(display, entry["author"]))
        count = len(self.batch_entries)
        self.batch_count_label.configure(text=f"{count} file{'s' if count != 1 else ''} loaded")

    # ── Logging helpers ──────────────────────────────────────────────────

    def _log_to(self, widget: tk.Text, msg: str, tag: str = "info"):
        widget.configure(state="normal")
        widget.insert("end", msg + "\n", tag)
        widget.see("end")
        widget.configure(state="disabled")

    def _log_clear_widget(self, widget: tk.Text):
        widget.configure(state="normal")
        widget.delete("1.0", "end")
        widget.configure(state="disabled")

    # ── Single mode ──────────────────────────────────────────────────────

    def _run_single(self):
        self._log_clear_widget(self.log_text)

        bbmodel = self.bbmodel_path.get().strip()
        pack = self.pack_path.get().strip()
        item = self.item_type.get().strip().lower().replace(" ", "_")
        name = self.model_name.get().strip() or None
        author = self.author_name.get().strip()

        if not pack:
            self._log_to(self.log_text, "ERROR: Select a resource pack folder first.", "error")
            return
        if not os.path.isdir(pack):
            self._log_to(self.log_text, f"ERROR: Pack folder not found: {pack}", "error")
            return
        if not bbmodel:
            self._log_to(self.log_text, "ERROR: Select a .bbmodel file first.", "error")
            return
        if not os.path.isfile(bbmodel):
            self._log_to(self.log_text, f"ERROR: File not found: {bbmodel}", "error")
            return
        if not item:
            self._log_to(self.log_text, "ERROR: Select a Minecraft item type.", "error")
            return

        model_name = sanitize_name(name if name else os.path.basename(bbmodel))
        existing = check_existing(pack, model_name)
        if existing:
            msg = (
                f"Model '{model_name}' already exists in the pack.\n"
                f"The following files will be overwritten:\n\n"
                + "\n".join(f"  • {e}" for e in existing)
                + "\n\nContinue?")
            if not messagebox.askyesno("Overwrite existing model?", msg):
                self._log_to(self.log_text, "Cancelled by user.", "warn")
                return

        # Check if we need a custom heading name for model list.txt
        heading = ""
        if needs_heading_name(pack, item):
            heading = self._ask_heading_name(item)
            if heading is None:  # User cancelled
                self._log_to(self.log_text, "Cancelled by user.", "warn")
                return
            if heading:
                # Save for future use
                self.settings.setdefault("custom_headings", {})[item] = heading
                save_settings(self.settings)

        try:
            messages = add_to_pack(bbmodel, pack, item, name, author, heading)
            for tag, msg in messages:
                self._log_to(self.log_text, msg, tag)
        except Exception as e:
            self._log_to(self.log_text, f"ERROR: {e}", "error")

    def _ask_heading_name(self, item_id: str) -> str | None:
        """
        Prompt for a plain-text heading name for the model list.
        Returns the name, empty string to use default, or None if cancelled.
        """
        # Check if we already have a saved custom heading
        saved = self.settings.get("custom_headings", {}).get(item_id)
        if saved:
            return saved

        fallback = item_id.replace("_", " ").title()

        dialog = tk.Toplevel(self.root)
        dialog.title("New Item Type")
        dialog.resizable(False, False)
        dialog.grab_set()

        result = {"value": None}

        ttk.Label(dialog, text=f"'{item_id}' is new to the model list.",
                  padding=(12, 12, 12, 0)).pack()
        ttk.Label(dialog, text="Enter the heading name for model list.txt:",
                  padding=(12, 4, 12, 4)).pack()
        ttk.Label(dialog, text=f"(leave blank to use '{fallback}')",
                  font=("", 8), padding=(12, 0, 12, 4)).pack()

        name_var = tk.StringVar()
        entry = ttk.Entry(dialog, textvariable=name_var, width=35)
        entry.pack(padx=12, pady=4)
        entry.focus_set()

        def apply():
            result["value"] = name_var.get().strip()
            dialog.destroy()

        def cancel():
            result["value"] = None
            dialog.destroy()

        entry.bind("<Return>", lambda e: apply())
        entry.bind("<Escape>", lambda e: cancel())

        btn_frame = ttk.Frame(dialog)
        btn_frame.pack(pady=(8, 12))
        ttk.Button(btn_frame, text="OK", command=apply, width=8).pack(side="left", padx=4)
        ttk.Button(btn_frame, text="Cancel", command=cancel, width=8).pack(side="left", padx=4)

        dialog.wait_window()
        return result["value"]

    # ── Batch mode ───────────────────────────────────────────────────────

    def _run_batch(self):
        self._log_clear_widget(self.batch_log)

        pack = self.pack_path.get().strip()
        item = self.item_type.get().strip().lower().replace(" ", "_")

        if not pack:
            self._log_to(self.batch_log, "ERROR: Select a resource pack folder first.", "error")
            return
        if not os.path.isdir(pack):
            self._log_to(self.batch_log, f"ERROR: Pack folder not found: {pack}", "error")
            return
        if not item:
            self._log_to(self.batch_log, "ERROR: Select a Minecraft item type.", "error")
            return
        if not self.batch_entries:
            self._log_to(self.batch_log, "ERROR: No files loaded. Add some .bbmodel files first.", "error")
            return

        # Pre-scan for overwrites
        overwrites = []
        for entry in self.batch_entries:
            name = sanitize_name(os.path.basename(entry["path"]))
            existing = check_existing(pack, name)
            if existing:
                overwrites.append(name)

        if overwrites:
            msg = (
                f"{len(overwrites)} model(s) already exist and will be updated:\n\n"
                + "\n".join(f"  • {n}" for n in overwrites)
                + "\n\nContinue?")
            if not messagebox.askyesno("Overwrite existing models?", msg):
                self._log_to(self.batch_log, "Cancelled by user.", "warn")
                return

        # Check if we need a custom heading name
        heading = ""
        if needs_heading_name(pack, item):
            heading = self._ask_heading_name(item)
            if heading is None:
                self._log_to(self.batch_log, "Cancelled by user.", "warn")
                return
            if heading:
                self.settings.setdefault("custom_headings", {})[item] = heading
                save_settings(self.settings)

        total = len(self.batch_entries)
        success_count = 0
        update_count = 0
        fail_count = 0

        self._log_to(self.batch_log, f"Processing {total} file(s) → {item}", "header")
        self._log_to(self.batch_log, "─" * 50, "info")

        for i, entry in enumerate(self.batch_entries, 1):
            filepath = entry["path"]
            author = entry["author"]
            basename = os.path.basename(filepath)
            name = sanitize_name(basename)
            author_display = f"  by {author}" if author else ""
            self._log_to(self.batch_log, f"[{i}/{total}] {basename} → {name}{author_display}", "header")

            try:
                existing = check_existing(pack, name)
                is_update = bool(existing)
                messages = add_to_pack(filepath, pack, item, name, author, heading)
                for tag, msg in messages:
                    self._log_to(self.batch_log, f"  {msg}", tag)
                if is_update:
                    update_count += 1
                else:
                    success_count += 1
            except Exception as e:
                self._log_to(self.batch_log, f"  ERROR: {e}", "error")
                fail_count += 1

        self._log_to(self.batch_log, "", "info")
        self._log_to(self.batch_log, "─" * 50, "info")
        parts = []
        if success_count:
            parts.append(f"{success_count} added")
        if update_count:
            parts.append(f"{update_count} updated")
        if fail_count:
            parts.append(f"{fail_count} failed")
        summary = f"Done! {', '.join(parts)}."
        tag = "error" if fail_count else "success"
        self._log_to(self.batch_log, summary, tag)
        self._log_to(self.batch_log, "Reload pack in-game with F3+T", "info")


def main():
    root = tk.Tk()

    try:
        style = ttk.Style()
        if "vista" in style.theme_names():
            style.theme_use("vista")
        elif "clam" in style.theme_names():
            style.theme_use("clam")
    except Exception:
        pass

    FruitbowlApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
