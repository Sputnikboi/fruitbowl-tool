#!/usr/bin/env python3
"""
Fruitbowl Resource Pack Tool
GUI tool for adding .bbmodel exports to the Fruitbowl Minecraft resource pack.
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

    # Build texture references
    textures = {}
    for tex in bbmodel.get("textures", []):
        tex_id = str(tex.get("id", "0"))
        textures[tex_id] = f"fruitbowl:item/{model_name}"
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
            face = {"uv": face_data["uv"]}
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
    Creates the file from scratch if it doesn't exist.
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

    # Check for duplicates
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


def add_to_pack(bbmodel_path: str, pack_root: str, mc_item_id: str,
                model_name: str | None = None) -> list[str]:
    """
    Full pipeline: extract from bbmodel and add to pack.
    Returns a list of log messages.
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

    # 1 — Texture
    png_data = extract_texture_png(bbmodel)
    os.makedirs(os.path.dirname(texture_path), exist_ok=True)
    with open(texture_path, "wb") as f:
        f.write(png_data)
    log.append(f"✓ Texture saved → textures/item/{model_name}.png")

    # 2 — Model JSON
    model_json = build_model_json(bbmodel, model_name)
    os.makedirs(os.path.dirname(model_path), exist_ok=True)
    with open(model_path, "w", encoding="utf-8") as f:
        json.dump(model_json, f, indent=2)
        f.write("\n")
    log.append(f"✓ Model saved → models/item/{model_name}.json")

    # 3 — Fruitbowl item JSON
    fb_item = build_fruitbowl_item_json(model_name)
    os.makedirs(os.path.dirname(fb_item_path), exist_ok=True)
    with open(fb_item_path, "w", encoding="utf-8") as f:
        json.dump(fb_item, f, indent=2)
        f.write("\n")
    log.append(f"✓ Item def saved → items/{model_name}.json")

    # 4 — Minecraft item dispatch
    os.makedirs(os.path.dirname(mc_item_path), exist_ok=True)
    threshold, was_duplicate = update_minecraft_item(mc_item_path, model_name, mc_item_id)
    if was_duplicate:
        log.append(f"⚠ Already in {mc_item_id}.json at threshold {threshold}")
    else:
        log.append(f"✓ Added to {mc_item_id}.json → threshold {threshold}")

    log.append("")
    log.append(f"In-game: hold {mc_item_id}, then run:")
    log.append(f"  /trigger CustomModelData set {threshold}")
    log.append(f"  (F3+T to reload pack)")

    return log


