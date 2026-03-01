"""
Constants and configuration for the Fruitbowl Resource Pack Tool.
Paths relative to pack root, known item types, and heading mappings.
"""

import os

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

# ── Block-type items: fallback model is a block model, not item model ────────
# When creating a new dispatch file for these items, the fallback must reference
# the block model (e.g. minecraft:block/stone_button_inventory) instead of
# the default minecraft:item/<id> which doesn't exist for block items.
BLOCK_ITEM_FALLBACKS = {
    "stone_button":     "minecraft:block/stone_button_inventory",
    "white_wool":       "minecraft:block/white_wool",
    "carved_pumpkin":   "minecraft:block/carved_pumpkin",
}
