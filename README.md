# Fruitbowl Resource Pack Tool

A simple GUI tool for adding Blockbench (`.bbmodel`) models to the Fruitbowl Minecraft resource pack.

Does in one click what used to be 6 manual steps.

![Python](https://img.shields.io/badge/Python-3.10+-blue)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

## What It Does

1. **Extracts the texture** (PNG) from the `.bbmodel` file
2. **Converts the model** (geometry, UVs, rotations, display settings) to a resource pack JSON
3. **Creates the fruitbowl item definition** (`assets/fruitbowl/items/<name>.json`)
4. **Registers it** in the minecraft item's `custom_model_data` dispatch (`assets/minecraft/items/<item>.json`)

It also:
- Auto-assigns the next available `threshold` number
- Detects duplicates (won't add the same model twice)
- Sanitizes filenames (lowercase, no spaces/special chars)
- Remembers your pack folder between sessions

## Setup

**Requirements:** Python 3.10+ (tkinter is included with Python on Windows by default)

No extra packages needed — just standard library.

## Usage

1. Download or clone this repo
2. Double-click `fruitbowl_tool.py` (or run `python fruitbowl_tool.py`)
3. Select your resource pack folder (the one containing `pack.mcmeta`)
4. Select a `.bbmodel` file
5. Pick the Minecraft item type from the dropdown (e.g. `stone_button` for hats)
6. Click **Add to Pack**

The tool will tell you the threshold number to use in-game:
```
/trigger CustomModelData set <number>
```
Then `F3+T` to reload the pack.

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