def scan_pack_items(pack_root: str) -> list[str]:
    """Scan the pack's minecraft/items dir to find available item types."""
    items_dir = os.path.join(pack_root, MC_ITEMS_DIR)
    items = []
    if os.path.isdir(items_dir):
        for f in sorted(os.listdir(items_dir)):
            if f.endswith(".json"):
                items.append(os.path.splitext(f)[0])
    # Merge with known items, keeping order
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
        self.item_type = tk.StringVar()
        self.available_items: list[str] = list(KNOWN_ITEMS)

        # ── Build UI ─────────────────────────────────────────────────────
        self._build_ui()

        # If we have a saved pack path, scan it
        if self.pack_path.get() and os.path.isdir(self.pack_path.get()):
            self._refresh_items()

    def _build_ui(self):
        pad = {"padx": 8, "pady": 4}
        wide_pad = {"padx": 8, "pady": (12, 4)}

        main = ttk.Frame(self.root, padding=12)
        main.grid(sticky="nsew")

        # ── Pack folder ──────────────────────────────────────────────────
        row = 0
        ttk.Label(main, text="Resource Pack Folder", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad
        )

        row += 1
        pack_entry = ttk.Entry(main, textvariable=self.pack_path, width=55)
        pack_entry.grid(row=row, column=0, columnspan=2, sticky="ew", **pad)
        ttk.Button(main, text="Browse…", width=10, command=self._browse_pack).grid(
            row=row, column=2, **pad
        )

        # ── BBModel file ────────────────────────────────────────────────
        row += 1
        ttk.Label(main, text="BBModel File", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad
        )

        row += 1
        ttk.Entry(main, textvariable=self.bbmodel_path, width=55).grid(
            row=row, column=0, columnspan=2, sticky="ew", **pad
        )
        ttk.Button(main, text="Browse…", width=10, command=self._browse_bbmodel).grid(
            row=row, column=2, **pad
        )

        # ── Model name ──────────────────────────────────────────────────
        row += 1
        ttk.Label(main, text="Model Name (leave blank to use filename)", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad
        )

        row += 1
        ttk.Entry(main, textvariable=self.model_name, width=55).grid(
            row=row, column=0, columnspan=2, sticky="ew", **pad
        )

        # ── Item type ───────────────────────────────────────────────────
        row += 1
        ttk.Label(main, text="Minecraft Item", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad
        )

        row += 1
        self.item_combo = ttk.Combobox(
            main, textvariable=self.item_type,
            values=self.available_items, width=52, state="normal"
        )
        self.item_combo.grid(row=row, column=0, columnspan=2, sticky="ew", **pad)

        # ── Buttons ─────────────────────────────────────────────────────
        row += 1
        btn_frame = ttk.Frame(main)
        btn_frame.grid(row=row, column=0, columnspan=3, pady=(16, 4))

        self.add_btn = ttk.Button(btn_frame, text="  Add to Pack  ", command=self._run)
        self.add_btn.pack(side="left", padx=4)

        # ── Log output ──────────────────────────────────────────────────
        row += 1
        ttk.Label(main, text="Output", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad
        )

        row += 1
        self.log_text = tk.Text(main, height=10, width=65, state="disabled",
                                bg="#1e1e1e", fg="#cccccc", font=("Consolas", 9),
                                relief="flat", borderwidth=1)
        self.log_text.grid(row=row, column=0, columnspan=3, **pad)

        # Tag configs for coloured log lines
        self.log_text.tag_configure("success", foreground="#6bcf6b")
        self.log_text.tag_configure("warn", foreground="#e8c85a")
        self.log_text.tag_configure("error", foreground="#e85a5a")
        self.log_text.tag_configure("info", foreground="#7ab0df")

    # ── Dialogs ──────────────────────────────────────────────────────────

    def _browse_pack(self):
        path = filedialog.askdirectory(title="Select resource pack folder")
        if path:
            # Validate it looks like a pack
            if not os.path.exists(os.path.join(path, "pack.mcmeta")):
                messagebox.showwarning(
                    "Not a resource pack",
                    "That folder doesn't contain a pack.mcmeta file.\n"
                    "Make sure you select the root of the resource pack."
                )
                return
            self.pack_path.set(path)
            self.settings["pack_path"] = path
            save_settings(self.settings)
            self._refresh_items()

    def _browse_bbmodel(self):
        path = filedialog.askopenfilename(
            title="Select .bbmodel file",
            filetypes=[("Blockbench Models", "*.bbmodel"), ("All Files", "*.*")]
        )
        if path:
            self.bbmodel_path.set(path)
            # Auto-fill model name from filename
            if not self.model_name.get():
                self.model_name.set(sanitize_name(os.path.basename(path)))

    def _refresh_items(self):
        pack = self.pack_path.get()
        if pack and os.path.isdir(pack):
            self.available_items = scan_pack_items(pack)
            self.item_combo["values"] = self.available_items

    # ── Logging ──────────────────────────────────────────────────────────

    def _log(self, msg: str, tag: str = "info"):
        self.log_text.configure(state="normal")
        self.log_text.insert("end", msg + "\n", tag)
        self.log_text.see("end")
        self.log_text.configure(state="disabled")

    def _log_clear(self):
        self.log_text.configure(state="normal")
        self.log_text.delete("1.0", "end")
        self.log_text.configure(state="disabled")

    # ── Run ──────────────────────────────────────────────────────────────

    def _run(self):
        self._log_clear()

        bbmodel = self.bbmodel_path.get().strip()
        pack = self.pack_path.get().strip()
        item = self.item_type.get().strip().lower().replace(" ", "_")
        name = self.model_name.get().strip() or None

        # Validate
        if not pack:
            self._log("ERROR: Select a resource pack folder first.", "error")
            return
        if not os.path.isdir(pack):
            self._log(f"ERROR: Pack folder not found: {pack}", "error")
            return
        if not bbmodel:
            self._log("ERROR: Select a .bbmodel file first.", "error")
            return
        if not os.path.isfile(bbmodel):
            self._log(f"ERROR: File not found: {bbmodel}", "error")
            return
        if not item:
            self._log("ERROR: Select a Minecraft item type.", "error")
            return

        display_name = sanitize_name(name if name else os.path.basename(bbmodel))
        self._log(f"Adding '{display_name}' to {item}...", "info")
        self._log("")

        try:
            messages = add_to_pack(bbmodel, pack, item, name)
            for msg in messages:
                if msg.startswith("✓"):
                    self._log(msg, "success")
                elif msg.startswith("⚠"):
                    self._log(msg, "warn")
                else:
                    self._log(msg, "info")
        except Exception as e:
            self._log(f"ERROR: {e}", "error")


def main():
    root = tk.Tk()

    # Try to set a Windows-native look
    try:
        root.tk.call("tk", "windowingsystem")  # just to check
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
