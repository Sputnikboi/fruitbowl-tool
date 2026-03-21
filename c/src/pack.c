#include "pack.h"
#include "model.h"
#include "util.h"
#include "cJSON.h"
#include "miniz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>

// Round to 4 decimal places (matches Python's round(val, 4))
static double round4(double v) {
    return round(v * 10000.0) / 10000.0;
}
static cJSON *cJSON_CreateRoundedArray(const float *arr, int count) {
    cJSON *a = cJSON_CreateArray();
    for (int i = 0; i < count; i++)
        cJSON_AddItemToArray(a, cJSON_CreateNumber(round4((double)arr[i])));
    return a;
}

// ═════════════════════════════════════════════════════════════════════════════
// Constants / Lookups
// ═════════════════════════════════════════════════════════════════════════════

typedef struct { const char *item_id; const char *heading; } HeadingMap;
static const HeadingMap HEADINGS[] = {
    {"stone_button", "Stone Button"}, {"pale_oak_button", "Pale Oak Button"},
    {"diamond_sword", "Diamond/Netherite Sword"}, {"netherite_sword", "Diamond/Netherite Sword"},
    {"wooden_sword", "Wooden Sword"}, {"bow", "Bow"}, {"totem_of_undying", "Totem"},
    {"goat_horn", "Goat Horn"}, {"apple", "Apple"}, {"cooked_beef", "Steak"},
    {"writable_book", "Book and Quill"}, {"diamond_shovel", "Shovel"},
    {"diamond_axe", "Axe"}, {"diamond_pickaxe", "Pickaxe"}, {"cookie", "Cookie"},
    {"shield", "Shield"}, {"trident", "Trident"}, {"stick", "Stick"},
    {"elytra", "Elytra"}, {"baked_potato", "Baked Potato"},
    {"golden_carrot", "Golden Carrot"}, {"feather", "Feather"},
    {"carved_pumpkin", "Carved Pumpkin"}, {"bundle", "Bundle"},
    {"white_wool", "White Wool"}, {"milk_bucket", "Milk Bucket"},
    {"snowball", "Snowball"}, {"salmon", "Salmon"}, {"leather", "Leather"},
    {NULL, NULL},
};

typedef struct { const char *item_id; const char *fallback; } FallbackMap;
static const FallbackMap FALLBACKS[] = {
    {"stone_button", "minecraft:block/stone_button_inventory"},
    {"pale_oak_button", "minecraft:block/pale_oak_button_inventory"},
    {"white_wool", "minecraft:block/white_wool"},
    {"carved_pumpkin", "minecraft:block/carved_pumpkin"},
    {NULL, NULL},
};

static const char *HELMET_TYPES[] = {
    "leather_helmet", "chainmail_helmet", "iron_helmet",
    "golden_helmet", "diamond_helmet", "netherite_helmet", "turtle_helmet",
    NULL,
};

static const char *SYNCED_HELMETS[] = {
    "leather_helmet", "chainmail_helmet", "iron_helmet",
    "golden_helmet", "diamond_helmet", "netherite_helmet", "turtle_helmet",
    NULL,
};

const char *fb_heading_for_item(const char *mc_item_id) {
    for (const HeadingMap *h = HEADINGS; h->item_id; h++) {
        if (strcmp(h->item_id, mc_item_id) == 0) return h->heading;
    }
    return NULL;
}

const char *fb_block_fallback(const char *mc_item_id) {
    for (const FallbackMap *f = FALLBACKS; f->item_id; f++) {
        if (strcmp(f->item_id, mc_item_id) == 0) return f->fallback;
    }
    return NULL;
}

static bool is_synced_helmet(const char *item_id) {
    for (const char **h = SYNCED_HELMETS; *h; h++) {
        if (strcmp(*h, item_id) == 0) return true;
    }
    return false;
}

// ═════════════════════════════════════════════════════════════════════════════
// JSON helpers
// ═════════════════════════════════════════════════════════════════════════════

static cJSON *load_json_file(const char *path) {
    int len;
    char *str = fb_read_file(path, &len);
    if (!str) return NULL;
    cJSON *root = cJSON_Parse(str);
    free(str);
    return root;
}

static bool save_json_file(const char *path, cJSON *root) {
    char *str = cJSON_Print(root);
    if (!str) return false;
    bool ok = fb_write_file(path, str, (int)strlen(str));
    cJSON_free(str);
    return ok;
}

// Extract fruitbowl model name from a dispatch entry (handles simple + condition)
static const char *extract_model_ref(cJSON *entry_model) {
    // Simple: {"type":"minecraft:model","model":"fruitbowl:item/name"}
    cJSON *model = cJSON_GetObjectItem(entry_model, "model");
    if (model && cJSON_IsString(model)) {
        const char *val = model->valuestring;
        if (strncmp(val, "fruitbowl:item/", 15) == 0)
            return val + 15;
    }
    // Condition wrapper: check on_false then on_true
    cJSON *on_false = cJSON_GetObjectItem(entry_model, "on_false");
    if (on_false) {
        model = cJSON_GetObjectItem(on_false, "model");
        if (model && cJSON_IsString(model) && strncmp(model->valuestring, "fruitbowl:item/", 15) == 0)
            return model->valuestring + 15;
    }
    cJSON *on_true = cJSON_GetObjectItem(entry_model, "on_true");
    if (on_true) {
        model = cJSON_GetObjectItem(on_true, "model");
        if (model && cJSON_IsString(model) && strncmp(model->valuestring, "fruitbowl:item/", 15) == 0)
            return model->valuestring + 15;
    }
    return NULL;
}

// ═════════════════════════════════════════════════════════════════════════════
// Base64 decode (for texture extraction)
// ═════════════════════════════════════════════════════════════════════════════

static const unsigned char b64_tbl[256] = {
    ['A']=0,['B']=1,['C']=2,['D']=3,['E']=4,['F']=5,['G']=6,['H']=7,
    ['I']=8,['J']=9,['K']=10,['L']=11,['M']=12,['N']=13,['O']=14,['P']=15,
    ['Q']=16,['R']=17,['S']=18,['T']=19,['U']=20,['V']=21,['W']=22,['X']=23,
    ['Y']=24,['Z']=25,['a']=26,['b']=27,['c']=28,['d']=29,['e']=30,['f']=31,
    ['g']=32,['h']=33,['i']=34,['j']=35,['k']=36,['l']=37,['m']=38,['n']=39,
    ['o']=40,['p']=41,['q']=42,['r']=43,['s']=44,['t']=45,['u']=46,['v']=47,
    ['w']=48,['x']=49,['y']=50,['z']=51,['0']=52,['1']=53,['2']=54,['3']=55,
    ['4']=56,['5']=57,['6']=58,['7']=59,['8']=60,['9']=61,['+']=62,['/']=63,
};

static unsigned char *b64_decode_pack(const char *src, int *out_len) {
    int len = (int)strlen(src);
    int pad = (len > 0 && src[len-1] == '=') + (len > 1 && src[len-2] == '=');
    int out_size = (len * 3) / 4 - pad;
    unsigned char *out = malloc(out_size);
    if (!out) return NULL;
    int j = 0;
    for (int i = 0; i < len; i += 4) {
        unsigned int n = (b64_tbl[(unsigned char)src[i]] << 18) |
                         (b64_tbl[(unsigned char)src[i+1]] << 12) |
                         (b64_tbl[(unsigned char)src[i+2]] << 6) |
                          b64_tbl[(unsigned char)src[i+3]];
        if (j < out_size) out[j++] = (n >> 16) & 0xFF;
        if (j < out_size) out[j++] = (n >> 8) & 0xFF;
        if (j < out_size) out[j++] = n & 0xFF;
    }
    *out_len = out_size;
    return out;
}

