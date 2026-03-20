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
// heading_override: if non-NULL, used instead of fb_heading_for_item().
bool fb_add_to_pack(const char *bbmodel_path, const char *pack_root,
                    const char *mc_item_id, const char *model_name,
                    const char *author, const char *heading_override,
                    FBLog *log);

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

// ── Zip the resource pack ───────────────────────────────────────────────────
// Creates a .zip at output_path from pack_root, excluding .git, .zip, etc.
// If output_path is NULL, defaults to <pack_parent>/<zip_name>.
bool fb_zip_pack(const char *pack_root, const char *output_path,
                 FBLog *log);

// ── Scan pack for known item types (from MC items dir + known list) ─────────
int fb_scan_pack_items(const char *pack_root, char items[][FB_MAX_NAME],
                       int max_items);

// ── Check if a model already exists in the pack ─────────────────────────────
bool fb_check_existing(const char *pack_root, const char *model_name,
                       FBLog *log);

// ── Check if an item type needs a custom heading name ───────────────────────
// Returns true if the item has no built-in heading and no custom heading.
bool fb_needs_heading_name(const char *mc_item_id,
                           const FBCustomHeading *headings, int heading_count);

// ── Get custom heading for an item ID (or NULL) ─────────────────────────────
const char *fb_custom_heading_for(const char *mc_item_id,
                                  const FBCustomHeading *headings,
                                  int heading_count);

#endif // FB_PACK_H
