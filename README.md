# Fruitbowl Resource Pack Tool

A GUI tool for managing the Fruitbowl Minecraft resource pack — add, preview, and manage Blockbench (`.bbmodel`) models.

Does in one click what used to be 6 manual steps.

![C](https://img.shields.io/badge/C11-raylib-blue)
![Python](https://img.shields.io/badge/Python-3.10+-blue)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

## Two Versions

| | Python (`main` branch) | C (`c-rewrite` branch) |
|---|---|---|
| **GUI** | tkinter | raylib + raygui |
| **3D Preview** | ✗ | ✓ — textured model viewer |
| **Import** | Single + batch | Single (batch planned) |
| **Manage** | Scan, delete, duplicate, author edit | Scan, delete, duplicate, author edit |
| **Batch Mode** | ✓ — per-model authors | Planned |
| **Z-Fight Scanner** | ✓ | Planned |
| **Platform** | Windows (Python required) | Windows (standalone .exe) |

## What It Does

1. **Extracts textures** (PNG) from the `.bbmodel` file (supports multi-texture models)
2. **Converts the model** (geometry, UVs, rotations, display settings) to resource pack JSON
3. **Scales UVs** from pixel coordinates to Minecraft's 0–16 range
4. **Shifts bounds** to fit within Minecraft's -16 to 32 coordinate limits
5. **Creates the fruitbowl item definition** (`assets/fruitbowl/items/<name>.json`)
6. **Registers it** in the minecraft item's `custom_model_data` dispatch file
7. **Updates model list.txt** with author attribution
8. **Syncs helmet dispatch files** when stone_button models change
9. **Ensures atlas config** so textures are stitched correctly

### Manage Pack

- Scan all models from dispatch files with file integrity checks
- Search/filter by name, item type, author, or `noauthor` for missing authors
- Delete with shared file protection (textures/models used by multiple items aren't removed)
- Duplicate models to other item types
- Update author attribution

### 3D Preview (C version)

- Drag & drop `.bbmodel` files for instant textured preview
- Preview models already in the pack from the Manage tab
- Orbital camera: left-drag rotate, scroll zoom, middle-drag pan
- Correct Minecraft UV mapping and face winding

## Setup

### C Version (recommended)

**Build from source** (requires raylib):

```bash
cd c/
make          # Linux
cmake -B build && cmake --build build   # Windows (MSVC/MinGW)
```

Or grab a release binary from the releases page.

### Python Version

**Requirements:** Python 3.10+ (tkinter included with Python on Windows)

```bash
python run.py
```

No extra packages needed — just standard library.

## Usage

### Single Model Import

1. Select your resource pack folder (the one containing `pack.mcmeta`)
2. Select or drag & drop a `.bbmodel` file
3. Pick the Minecraft item type (e.g. `stone_button` for hats)
4. Click **Add to Pack**

### Managing the Pack

1. Switch to the **Manage** tab
2. Click **Scan Pack** to load all models from dispatch files
3. Use the search bar to filter (type `noauthor` to find missing attributions)
4. Select a model → **Delete**, **Preview**, **Duplicate**, or **Set Author**

### In-game

The output tells you the threshold number for each model:
```
/trigger CustomModelData set <number>
```
Then `F3+T` to reload the pack.

## Project Structure

```
fruitbowl-tool/
├── c/                           ← C rewrite (c-rewrite branch)
│   ├── src/
│   │   ├── main.c              ← Entry point, window init
│   │   ├── gui.c               ← Full GUI: tabs, import, manage, preview
│   │   ├── model.c             ← bbmodel parser, texture extraction, pack model loader
│   │   ├── pack.c              ← Pack operations: add, scan, delete, duplicate, author, atlas, helmets
│   │   ├── renderer.c          ← 3D model rendering with correct MC UV/face mapping
│   │   ├── util.c              ← File I/O, string helpers, logging
│   │   ├── filedialog.c        ← Native file dialogs via tinyfiledialogs
│   │   └── types.h             ← All type definitions, app state
│   ├── include/                ← Vendored: raylib.h, raygui.h, cJSON.h, tinyfiledialogs.h
│   ├── lib/                    ← cJSON.c, tinyfiledialogs.c
│   ├── Makefile                ← Linux build
│   └── CMakeLists.txt          ← Windows build
├── fruitbowl_tool/              ← Python version (main branch)
│   ├── core.py                 ← Add-to-pack logic
│   ├── manage.py               ← Scan/delete/duplicate logic
│   ├── gui.py                  ← tkinter GUI
│   ├── constants.py            ← Paths, known items, heading mappings
│   ├── settings.py             ← Settings persistence
│   ├── zfight.py               ← Z-fighting scanner
│   └── deploy.py               ← Server deployment (unused)
├── run.py                       ← Python entry point
└── README.md
```

## Supported Item Types

Auto-detects items in your pack, plus these defaults:

| Item | Used For |
|------|----------|
| `stone_button` | Hats / accessories (auto-syncs to helmets) |
| `diamond_sword` | Swords / weapons |
| `wooden_sword` | Wooden weapons |
| `bow` | Bows / ranged |
| `white_wool` | Dolls / plushies |
| `feather` | Handheld props |
| `elytra` | Wings |
| `shield` | Spell circles |
| `trident` | Tridents / spears |
| `carved_pumpkin` | Pumpkins |
| `bundle` | Baskets / containers |

## File Structure

After adding a model called `cool_hat` to `stone_button`:

```
assets/
├── fruitbowl/
│   ├── textures/item/cool_hat.png       ← texture extracted from bbmodel
│   ├── models/item/cool_hat.json        ← model converted from bbmodel
│   └── items/cool_hat.json              ← item definition
└── minecraft/
    ├── items/stone_button.json          ← threshold entry appended here
    └── atlases/blocks.json              ← directory source for fruitbowl textures
```

## Technical Notes

- **UV scaling**: bbmodel stores pixel coordinates (0–64 for 64×64 textures), Minecraft expects 0–16. Formula: `mc_uv = pixel × 16 / texture_size`.
- **Texture indices**: Blockbench `#0` refs use array position, not the texture's `id` field.
- **Atlas config**: `blocks.json` must have a `directory` source for `item/` to stitch fruitbowl namespace textures. Item textures must only be in the blocks atlas.
- **Bounds clamping**: Elements exceeding MC's -16 to 32 range are auto-shifted to fit.
- **Helmet sync**: Helmet dispatch files are copies of stone_button, auto-synced on any change.
- **Shared file protection**: Deleting a model from one item type won't remove files still referenced by other items.
- **Block fallbacks**: stone_button, white_wool, carved_pumpkin use `minecraft:block/*_inventory` fallbacks, not `minecraft:item/*`.

## Known Issues

- **Trident throwing orientation**: Custom trident models may not display correctly when thrown. The tool generates a `_throwing` variant with flipped display settings, but this doesn't fully match vanilla trident behavior.