// ═════════════════════════════════════════════════════════════════════════════
// Atlas
// ═════════════════════════════════════════════════════════════════════════════

void fb_ensure_atlas(const char *pack_root, FBLog *log) {
    char path[FB_MAX_PATH];
    fb_path_join(path, sizeof(path), pack_root, MC_ATLAS_DIR "/blocks.json");

    // Ensure directory exists
    char dir[FB_MAX_PATH];
    fb_path_join(dir, sizeof(dir), pack_root, MC_ATLAS_DIR);
    fb_ensure_dir(dir);

    cJSON *atlas = NULL;
    if (fb_file_exists(path)) {
        atlas = load_json_file(path);
    }
    if (!atlas) {
        atlas = cJSON_CreateObject();
        cJSON_AddArrayToObject(atlas, "sources");
    }

    cJSON *sources = cJSON_GetObjectItem(atlas, "sources");
    if (!sources) {
        sources = cJSON_AddArrayToObject(atlas, "sources");
    }

    // Check for existing item/ source
    bool has_item = false;
    cJSON *src;
    cJSON_ArrayForEach(src, sources) {
        cJSON *s = cJSON_GetObjectItem(src, "source");
        cJSON *p = cJSON_GetObjectItem(src, "prefix");
        if (s && p && strcmp(s->valuestring, "item") == 0 && strcmp(p->valuestring, "item/") == 0) {
            has_item = true;
            break;
        }
    }

    if (!has_item) {
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "type", "minecraft:directory");
        cJSON_AddStringToObject(entry, "source", "item");
        cJSON_AddStringToObject(entry, "prefix", "item/");
        cJSON_AddItemToArray(sources, entry);
        save_json_file(path, atlas);
        fb_log(log, FB_LOG_SUCCESS, "Fixed blocks atlas — added item/ source");
    }

    cJSON_Delete(atlas);

    // Also clean items.json: remove item/ from items atlas to prevent
    // "Multiple atlases" errors
    char items_atlas_path[FB_MAX_PATH];
    fb_path_join(items_atlas_path, sizeof(items_atlas_path), pack_root, MC_ATLAS_DIR "/items.json");
    if (fb_file_exists(items_atlas_path)) {
        cJSON *items_atlas = load_json_file(items_atlas_path);
        if (items_atlas) {
            cJSON *isources = cJSON_GetObjectItem(items_atlas, "sources");
            if (isources) {
                int size = cJSON_GetArraySize(isources);
                for (int i = size - 1; i >= 0; i--) {
                    cJSON *isrc = cJSON_GetArrayItem(isources, i);
                    cJSON *s = cJSON_GetObjectItem(isrc, "source");
                    if (s && cJSON_IsString(s) && strcmp(s->valuestring, "item") == 0) {
                        cJSON_DeleteItemFromArray(isources, i);
                        save_json_file(items_atlas_path, items_atlas);
                        fb_log(log, FB_LOG_SUCCESS, "Cleaned items atlas — removed item/ source");
                        break;
                    }
                }
            }
            cJSON_Delete(items_atlas);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Dispatch file (minecraft/items/<item>.json)
// ═════════════════════════════════════════════════════════════════════════════

// Returns threshold, sets *existed=true if already present
static int update_dispatch(const char *pack_root, const char *mc_item_id,
                           const char *model_name, bool *existed) {
    char path[FB_MAX_PATH];
    fb_path_join(path, sizeof(path), pack_root, MC_ITEMS_DIR_REL);
    char fullpath[FB_MAX_PATH];
    snprintf(fullpath, sizeof(fullpath), "%s/%s.json", path, mc_item_id);

    // Ensure directory
    fb_ensure_dir(path);

    cJSON *root = NULL;
    if (fb_file_exists(fullpath)) {
        root = load_json_file(fullpath);
    }

    if (!root) {
        // Create new dispatch
        root = cJSON_CreateObject();
        cJSON *model = cJSON_AddObjectToObject(root, "model");
        cJSON_AddStringToObject(model, "type", "minecraft:range_dispatch");
        cJSON_AddStringToObject(model, "property", "minecraft:custom_model_data");
        cJSON_AddNumberToObject(model, "index", 0);

        cJSON *fallback = cJSON_AddObjectToObject(model, "fallback");
        cJSON_AddStringToObject(fallback, "type", "minecraft:model");
        const char *fb = fb_block_fallback(mc_item_id);
        char fb_buf[128];
        if (!fb) {
            snprintf(fb_buf, sizeof(fb_buf), "minecraft:item/%s", mc_item_id);
            fb = fb_buf;
        }
        cJSON_AddStringToObject(fallback, "model", fb);
        cJSON_AddArrayToObject(model, "entries");
    }

    cJSON *entries = cJSON_GetObjectItem(cJSON_GetObjectItem(root, "model"), "entries");
    char ref[256];
    snprintf(ref, sizeof(ref), "fruitbowl:item/%s", model_name);

    // Check for existing entry
    int max_threshold = 0;
    cJSON *entry;
    cJSON_ArrayForEach(entry, entries) {
        cJSON *m = cJSON_GetObjectItem(entry, "model");
        int t = cJSON_GetObjectItem(entry, "threshold")->valueint;
        if (t > max_threshold) max_threshold = t;

        const char *existing = extract_model_ref(m);
        if (existing && strcmp(existing, model_name) == 0) {
            *existed = true;
            int result = t;
            cJSON_Delete(root);
            return result;
        }
    }

    // Add new entry
    int threshold = max_threshold + 1;
    *existed = false;

    cJSON *new_entry = cJSON_CreateObject();
    cJSON_AddNumberToObject(new_entry, "threshold", threshold);

    if (strcmp(mc_item_id, "trident") == 0) {
        cJSON *m = cJSON_AddObjectToObject(new_entry, "model");
        cJSON_AddStringToObject(m, "type", "minecraft:condition");
        cJSON_AddStringToObject(m, "property", "minecraft:using_item");
        cJSON *on_true = cJSON_AddObjectToObject(m, "on_true");
        cJSON_AddStringToObject(on_true, "type", "minecraft:model");
        char throwing_ref[256];
        snprintf(throwing_ref, sizeof(throwing_ref), "fruitbowl:item/%s_throwing", model_name);
        cJSON_AddStringToObject(on_true, "model", throwing_ref);
        cJSON *on_false = cJSON_AddObjectToObject(m, "on_false");
        cJSON_AddStringToObject(on_false, "type", "minecraft:model");
        cJSON_AddStringToObject(on_false, "model", ref);
    } else if (strcmp(mc_item_id, "bow") == 0) {
        cJSON *m = cJSON_AddObjectToObject(new_entry, "model");
        cJSON_AddStringToObject(m, "type", "minecraft:condition");
        cJSON_AddStringToObject(m, "property", "minecraft:using_item");
        cJSON *on_true = cJSON_AddObjectToObject(m, "on_true");
        cJSON_AddStringToObject(on_true, "type", "minecraft:model");
        cJSON_AddStringToObject(on_true, "model", ref);
        cJSON *on_false = cJSON_AddObjectToObject(m, "on_false");
        cJSON_AddStringToObject(on_false, "type", "minecraft:model");
        cJSON_AddStringToObject(on_false, "model", ref);
    } else {
        cJSON *m = cJSON_AddObjectToObject(new_entry, "model");
        cJSON_AddStringToObject(m, "type", "minecraft:model");
        cJSON_AddStringToObject(m, "model", ref);
    }

    cJSON_AddItemToArray(entries, new_entry);
    save_json_file(fullpath, root);
    cJSON_Delete(root);
    return threshold;
}

// Remove a model from a dispatch file. Returns true if removed.
static bool remove_from_dispatch(const char *pack_root, const char *mc_item_id,
                                 const char *model_name) {
    char fullpath[FB_MAX_PATH];
    snprintf(fullpath, sizeof(fullpath), "%s/%s/%s.json",
             pack_root, MC_ITEMS_DIR_REL, mc_item_id);

    cJSON *root = load_json_file(fullpath);
    if (!root) return false;

    cJSON *entries = cJSON_GetObjectItem(cJSON_GetObjectItem(root, "model"), "entries");
    if (!entries) { cJSON_Delete(root); return false; }

    int size = cJSON_GetArraySize(entries);
    for (int i = 0; i < size; i++) {
        cJSON *entry = cJSON_GetArrayItem(entries, i);
        cJSON *m = cJSON_GetObjectItem(entry, "model");
        const char *ref = extract_model_ref(m);
        if (ref && strcmp(ref, model_name) == 0) {
            cJSON_DeleteItemFromArray(entries, i);
            save_json_file(fullpath, root);
            cJSON_Delete(root);
            return true;
        }
    }

    cJSON_Delete(root);
    return false;
}

// Count dispatch references across all item files (excluding one)
static int count_dispatch_refs(const char *pack_root, const char *model_name,
                               const char *exclude_item) {
    char dir[FB_MAX_PATH];
    fb_path_join(dir, sizeof(dir), pack_root, MC_ITEMS_DIR_REL);

    DIR *d = opendir(dir);
    if (!d) return 0;

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(d))) {
        int len = (int)strlen(ent->d_name);
        if (len < 6 || strcmp(ent->d_name + len - 5, ".json") != 0) continue;

        char item_id[FB_MAX_NAME];
        strncpy(item_id, ent->d_name, len - 5);
        item_id[len - 5] = '\0';

        if (exclude_item && strcmp(item_id, exclude_item) == 0) continue;

        char fullpath[FB_MAX_PATH];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, ent->d_name);
        cJSON *root = load_json_file(fullpath);
        if (!root) continue;

        cJSON *entries = cJSON_GetObjectItem(cJSON_GetObjectItem(root, "model"), "entries");
        if (entries) {
            cJSON *entry;
            cJSON_ArrayForEach(entry, entries) {
                const char *ref = extract_model_ref(cJSON_GetObjectItem(entry, "model"));
                if (ref && strcmp(ref, model_name) == 0) {
                    count++;
                    break;
                }
            }
        }
        cJSON_Delete(root);
    }
    closedir(d);
    return count;
}

