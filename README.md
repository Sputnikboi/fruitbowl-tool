# Fruitbowl Resource Pack Tool

A simple GUI tool for managing the Fruitbowl Minecraft resource pack — add, update, and delete Blockbench (`.bbmodel`) models.

Does in one click what used to be 6 manual steps.

![Python](https://img.shields.io/badge/Python-3.10+-blue)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

## What It Does

1. **Extracts the texture** (PNG) from the `.bbmodel` file
2. **Converts the model** (geometry, UVs, rotations, display settings) to a resource pack JSON
3. **Creates the fruitbowl item definition** (`assets/fruitbowl/items/<name>.json`)
4. **Registers it** in the minecraft item's `custom_model_data` dispatch (`assets/minecraft/items/<item>.json`)
5. **Updates model list.txt** with author attribution

It also:
- Auto-assigns the next available `threshold` number
- Detects duplicates (won't double-register in the dispatch file)
- **Warns before overwriting** existing models with a confirmation dialog
- Sanitizes filenames (lowercase, no spaces/special chars)
- Remembers your pack folder between sessions
- **Manage Pack** tab to browse, search, update, or delete any model in the pack

## Setup

**Requirements:** Python 3.10+ (tkinter is included with Python on Windows by default)

No extra packages needed — just standard library.

## Usage

```
python run.py
```

### Single Model

1. Select your resource pack folder (the one containing `pack.mcmeta`)
2. Select a `.bbmodel` file
3. Pick the Minecraft item type from the dropdown (e.g. `stone_button` for hats)
4. Click **Add to Pack**

### Batch Mode

For adding multiple models at once:

1. Switch to the **Batch Mode** tab
2. Pick the Minecraft item type (applies to all files)
3. Click **Add Files…** or **Add Folder…** to load `.bbmodel` files
4. Set authors per-model (double-click) or in bulk (select + **Set Author…**)
5. Click **Add All to Pack**

### Manage Pack

Browse and manage all models currently in the resource pack:

1. Switch to the **Manage Pack** tab
2. Click **Scan Pack** to load all models from dispatch files
3. Use the search bar to filter by name, item type, or author
4. Click column headers to sort
5. Select models and click **Delete** to remove all associated files
6. Select a model and click **Update…** to re-import from a new `.bbmodel`

Models with missing files (texture, model JSON, or item def) are highlighted.

### In-game

The output tells you the threshold number for each model:
```
/trigger CustomModelData set <number>
```
Then `F3+T` to reload the pack.

## Project Structure

```
fruitbowl-tool/
├── run.py                       ← entry point
├── fruitbowl_tool/
│   ├── __init__.py
│   ├── constants.py             ← paths, known items, heading mappings
│   ├── settings.py              ← settings persistence
│   ├── core.py                  ← add-to-pack logic (no GUI)
│   ├── manage.py                ← scan/delete logic (no GUI)
│   └── gui.py                   ← tkinter GUI
└── README.md
```

## Supported Item Types

The dropdown auto-detects items already in your pack, plus these defaults:

| Item | Used For |
|------|----------|
| `stone_button` | Hats / accessories |
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
| *...and more* | |

## File Structure

After adding a model called `cool_hat` to `stone_button`:

```
assets/
├── fruitbowl/
│   ├── textures/item/cool_hat.png       ← texture extracted from bbmodel
│   ├── models/item/cool_hat.json        ← model converted from bbmodel
│   └── items/cool_hat.json              ← item definition (boilerplate)
└── minecraft/
    └── items/stone_button.json          ← threshold entry appended here
```
