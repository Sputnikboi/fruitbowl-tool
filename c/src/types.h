#ifndef FB_TYPES_H
#define FB_TYPES_H

#include "raylib.h"
#include <stdbool.h>

// ── Limits ──────────────────────────────────────────────────────────────────
#define FB_MAX_ELEMENTS     256
#define FB_MAX_FACES        6
#define FB_MAX_TEXTURES     16
#define FB_MAX_NAME         128
#define FB_MAX_PATH         512
#define FB_MAX_MODELS       1024
#define FB_MAX_ENTRIES      256
#define FB_MAX_LOG          128

// ── Model types ─────────────────────────────────────────────────────────────

typedef struct {
    float uv[4];
    int   texture_idx;
    int   rotation;
    int   tintindex;
    bool  has_texture;
    bool  has_tintindex;
} FBFace;

typedef struct {
    float from[3];
    float to[3];
    float rot_angle;
    float rot_origin[3];
    char  rot_axis;
    FBFace faces[6];
    bool  shade;
} FBElement;

typedef struct {
    float rotation[3];
    float translation[3];
    float scale[3];
    bool  has_rotation;
    bool  has_translation;
    bool  has_scale;
} FBDisplayView;

typedef struct {
    FBDisplayView thirdperson_right;
    FBDisplayView thirdperson_left;
    FBDisplayView firstperson_right;
    FBDisplayView firstperson_left;
    FBDisplayView ground;
    FBDisplayView gui;
    FBDisplayView head;
    FBDisplayView fixed;
} FBDisplay;

typedef struct {
    char       name[FB_MAX_NAME];
    int        texture_size[2];
    FBElement  elements[FB_MAX_ELEMENTS];
    int        element_count;
    int        texture_count;
    FBDisplay  display;
    bool       has_display;
    Texture2D  textures[FB_MAX_TEXTURES];
    bool       textures_loaded;
    char       groups_json[8192]; // raw JSON for groups (preserved from bbmodel)
    bool       has_groups;
} FBModel;

// ── Pack management ─────────────────────────────────────────────────────────

typedef struct {
    char item_type[FB_MAX_NAME];       // display name ("hats" for stone_button)
    char real_item_type[FB_MAX_NAME];  // actual MC item ID ("stone_button")
    char model_name[FB_MAX_NAME];
    int  threshold;
    char author[FB_MAX_NAME];
    char display_name[FB_MAX_NAME];
    bool has_texture;
    bool has_model;
    bool has_item_def;
} FBPackEntry;

// ── Log messages ────────────────────────────────────────────────────────────

typedef enum {
    FB_LOG_INFO,
    FB_LOG_SUCCESS,
    FB_LOG_WARN,
    FB_LOG_ERROR,
    FB_LOG_HEADER,
} FBLogLevel;

typedef struct {
    char       text[256];
    FBLogLevel level;
} FBLogEntry;

typedef struct {
    FBLogEntry entries[FB_MAX_LOG];
    int        count;
} FBLog;

// ── App state ───────────────────────────────────────────────────────────────

typedef enum {
    TAB_IMPORT,
    TAB_BATCH,
    TAB_MANAGE,
    TAB_PREVIEW,
} FBTab;

// ── Batch entry ──────────────────────────────────────────────────────────────

typedef struct {
    char path[FB_MAX_PATH];
    char author[FB_MAX_NAME];
    char display_name[FB_MAX_NAME]; // derived from filename for display
} FBBatchEntry;

// ── Custom heading persistence ──────────────────────────────────────────────

typedef struct {
    char item_id[FB_MAX_NAME];
    char heading[FB_MAX_NAME];
} FBCustomHeading;

#define FB_MAX_BATCH        256
#define FB_MAX_HEADINGS     64
#define FB_MAX_SUGGESTIONS  64

typedef struct {
    // Paths
    char pack_path[FB_MAX_PATH];
    char bbmodel_path[FB_MAX_PATH];
    char model_name[FB_MAX_NAME];
    char author[FB_MAX_NAME];
    char item_type[FB_MAX_NAME];

    // Active tab
    FBTab       active_tab;

    // Log
    FBLog       log;

    // Pack scan results
    FBPackEntry pack_entries[FB_MAX_MODELS];
    int         pack_entry_count;
    int         manage_selected;        // index into pack_entries, -1 = none
    char        manage_filter[FB_MAX_NAME];
    float       manage_scroll;
    int         manage_sort_col;        // 0=type,1=name,2=threshold,3=author,4=status
    bool        manage_sort_asc;

    // Preview
    FBModel     preview_model;
    bool        preview_loaded;
    Camera3D    camera;

    // Batch mode
    FBBatchEntry batch_entries[FB_MAX_BATCH];
    int          batch_count;
    int          batch_selected;        // -1 = none
    float        batch_scroll;

    // Item type suggestions (from scan_pack_items)
    char        item_suggestions[FB_MAX_SUGGESTIONS][FB_MAX_NAME];
    int         item_suggestion_count;
    bool        item_dropdown_open;
    int         item_dropdown_scroll;

    // Settings persistence
    FBCustomHeading custom_headings[FB_MAX_HEADINGS];
    int             custom_heading_count;
    bool            settings_loaded;

    // Heading name dialog
    bool        heading_dialog_open;
    char        heading_dialog_item[FB_MAX_NAME];
    char        heading_dialog_value[FB_MAX_NAME];
    char        pending_heading_override[FB_MAX_NAME];
    bool        pending_import;  // deferred import waiting for heading name

    // Duplicate dialog
    bool        dup_dialog_open;
    char        dup_dialog_value[FB_MAX_NAME];  // target item type
    int         dup_dialog_entry;               // index of entry being duplicated

    // Manage inline author editing
    int         manage_editing_author;  // index being edited, -1 = none
    char        manage_edit_buf[FB_MAX_NAME];

    // GUI state
    bool        should_close;
    bool        pack_scanned;
} FBAppState;

#endif // FB_TYPES_H