// ═════════════════════════════════════════════════════════════════════════════
// Model list.txt
// ═════════════════════════════════════════════════════════════════════════════

static void update_model_list(const char *pack_root, const char *mc_item_id,
                              const char *model_name, int threshold,
                              const char *author, const char *heading_override,
                              FBLog *log) {
    char path[FB_MAX_PATH];
    fb_path_join(path, sizeof(path), pack_root, FB_MODEL_LIST);

    char display[FB_MAX_NAME];
    fb_display_name(model_name, display, sizeof(display));

    const char *heading = heading_override;
    if (!heading || !heading[0]) heading = fb_heading_for_item(mc_item_id);
    char heading_buf[FB_MAX_NAME];
    if (!heading) {
        // Title-case the item id
        fb_display_name(mc_item_id, heading_buf, sizeof(heading_buf));
        heading = heading_buf;
    }

    char entry_line[256];
    if (author && author[0]) {
        snprintf(entry_line, sizeof(entry_line), "%d = %s (%s)", threshold, display, author);
    } else {
        snprintf(entry_line, sizeof(entry_line), "%d = %s", threshold, display);
    }

    // Read existing content
    int file_len = 0;
    char *content = fb_read_file(path, &file_len);

    // Build heading pattern
    char heading_pat[FB_MAX_NAME + 2];
    snprintf(heading_pat, sizeof(heading_pat), "%s:", heading);

    if (!content) {
        // Create new file
        char buf[512];
        snprintf(buf, sizeof(buf), "%s\n%s\n", heading_pat, entry_line);
        fb_write_file(path, buf, (int)strlen(buf));
        fb_log(log, FB_LOG_SUCCESS, "Model list -> %s", entry_line);
        return;
    }

    // Find heading
    char *heading_pos = strstr(content, heading_pat);
    if (heading_pos) {
        // Check it's at start of line
        if (heading_pos != content && *(heading_pos - 1) != '\n') {
            heading_pos = NULL;
        }
    }

    if (!heading_pos) {
        // Append new section — trim trailing newlines from existing content first
        int trimmed = file_len;
        while (trimmed > 0 && (content[trimmed-1] == '\n' || content[trimmed-1] == '\r'))
            trimmed--;
        content[trimmed] = '\0';
        int new_len = trimmed + (int)strlen(heading_pat) + (int)strlen(entry_line) + 8;
        char *new_content = malloc(new_len);
        snprintf(new_content, new_len, "%s\n\n\n%s\n%s\n", content, heading_pat, entry_line);
        fb_write_file(path, new_content, (int)strlen(new_content));
        free(new_content);
        free(content);
        fb_log(log, FB_LOG_SUCCESS, "Model list -> %s", entry_line);
        return;
    }

    // Find end of section (next heading or EOF) — needed for both replace and append
    char *section_end = heading_pos + strlen(heading_pat);
    while (*section_end) {
        if (*section_end == '\n') {
            char *next = section_end + 1;
            // Skip empty lines
            while (*next == '\n' || *next == '\r') next++;
            // Check if next non-empty line is a heading (no digit start)
            if (*next && !(*next >= '0' && *next <= '9') && *next != '\n' && *next != '\r') {
                break;
            }
            if (!*next) { section_end = next; break; }
        }
        section_end++;
    }

    // Check if threshold already exists in THIS section only — replace it
    char thresh_pat[32];
    snprintf(thresh_pat, sizeof(thresh_pat), "\n%d =", threshold);
    char *line_pos = NULL;
    {
        char *search = heading_pos;
        while ((search = strstr(search, thresh_pat)) != NULL) {
            if (search >= section_end) break; // past our section
            line_pos = search;
            break;
        }
    }
    if (line_pos) {
        // Find end of this line
        char *line_end = strchr(line_pos + 1, '\n');
        if (!line_end) line_end = content + file_len;

        // Replace the line
        int before = (int)(line_pos - content) + 1; // include the \n
        int after_start = (int)(line_end - content);
        int entry_len = (int)strlen(entry_line);
        int new_len = before + entry_len + (file_len - after_start) + 1;
        char *new_content = malloc(new_len);
        memcpy(new_content, content, before);
        memcpy(new_content + before, entry_line, entry_len);
        memcpy(new_content + before + entry_len, content + after_start, file_len - after_start);
        new_content[before + entry_len + (file_len - after_start)] = '\0';
        fb_write_file(path, new_content, (int)strlen(new_content));
        free(new_content);
        free(content);
        fb_log(log, FB_LOG_WARN, "Updated model list: %s", entry_line);
        return;
    }

    // Back up past trailing newlines, keep one
    while (section_end > heading_pos && (*(section_end-1) == '\n' || *(section_end-1) == '\r'))
        section_end--;
    section_end++; // keep one newline

    int insert_at = (int)(section_end - content);
    int entry_len = (int)strlen(entry_line);
    int new_len = file_len + entry_len + 1;
    char *new_content = malloc(new_len);
    memcpy(new_content, content, insert_at);
    memcpy(new_content + insert_at, entry_line, entry_len);
    memcpy(new_content + insert_at + entry_len, content + insert_at, file_len - insert_at);
    new_content[insert_at + entry_len + (file_len - insert_at)] = '\0';
    fb_write_file(path, new_content, (int)strlen(new_content));
    free(new_content);
    free(content);
    fb_log(log, FB_LOG_SUCCESS, "Model list -> %s", entry_line);
}

