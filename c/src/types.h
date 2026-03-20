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
#define FB_MAX_MODELS       512
#define FB_MAX_ENTRIES      256
#define FB_MAX_LOG          64

// ── Model types ─────────────────────────────────────────────────────────────

typedef struct {
    float uv[4];           // [u1, v1, u2, v2] in MC 0-16 space
    int   texture_idx;     // which texture this face uses
    int   rotation;        // UV rotation (0, 90, 180, 270)
    bool  has_texture;
} FBFace;

typedef struct {
    float from[3];
    float to[3];
    float rot_angle;
    float rot_origin[3];
    char  rot_axis;        // 'x', 'y', 'z', or 0 for none
    FBFace faces[6];       // north, east, south, west, up, down
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

    // Loaded textures for rendering
    Texture2D  textures[FB_MAX_TEXTURES];
    bool       textures_loaded;
} FBModel;

// ── BBModel (raw parsed, before conversion) ─────────────────────────────────

typedef struct {
    char   name[FB_MAX_NAME];
    char   source_b64[1];  // flexible array — NOT used, we parse differently
} FBBBTexture;

typedef struct {
    int    width;
    int    height;
    int    texture_count;
    char   texture_names[FB_MAX_TEXTURES][FB_MAX_NAME];
    // Base64 sources stored separately due to size
} FBBBResolution;

// ── Pack management ─────────────────────────────────────────────────────────

typedef struct {
    char item_type[FB_MAX_NAME];
    char model_name[FB_MAX_NAME];
    int  threshold;
    char author[FB_MAX_NAME];
    char display_name[FB_MAX_NAME];
    bool has_texture;
    bool has_model;
    bool has_item_def;
} FBPackEntry;

typedef struct {
    FBPackEntry entries[FB_MAX_MODELS];
    int         count;
} FBPackIndex;

// ── Log messages ────────────────────────────────────────────────────────────

typedef enum {
    FB_LOG_INFO,
    FB_LOG_SUCCESS,
    FB_LOG_WARN,
    FB_LOG_ERROR,
    FB_LOG_HEADER,
} FBLogLevel;

typedef struct {
    char      text[256];
    FBLogLevel level;
} FBLogEntry;

typedef struct {
    FBLogEntry entries[FB_MAX_LOG];
    int        count;
} FBLog;

// ── App state ───────────────────────────────────────────────────────────────

typedef enum {
    TAB_SINGLE,
    TAB_BATCH,
    TAB_MANAGE,
    TAB_PREVIEW,
} FBTab;

typedef struct {
    // Paths
    char pack_path[FB_MAX_PATH];
    char bbmodel_path[FB_MAX_PATH];
    char model_name[FB_MAX_NAME];
    char author[FB_MAX_NAME];
    char item_type[FB_MAX_NAME];

    // State
    FBTab       active_tab;
    FBPackIndex pack_index;
    FBLog       log;

    // Preview
    FBModel     preview_model;
    bool        preview_loaded;
    Camera3D    camera;

    // GUI state
    bool        should_close;
} FBAppState;

#endif // FB_TYPES_H
