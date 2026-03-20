#ifndef FB_PACK_H
#define FB_PACK_H

#include "types.h"

// ── Pack paths (relative to pack root) ──────────────────────────────────────
#define FB_TEXTURE_DIR   "assets/fruitbowl/textures/item"
#define FB_MODEL_DIR     "assets/fruitbowl/models/item"
#define FB_ITEMS_DIR     "assets/fruitbowl/items"
#define MC_ITEMS_DIR_REL "assets/minecraft/items"
#define MC_ATLAS_DIR     "assets/minecraft/atlases"
#define FB_MODEL_LIST    "model list.txt"

// ── Import a bbmodel into the pack ──────────────────────────────────────────
// Full pipeline: extract textures, build model JSON, create item def,
// update dispatch, update model list, sync helmets if needed.
bool fb_add_to_pack(const char *bbmodel_path, const char *pack_root,
                    const char *mc_item_id, const char *model_name,
                    const char *author, FBLog *log);

// ── Scan pack for all registered models ─────────────────────────────────────
int fb_scan_pack(const char *pack_root, FBPackEntry *entries, int max_entries);

// ── Delete a model from the pack ────────────────────────────────────────────
bool fb_delete_model(const char *pack_root, const char *mc_item_id,
                     const char *model_name, int threshold, FBLog *log);

// ── Duplicate a model to another item type ──────────────────────────────────
bool fb_duplicate_to_item(const char *pack_root, const char *model_name,
                          const char *target_item_id, const char *author,
                          FBLog *log);

// ── Update author in model list ─────────────────────────────────────────────
bool fb_update_author(const char *pack_root, const char *mc_item_id,
                      int threshold, const char *display_name,
                      const char *new_author, FBLog *log);

// ── Ensure atlas is configured ──────────────────────────────────────────────
void fb_ensure_atlas(const char *pack_root, FBLog *log);

// ── Sync helmet dispatch files from stone_button ────────────────────────────
void fb_sync_helmets(const char *pack_root, FBLog *log);

// ── Get heading name for an item ID ─────────────────────────────────────────
const char *fb_heading_for_item(const char *mc_item_id);

// ── Get block fallback model for an item ID (NULL if normal item) ───────────
const char *fb_block_fallback(const char *mc_item_id);

#endif // FB_PACK_H