static void remove_from_model_list(const char *pack_root, const char *mc_item_id,
                                   int threshold, FBLog *log) {
    char path[FB_MAX_PATH];
    fb_path_join(path, sizeof(path), pack_root, FB_MODEL_LIST);

    int file_len;
    char *content = fb_read_file(path, &file_len);
    if (!content) { fb_log(log, FB_LOG_WARN, "model list.txt not found"); return; }

    const char *heading = fb_heading_for_item(mc_item_id);
    if (!heading) { free(content); return; }

    char thresh_pat[32];
    snprintf(thresh_pat, sizeof(thresh_pat), "%d =", threshold);

    // Find the line with this threshold
    char *pos = content;
    char *found = NULL;
    while ((pos = strstr(pos, thresh_pat))) {
        // Ensure it's at start of line
        if (pos == content || *(pos - 1) == '\n') {
            found = pos;
            break;
        }
        pos++;
    }

    if (!found) {
        fb_log(log, FB_LOG_WARN, "Threshold %d not found in model list", threshold);
        free(content);
        return;
    }

    // Find end of line
    char *line_end = strchr(found, '\n');
    int line_start = (int)(found - content);
    int line_len = line_end ? (int)(line_end - found + 1) : (file_len - line_start);

    // Remove the line
    memmove(found, found + line_len, file_len - line_start - line_len + 1);
    file_len -= line_len;
    fb_write_file(path, content, file_len);
    free(content);
    fb_log(log, FB_LOG_SUCCESS, "Removed threshold %d from model list", threshold);
}

// ═════════════════════════════════════════════════════════════════════════════
// Helmet sync
// ═════════════════════════════════════════════════════════════════════════════

void fb_sync_helmets(const char *pack_root, FBLog *log) {
    char sb_path[FB_MAX_PATH];
    snprintf(sb_path, sizeof(sb_path), "%s/%s/stone_button.json", pack_root, MC_ITEMS_DIR_REL);

    cJSON *sb = load_json_file(sb_path);
    if (!sb) { fb_log(log, FB_LOG_ERROR, "stone_button.json not found"); return; }

    int num_entries = cJSON_GetArraySize(
        cJSON_GetObjectItem(cJSON_GetObjectItem(sb, "model"), "entries"));

    for (const char **h = HELMET_TYPES; *h; h++) {
        cJSON *copy = cJSON_Duplicate(sb, 1);
        cJSON *fallback = cJSON_GetObjectItem(cJSON_GetObjectItem(copy, "model"), "fallback");
        cJSON_SetValuestring(cJSON_GetObjectItem(fallback, "model"),
                             TextFormat("minecraft:item/%s", *h));

        char out_path[FB_MAX_PATH];
        snprintf(out_path, sizeof(out_path), "%s/%s/%s.json", pack_root, MC_ITEMS_DIR_REL, *h);
        save_json_file(out_path, copy);
        cJSON_Delete(copy);
        fb_log(log, FB_LOG_SUCCESS, "%s.json — %d entries synced", *h, num_entries);
    }

    cJSON_Delete(sb);
}

// ═════════════════════════════════════════════════════════════════════════════
// Add to pack (full pipeline)
// ═════════════════════════════════════════════════════════════════════════════

bool fb_add_to_pack(const char *bbmodel_path, const char *pack_root,
                    const char *mc_item_id, const char *model_name,
                    const char *author, const char *heading_override,
                    FBLog *log) {
    // Parse bbmodel
    int file_len;
    char *json_str = fb_read_file(bbmodel_path, &file_len);
    if (!json_str) { fb_log(log, FB_LOG_ERROR, "Cannot read %s", bbmodel_path); return false; }

    cJSON *bb = cJSON_Parse(json_str);
    free(json_str);
    if (!bb) { fb_log(log, FB_LOG_ERROR, "Cannot parse bbmodel"); return false; }

    // Derive name
    char name[FB_MAX_NAME];
    if (model_name && model_name[0]) {
        fb_sanitize_name(model_name, name, sizeof(name));
    } else {
        fb_sanitize_name(bbmodel_path, name, sizeof(name));
    }

    fb_ensure_atlas(pack_root, log);

    // 1 — Extract and save textures
    cJSON *textures = cJSON_GetObjectItem(bb, "textures");
    int tex_count = textures ? cJSON_GetArraySize(textures) : 0;

    // Find which indices are used
    int primary_idx = 0;
    int usage[FB_MAX_TEXTURES] = {0};
    cJSON *elements = cJSON_GetObjectItem(bb, "elements");
    cJSON *el;
    cJSON_ArrayForEach(el, elements) {
        cJSON *faces = cJSON_GetObjectItem(el, "faces");
        cJSON *face;
        cJSON_ArrayForEach(face, faces) {
            cJSON *tex = cJSON_GetObjectItem(face, "texture");
            if (tex && cJSON_IsNumber(tex) && tex->valueint < FB_MAX_TEXTURES)
                usage[tex->valueint]++;
        }
    }
    for (int i = 1; i < FB_MAX_TEXTURES; i++) {
        if (usage[i] > usage[primary_idx]) primary_idx = i;
    }

    const char *b64_prefix = "data:image/png;base64,";
    int prefix_len = (int)strlen(b64_prefix);

    for (int i = 0; i < tex_count && i < FB_MAX_TEXTURES; i++) {
        if (usage[i] == 0 && i != primary_idx) continue;

        cJSON *tex = cJSON_GetArrayItem(textures, i);
        cJSON *source = cJSON_GetObjectItem(tex, "source");
        if (!source || !cJSON_IsString(source)) continue;
        if (strncmp(source->valuestring, b64_prefix, prefix_len) != 0) continue;

        int png_len;
        unsigned char *png = b64_decode_pack(source->valuestring + prefix_len, &png_len);
        if (!png) continue;

        char tex_path[FB_MAX_PATH];
        if (i == primary_idx) {
            snprintf(tex_path, sizeof(tex_path), "%s/%s/%s.png", pack_root, FB_TEXTURE_DIR, name);
        } else {
            snprintf(tex_path, sizeof(tex_path), "%s/%s/%s_%d.png", pack_root, FB_TEXTURE_DIR, name, i);
        }

        char tex_dir[FB_MAX_PATH];
        fb_path_join(tex_dir, sizeof(tex_dir), pack_root, FB_TEXTURE_DIR);
        fb_ensure_dir(tex_dir);
        fb_write_file(tex_path, (const char *)png, png_len);
        free(png);

        if (i == primary_idx)
            fb_log(log, FB_LOG_SUCCESS, "Texture -> %s.png", name);
        else
            fb_log(log, FB_LOG_SUCCESS, "Texture -> %s_%d.png", name, i);
    }

    // 2 — Build and save model JSON
    FBModel model;
    fb_parse_bbmodel(bbmodel_path, &model);

    // Build output JSON manually with correct texture refs
    cJSON *out = cJSON_CreateObject();
    cJSON_AddStringToObject(out, "credit", "Made with Blockbench");

    cJSON *res = cJSON_GetObjectItem(bb, "resolution");
    int tw = res ? cJSON_GetObjectItem(res, "width")->valueint : 16;
    int th = res ? cJSON_GetObjectItem(res, "height")->valueint : 16;
    if (tw != 16 || th != 16) {
        cJSON *ts = cJSON_AddArrayToObject(out, "texture_size");
        cJSON_AddItemToArray(ts, cJSON_CreateNumber(tw));
        cJSON_AddItemToArray(ts, cJSON_CreateNumber(th));
    }

    // Texture refs
    cJSON *tex_obj = cJSON_AddObjectToObject(out, "textures");
    for (int i = 0; i < FB_MAX_TEXTURES; i++) {
        if (usage[i] == 0 && i != primary_idx) continue;
        if (i >= tex_count) continue;
        char key[8]; snprintf(key, sizeof(key), "%d", i);
        if (i == primary_idx) {
            cJSON_AddStringToObject(tex_obj, key, TextFormat("fruitbowl:item/%s", name));
        } else {
            cJSON_AddStringToObject(tex_obj, key, TextFormat("fruitbowl:item/%s_%d", name, i));
        }
    }
    cJSON_AddStringToObject(tex_obj, "particle", TextFormat("fruitbowl:item/%s", name));

    // Elements — from the parsed model (already has UV scaling + bounds shift)
    cJSON *out_elements = cJSON_AddArrayToObject(out, "elements");
    for (int i = 0; i < model.element_count; i++) {
        FBElement *e = &model.elements[i];
        cJSON *ej = cJSON_CreateObject();

        cJSON *from_a = cJSON_CreateRoundedArray(e->from, 3);
        cJSON *to_a = cJSON_CreateRoundedArray(e->to, 3);
        cJSON_AddItemToObject(ej, "from", from_a);
        cJSON_AddItemToObject(ej, "to", to_a);

        if (e->rot_axis) {
            cJSON *rot = cJSON_AddObjectToObject(ej, "rotation");
            cJSON_AddNumberToObject(rot, "angle", round4(e->rot_angle));
            char axis_str[2] = {e->rot_axis, '\0'};
            cJSON_AddStringToObject(rot, "axis", axis_str);
            cJSON *orig = cJSON_CreateRoundedArray(e->rot_origin, 3);
            cJSON_AddItemToObject(rot, "origin", orig);
        }

        cJSON *faces_obj = cJSON_AddObjectToObject(ej, "faces");
        const char *face_names[] = {"north","east","south","west","up","down"};
        for (int fi = 0; fi < 6; fi++) {
            FBFace *f = &e->faces[fi];
            if (!f->has_texture) continue;
            cJSON *fj = cJSON_CreateObject();
            cJSON *uv = cJSON_CreateRoundedArray(f->uv, 4);
            cJSON_AddItemToObject(fj, "uv", uv);
            cJSON_AddStringToObject(fj, "texture", TextFormat("#%d", f->texture_idx));
            if (f->rotation) cJSON_AddNumberToObject(fj, "rotation", f->rotation);
            if (f->has_tintindex) cJSON_AddNumberToObject(fj, "tintindex", f->tintindex);
            cJSON_AddItemToObject(faces_obj, face_names[fi], fj);
        }

        if (!e->shade) cJSON_AddFalseToObject(ej, "shade");
        cJSON_AddItemToArray(out_elements, ej);
    }

    // Display
    cJSON *bb_display = cJSON_GetObjectItem(bb, "display");
    if (bb_display) {
        cJSON_AddItemToObject(out, "display", cJSON_Duplicate(bb_display, 1));
    }

    // Groups (preserve from bbmodel)
    if (model.has_groups && model.groups_json[0]) {
        cJSON *groups_parsed = cJSON_Parse(model.groups_json);
        if (groups_parsed) {
            cJSON_AddItemToObject(out, "groups", groups_parsed);
        }
    }

    // Check if model already exists (warn but don't block)
    fb_check_existing(pack_root, name, log);

    char model_path[FB_MAX_PATH];
    snprintf(model_path, sizeof(model_path), "%s/%s/%s.json", pack_root, FB_MODEL_DIR, name);
    char model_dir[FB_MAX_PATH];
    fb_path_join(model_dir, sizeof(model_dir), pack_root, FB_MODEL_DIR);
    fb_ensure_dir(model_dir);
    save_json_file(model_path, out);
    fb_log(log, FB_LOG_SUCCESS, "Model -> %s.json", name);
    cJSON_Delete(out);

    // 2b — Trident throwing variant
    if (strcmp(mc_item_id, "trident") == 0 && bb_display) {
        cJSON *throw_out = cJSON_CreateObject();
        cJSON_AddStringToObject(throw_out, "credit", "Made with Blockbench");
        // Copy texture_size, textures, elements from saved model
        char saved_model_path[FB_MAX_PATH];
        snprintf(saved_model_path, sizeof(saved_model_path), "%s/%s/%s.json",
                 pack_root, FB_MODEL_DIR, name);
        cJSON *saved = load_json_file(saved_model_path);
        if (saved) {
            // Copy everything from saved
            cJSON *child = saved->child;
            while (child) {
                if (strcmp(child->string, "display") != 0) {
                    cJSON_AddItemToObject(throw_out, child->string,
                                          cJSON_Duplicate(child, 1));
                }
                child = child->next;
            }
            // Build flipped display (180° rotation on Y for throwing)
            cJSON *throw_display = cJSON_Duplicate(bb_display, 1);
            // Flip thirdperson_right: add 180 to Y rotation
            const char *throw_views[] = {"thirdperson_right", "thirdperson_left",
                                         "firstperson_right", "firstperson_left", NULL};
            for (const char **v = throw_views; *v; v++) {
                cJSON *view = cJSON_GetObjectItem(throw_display, *v);
                if (view) {
                    cJSON *rot = cJSON_GetObjectItem(view, "rotation");
                    if (rot && cJSON_GetArraySize(rot) >= 3) {
                        cJSON *ry = cJSON_GetArrayItem(rot, 1);
                        if (ry) cJSON_SetNumberValue(ry, ry->valuedouble + 180.0);
                    }
                }
            }
            cJSON_AddItemToObject(throw_out, "display", throw_display);

            char throw_path[FB_MAX_PATH];
            snprintf(throw_path, sizeof(throw_path), "%s/%s/%s_throwing.json",
                     pack_root, FB_MODEL_DIR, name);
            save_json_file(throw_path, throw_out);
            fb_log(log, FB_LOG_SUCCESS, "Model -> %s_throwing.json", name);
            cJSON_Delete(saved);
        }
        cJSON_Delete(throw_out);
    }

    // 3 — Fruitbowl item def
    cJSON *item_def = cJSON_CreateObject();
    cJSON *item_model = cJSON_AddObjectToObject(item_def, "model");
    cJSON_AddStringToObject(item_model, "type", "minecraft:model");
    cJSON_AddStringToObject(item_model, "model", TextFormat("fruitbowl:item/%s", name));

    char item_path[FB_MAX_PATH];
    snprintf(item_path, sizeof(item_path), "%s/%s/%s.json", pack_root, FB_ITEMS_DIR, name);
    char items_dir[FB_MAX_PATH];
    fb_path_join(items_dir, sizeof(items_dir), pack_root, FB_ITEMS_DIR);
    fb_ensure_dir(items_dir);
    save_json_file(item_path, item_def);
    fb_log(log, FB_LOG_SUCCESS, "Item -> %s.json", name);
    cJSON_Delete(item_def);

    // 4 — Dispatch
    bool existed = false;
    int threshold = update_dispatch(pack_root, mc_item_id, name, &existed);
    if (existed)
        fb_log(log, FB_LOG_WARN, "Already in %s.json — threshold %d", mc_item_id, threshold);
    else
        fb_log(log, FB_LOG_SUCCESS, "Registered in %s.json -> threshold %d", mc_item_id, threshold);

    // 5 — Model list
    update_model_list(pack_root, mc_item_id, name, threshold, author,
                      heading_override, log);

    // 6 — Sync helmets
    if (strcmp(mc_item_id, "stone_button") == 0)
        fb_sync_helmets(pack_root, log);

    fb_log(log, FB_LOG_INFO, "/trigger CustomModelData set %d", threshold);

    cJSON_Delete(bb);
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// Scan pack
// ═════════════════════════════════════════════════════════════════════════════

// Parse model list.txt to build author lookup: key="heading|threshold" -> author string
// Stores into a flat array of {heading, threshold, author} for lookup
typedef struct { char heading[FB_MAX_NAME]; int threshold; char author[FB_MAX_NAME]; } AuthorEntry;
#define MAX_AUTHOR_ENTRIES 1024

static int parse_authors(const char *pack_root, AuthorEntry *out, int max) {
    char path[FB_MAX_PATH];
    fb_path_join(path, sizeof(path), pack_root, FB_MODEL_LIST);
    int file_len;
    char *content = fb_read_file(path, &file_len);
    if (!content) return 0;

    int count = 0;
    char current_heading[FB_MAX_NAME] = {0};
    char *line = strtok(content, "\n");
    while (line && count < max) {
        // Trim CR
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\r' || line[len-1] == ' ')) line[--len] = '\0';

        if (len == 0) { line = strtok(NULL, "\n"); continue; }

        // Check if heading (ends with : and doesn't start with digit)
        if (line[len-1] == ':' && !(line[0] >= '0' && line[0] <= '9')) {
            strncpy(current_heading, line, len - 1);
            current_heading[len - 1] = '\0';
            line = strtok(NULL, "\n");
            continue;
        }

        // Parse "N = Display Name (Author)"
        int threshold = 0;
        if (line[0] >= '0' && line[0] <= '9') {
            threshold = atoi(line);
            char *eq = strchr(line, '=');
            if (eq && current_heading[0]) {
                // Find author in parens at end
                char *paren_open = NULL;
                // Find last '(' that has a matching ')'
                for (char *p = eq + 1; *p; p++) {
                    if (*p == '(') paren_open = p;
                }
                AuthorEntry *ae = &out[count];
                strncpy(ae->heading, current_heading, FB_MAX_NAME - 1);
                ae->threshold = threshold;
                ae->author[0] = '\0';
                if (paren_open) {
                    char *paren_close = strchr(paren_open, ')');
                    if (paren_close && paren_close > paren_open + 1) {
                        int alen = (int)(paren_close - paren_open - 1);
                        if (alen >= FB_MAX_NAME) alen = FB_MAX_NAME - 1;
                        strncpy(ae->author, paren_open + 1, alen);
                        ae->author[alen] = '\0';
                    }
                }
                count++;
            }
        }
        line = strtok(NULL, "\n");
    }
    free(content);
    return count;
}

static const char *find_author(AuthorEntry *authors, int author_count,
                               const char *heading, int threshold) {
    for (int i = 0; i < author_count; i++) {
        if (authors[i].threshold == threshold && strcmp(authors[i].heading, heading) == 0)
            return authors[i].author;
    }
    return "";
}

int fb_scan_pack(const char *pack_root, FBPackEntry *entries, int max_entries) {
    char dir[FB_MAX_PATH];
    fb_path_join(dir, sizeof(dir), pack_root, MC_ITEMS_DIR_REL);

    DIR *d = opendir(dir);
    if (!d) return 0;

    // Parse authors from model list
    static AuthorEntry authors[MAX_AUTHOR_ENTRIES];
    int author_count = parse_authors(pack_root, authors, MAX_AUTHOR_ENTRIES);

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) && count < max_entries) {
        int len = (int)strlen(ent->d_name);
        if (len < 6 || strcmp(ent->d_name + len - 5, ".json") != 0) continue;

        char item_id[FB_MAX_NAME];
        strncpy(item_id, ent->d_name, len - 5);
        item_id[len - 5] = '\0';

        // Skip synced helmets
        if (is_synced_helmet(item_id)) continue;

        char fullpath[FB_MAX_PATH];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, ent->d_name);
        cJSON *root = load_json_file(fullpath);
        if (!root) continue;

        cJSON *model_data = cJSON_GetObjectItem(root, "model");
        cJSON *type = cJSON_GetObjectItem(model_data, "type");
        cJSON *prop = cJSON_GetObjectItem(model_data, "property");
        if (!type || strcmp(type->valuestring, "minecraft:range_dispatch") != 0 ||
            !prop || strcmp(prop->valuestring, "minecraft:custom_model_data") != 0) {
            cJSON_Delete(root);
            continue;
        }

        const char *heading = fb_heading_for_item(item_id);
        char heading_buf[FB_MAX_NAME];
        if (!heading) {
            fb_display_name(item_id, heading_buf, sizeof(heading_buf));
            heading = heading_buf;
        }

        cJSON *dispatch_entries = cJSON_GetObjectItem(model_data, "entries");
        cJSON *entry;
        cJSON_ArrayForEach(entry, dispatch_entries) {
            if (count >= max_entries) break;

            int threshold = cJSON_GetObjectItem(entry, "threshold")->valueint;
            const char *ref = extract_model_ref(cJSON_GetObjectItem(entry, "model"));
            if (!ref) continue;

            FBPackEntry *pe = &entries[count];
            memset(pe, 0, sizeof(FBPackEntry));

            // Store real MC item ID always
            strncpy(pe->real_item_type, item_id, FB_MAX_NAME - 1);

            // Display as "hats" for stone_button
            if (strcmp(item_id, "stone_button") == 0)
                strncpy(pe->item_type, "hats", FB_MAX_NAME - 1);
            else
                strncpy(pe->item_type, item_id, FB_MAX_NAME - 1);

            strncpy(pe->model_name, ref, FB_MAX_NAME - 1);
            pe->threshold = threshold;
            fb_display_name(ref, pe->display_name, FB_MAX_NAME);

            // Author lookup from model list
            const char *auth = find_author(authors, author_count, heading, threshold);
            if (auth) strncpy(pe->author, auth, FB_MAX_NAME - 1);

            // Check file existence
            char check[FB_MAX_PATH];
            snprintf(check, sizeof(check), "%s/%s/%s.png", pack_root, FB_TEXTURE_DIR, ref);
            pe->has_texture = fb_file_exists(check);
            snprintf(check, sizeof(check), "%s/%s/%s.json", pack_root, FB_MODEL_DIR, ref);
            pe->has_model = fb_file_exists(check);
            snprintf(check, sizeof(check), "%s/%s/%s.json", pack_root, FB_ITEMS_DIR, ref);
            pe->has_item_def = fb_file_exists(check);

            count++;
        }
        cJSON_Delete(root);
    }
    closedir(d);
    return count;
}

// ═════════════════════════════════════════════════════════════════════════════
// Delete model
// ═════════════════════════════════════════════════════════════════════════════

bool fb_delete_model(const char *pack_root, const char *mc_item_id,
                     const char *model_name, int threshold, FBLog *log) {
    // Map display "hats" back to real item
    const char *real_item = mc_item_id;
    if (strcmp(mc_item_id, "hats") == 0) real_item = "stone_button";

    // 1 — Remove from dispatch
    if (remove_from_dispatch(pack_root, real_item, model_name))
        fb_log(log, FB_LOG_SUCCESS, "Removed from %s.json", real_item);
    else
        fb_log(log, FB_LOG_WARN, "Not found in %s.json", real_item);

    // 2 — Remove from model list
    remove_from_model_list(pack_root, real_item, threshold, log);

    // 3-5 — Only delete files if no other refs
    int other_refs = count_dispatch_refs(pack_root, model_name, NULL);
    if (other_refs > 0) {
        fb_log(log, FB_LOG_INFO, "Keeping files — used by %d other item(s)", other_refs);
    } else {
        char path[FB_MAX_PATH];

        snprintf(path, sizeof(path), "%s/%s/%s.png", pack_root, FB_TEXTURE_DIR, model_name);
        if (fb_file_exists(path)) { remove(path); fb_log(log, FB_LOG_SUCCESS, "Deleted %s.png", model_name); }

        // Secondary textures
        char tex_dir[FB_MAX_PATH];
        fb_path_join(tex_dir, sizeof(tex_dir), pack_root, FB_TEXTURE_DIR);
        DIR *d = opendir(tex_dir);
        if (d) {
            char prefix[FB_MAX_NAME + 2];
            snprintf(prefix, sizeof(prefix), "%s_", model_name);
            struct dirent *ent;
            while ((ent = readdir(d))) {
                if (strncmp(ent->d_name, prefix, strlen(prefix)) == 0) {
                    snprintf(path, sizeof(path), "%s/%s", tex_dir, ent->d_name);
                    remove(path);
                    fb_log(log, FB_LOG_SUCCESS, "Deleted %s", ent->d_name);
                }
            }
            closedir(d);
        }

        snprintf(path, sizeof(path), "%s/%s/%s.json", pack_root, FB_MODEL_DIR, model_name);
        if (fb_file_exists(path)) { remove(path); fb_log(log, FB_LOG_SUCCESS, "Deleted model %s.json", model_name); }

        snprintf(path, sizeof(path), "%s/%s/%s_throwing.json", pack_root, FB_MODEL_DIR, model_name);
        if (fb_file_exists(path)) { remove(path); fb_log(log, FB_LOG_SUCCESS, "Deleted %s_throwing.json", model_name); }

        snprintf(path, sizeof(path), "%s/%s/%s.json", pack_root, FB_ITEMS_DIR, model_name);
        if (fb_file_exists(path)) { remove(path); fb_log(log, FB_LOG_SUCCESS, "Deleted item def %s.json", model_name); }
    }

    // Sync helmets
    if (strcmp(real_item, "stone_button") == 0)
        fb_sync_helmets(pack_root, log);

    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// Duplicate to item
// ═════════════════════════════════════════════════════════════════════════════

bool fb_duplicate_to_item(const char *pack_root, const char *model_name,
                          const char *target_item_id, const char *author,
                          FBLog *log) {
    bool existed = false;
    int threshold = update_dispatch(pack_root, target_item_id, model_name, &existed);
    if (existed)
        fb_log(log, FB_LOG_WARN, "Already in %s.json at threshold %d", target_item_id, threshold);
    else
        fb_log(log, FB_LOG_SUCCESS, "Added to %s.json -> threshold %d", target_item_id, threshold);

    update_model_list(pack_root, target_item_id, model_name, threshold, author, NULL, log);

    if (strcmp(target_item_id, "stone_button") == 0)
        fb_sync_helmets(pack_root, log);

    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// Update author
// ═════════════════════════════════════════════════════════════════════════════

bool fb_update_author(const char *pack_root, const char *mc_item_id,
                      int threshold, const char *display_name,
                      const char *new_author, FBLog *log) {
    const char *real_item = mc_item_id;
    if (strcmp(mc_item_id, "hats") == 0) real_item = "stone_button";

    // Just reuse update_model_list — it replaces existing threshold entries
    update_model_list(pack_root, real_item, "", threshold, new_author, NULL, log);

    // Actually we need the display name. Let's do it properly.
    char path[FB_MAX_PATH];
    fb_path_join(path, sizeof(path), pack_root, FB_MODEL_LIST);
    if (!fb_file_exists(path)) {
        fb_log(log, FB_LOG_ERROR, "model list.txt not found");
        return false;
    }

    const char *heading = fb_heading_for_item(real_item);
    if (!heading) { fb_log(log, FB_LOG_ERROR, "Unknown item %s", real_item); return false; }

    char new_line[256];
    if (new_author && new_author[0])
        snprintf(new_line, sizeof(new_line), "%d = %s (%s)", threshold, display_name, new_author);
    else
        snprintf(new_line, sizeof(new_line), "%d = %s", threshold, display_name);

    int file_len;
    char *content = fb_read_file(path, &file_len);
    if (!content) return false;

    char thresh_pat[32];
    snprintf(thresh_pat, sizeof(thresh_pat), "%d =", threshold);

    char *pos = content;
    while ((pos = strstr(pos, thresh_pat))) {
        if (pos == content || *(pos - 1) == '\n') {
            char *line_end = strchr(pos, '\n');
            int line_start = (int)(pos - content);
            int old_line_len = line_end ? (int)(line_end - pos) : (file_len - line_start);
            int new_line_len = (int)strlen(new_line);

            int new_total = file_len - old_line_len + new_line_len;
            char *new_content = malloc(new_total + 1);
            memcpy(new_content, content, line_start);
            memcpy(new_content + line_start, new_line, new_line_len);
            memcpy(new_content + line_start + new_line_len, pos + old_line_len,
                   file_len - line_start - old_line_len);
            new_content[new_total] = '\0';

            fb_write_file(path, new_content, new_total);
            free(new_content);
            free(content);
            fb_log(log, FB_LOG_SUCCESS, "Updated: %s", new_line);
            return true;
        }
        pos++;
    }

    free(content);
    fb_log(log, FB_LOG_WARN, "Threshold %d not found", threshold);
    return false;
}

// ═════════════════════════════════════════════════════════════════════════════
// Scan pack items (item types available in the pack)
// ═════════════════════════════════════════════════════════════════════════════

static const char *KNOWN_ITEMS[] = {
    "stone_button", "pale_oak_button", "diamond_sword", "netherite_sword",
    "wooden_sword", "bow", "totem_of_undying", "goat_horn", "apple",
    "cooked_beef", "writable_book", "diamond_shovel", "diamond_axe",
    "diamond_pickaxe", "cookie", "shield", "trident", "stick", "elytra",
    "baked_potato", "golden_carrot", "feather", "carved_pumpkin", "bundle",
    "white_wool", "milk_bucket", "snowball", "salmon", "leather",
    NULL,
};

int fb_scan_pack_items(const char *pack_root, char items[][FB_MAX_NAME],
                       int max_items) {
    int count = 0;

    // Add known items first
    for (const char **k = KNOWN_ITEMS; *k && count < max_items; k++) {
        strncpy(items[count++], *k, FB_MAX_NAME - 1);
    }

    // Scan pack's minecraft/items/ directory for additional item types
    char dir[FB_MAX_PATH];
    fb_path_join(dir, sizeof(dir), pack_root, MC_ITEMS_DIR_REL);
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) && count < max_items) {
            int len = (int)strlen(ent->d_name);
            if (len < 6 || strcmp(ent->d_name + len - 5, ".json") != 0) continue;

            char item_id[FB_MAX_NAME];
            strncpy(item_id, ent->d_name, len - 5);
            item_id[len - 5] = '\0';

            // Skip if already in list
            bool found = false;
            for (int i = 0; i < count; i++) {
                if (strcmp(items[i], item_id) == 0) { found = true; break; }
            }
            if (!found) strncpy(items[count++], item_id, FB_MAX_NAME - 1);
        }
        closedir(d);
    }

    // Sort alphabetically
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (strcmp(items[i], items[j]) > 0) {
                char tmp[FB_MAX_NAME];
                memcpy(tmp, items[i], FB_MAX_NAME);
                memcpy(items[i], items[j], FB_MAX_NAME);
                memcpy(items[j], tmp, FB_MAX_NAME);
            }
        }
    }

    return count;
}

// ═════════════════════════════════════════════════════════════════════════════
// Check if model already exists
// ═════════════════════════════════════════════════════════════════════════════

bool fb_check_existing(const char *pack_root, const char *model_name,
                       FBLog *log) {
    char path[FB_MAX_PATH];

    snprintf(path, sizeof(path), "%s/%s/%s.json", pack_root, FB_MODEL_DIR, model_name);
    bool has_model = fb_file_exists(path);

    snprintf(path, sizeof(path), "%s/%s/%s.png", pack_root, FB_TEXTURE_DIR, model_name);
    bool has_texture = fb_file_exists(path);

    if (has_model || has_texture) {
        fb_log(log, FB_LOG_WARN, "'%s' already exists — overwriting", model_name);
        return true;
    }
    return false;
}

// ═════════════════════════════════════════════════════════════════════════════
// Custom heading helpers
// ═════════════════════════════════════════════════════════════════════════════

bool fb_needs_heading_name(const char *mc_item_id,
                           const FBCustomHeading *headings, int heading_count) {
    // If it has a built-in heading, no custom one needed
    if (fb_heading_for_item(mc_item_id)) return false;

    // Check if there's a custom heading already
    for (int i = 0; i < heading_count; i++) {
        if (strcmp(headings[i].item_id, mc_item_id) == 0) return false;
    }
    return true;
}

const char *fb_custom_heading_for(const char *mc_item_id,
                                  const FBCustomHeading *headings,
                                  int heading_count) {
    for (int i = 0; i < heading_count; i++) {
        if (strcmp(headings[i].item_id, mc_item_id) == 0)
            return headings[i].heading;
    }
    return NULL;
}

// ═════════════════════════════════════════════════════════════════════════════
// Zip the resource pack
// ═════════════════════════════════════════════════════════════════════════════

static bool is_excluded(const char *name) {
    static const char *EXCLUDE_DIRS[] = {".git", "__pycache__", NULL};
    for (const char **e = EXCLUDE_DIRS; *e; e++) {
        if (strcmp(name, *e) == 0) return true;
    }
    return false;
}

static bool is_excluded_file(const char *name) {
    if (strcmp(name, ".gitignore") == 0) return true;
    // Skip zip files
    int len = (int)strlen(name);
    if (len > 4 && strcmp(name + len - 4, ".zip") == 0) return true;
    return false;
}

static bool zip_add_dir(mz_zip_archive *zip, const char *root_path,
                        const char *rel_prefix) {
    DIR *d = opendir(root_path);
    if (!d) return false;

    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (ent->d_name[0] == '.' && (ent->d_name[1] == '\0' ||
            (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) continue;

        char full[FB_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", root_path, ent->d_name);

        char arc[FB_MAX_PATH];
        if (rel_prefix[0])
            snprintf(arc, sizeof(arc), "%s/%s", rel_prefix, ent->d_name);
        else
            snprintf(arc, sizeof(arc), "%s", ent->d_name);

        struct stat st;
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (is_excluded(ent->d_name)) continue;
            if (!zip_add_dir(zip, full, arc)) { closedir(d); return false; }
        } else {
            if (is_excluded_file(ent->d_name)) continue;
            if (!mz_zip_writer_add_file(zip, arc, full, NULL, 0,
                                        MZ_BEST_COMPRESSION)) {
                closedir(d);
                return false;
            }
        }
    }
    closedir(d);
    return true;
}

bool fb_zip_pack(const char *pack_root, const char *output_path,
                 FBLog *log) {
    char path[FB_MAX_PATH];

    if (output_path && output_path[0]) {
        strncpy(path, output_path, sizeof(path) - 1);
    } else {
        // Default: <pack_parent>/pack.zip
        char parent[FB_MAX_PATH];
        strncpy(parent, pack_root, sizeof(parent) - 1);
        char *last_slash = strrchr(parent, '/');
        if (last_slash) *last_slash = '\0';
        snprintf(path, sizeof(path), "%s/pack.zip", parent);
    }

    // Remove old zip
    remove(path);

    mz_zip_archive zip = {0};
    if (!mz_zip_writer_init_file(&zip, path, 0)) {
        fb_log(log, FB_LOG_ERROR, "Failed to create zip: %s", path);
        return false;
    }

    fb_log(log, FB_LOG_HEADER, "Zipping pack...");

    if (!zip_add_dir(&zip, pack_root, "")) {
        fb_log(log, FB_LOG_ERROR, "Failed adding files to zip");
        mz_zip_writer_end(&zip);
        remove(path);
        return false;
    }

    if (!mz_zip_writer_finalize_archive(&zip)) {
        fb_log(log, FB_LOG_ERROR, "Failed to finalize zip");
        mz_zip_writer_end(&zip);
        remove(path);
        return false;
    }

    mz_uint64 size = zip.m_archive_size;
    mz_zip_writer_end(&zip);

    fb_log(log, FB_LOG_SUCCESS, "Packed -> %s (%llu KB)", path,
           (unsigned long long)(size / 1024));
    return true;
}
