#include "gui.h"
#include "model.h"
#include "renderer.h"
#include "pack.h"
#include "util.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include "filedialog.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <dirent.h>

// ── Layout (base values, multiplied by gui_scale) ───────────────────────────
#define PANEL_W_BASE 380
#define TAB_H_BASE   32
#define PAD_BASE     8
#define ROW_H_BASE   24
#define FIELD_H_BASE 26
#define BTN_H_BASE   30

// Scale factor — adjust this to make everything bigger/smaller
static float gui_scale = 1.3f;

// Scaled helpers (recomputed each frame for resize support)
static int PANEL_W, TAB_H, PAD, ROW_H, FIELD_H, BTN_H;
static int FONT_LABEL, FONT_SMALL, FONT_NORMAL, FONT_TITLE;

static void update_scale(void) {
    PANEL_W    = (int)(PANEL_W_BASE * gui_scale);
    TAB_H      = (int)(TAB_H_BASE * gui_scale);
    PAD        = (int)(PAD_BASE * gui_scale);
    ROW_H      = (int)(ROW_H_BASE * gui_scale);
    FIELD_H    = (int)(FIELD_H_BASE * gui_scale);
    BTN_H      = (int)(BTN_H_BASE * gui_scale);
    FONT_LABEL = (int)(11 * gui_scale);
    FONT_SMALL = (int)(10 * gui_scale);
    FONT_NORMAL= (int)(12 * gui_scale);
    FONT_TITLE = (int)(14 * gui_scale);
}

// ── Colors ──────────────────────────────────────────────────────────────────
#define C_BG        (Color){35, 35, 38, 255}
#define C_PANEL     (Color){30, 30, 30, 255}
#define C_PANEL2    (Color){38, 38, 42, 255}
#define C_BORDER    (Color){60, 60, 60, 255}
#define C_TEXT      (Color){200, 200, 200, 255}
#define C_DIM       (Color){120, 120, 120, 255}
#define C_GREEN     (Color){100, 200, 100, 255}
#define C_YELLOW    (Color){228, 200, 90, 255}
#define C_RED       (Color){228, 90, 90, 255}
#define C_BLUE      (Color){122, 176, 223, 255}
#define C_HIGHLIGHT (Color){50, 60, 80, 255}
#define C_ROW_ALT   (Color){34, 34, 38, 255}
#define C_ROW_HOVER (Color){45, 50, 65, 255}

// ── Sidebar collapse state ───────────────────────────────────────────────────
static bool sidebar_collapsed = false;
#define COLLAPSE_BTN_W_BASE 24

// ── Orbital camera ──────────────────────────────────────────────────────────
static float cam_yaw = 0.8f, cam_pitch = 0.5f, cam_dist = 40.0f;
static Vector3 cam_target = {8, 8, 8};
static bool show_grid = true;

static void compute_model_bounds(const FBModel *m, Vector3 *center, float *radius) {
    if (m->element_count == 0) { *center = (Vector3){8,8,8}; *radius = 16; return; }
    float mins[3]={999,999,999}, maxs[3]={-999,-999,-999};
    for (int i = 0; i < m->element_count; i++)
        for (int a = 0; a < 3; a++) {
            if (m->elements[i].from[a] < mins[a]) mins[a] = m->elements[i].from[a];
            if (m->elements[i].to[a]   < mins[a]) mins[a] = m->elements[i].to[a];
            if (m->elements[i].from[a] > maxs[a]) maxs[a] = m->elements[i].from[a];
            if (m->elements[i].to[a]   > maxs[a]) maxs[a] = m->elements[i].to[a];
        }
    center->x = (mins[0]+maxs[0])/2; center->y = (mins[1]+maxs[1])/2; center->z = (mins[2]+maxs[2])/2;
    float dx=maxs[0]-mins[0], dy=maxs[1]-mins[1], dz=maxs[2]-mins[2];
    *radius = sqrtf(dx*dx+dy*dy+dz*dz)/2;
}

static void reset_camera_to_model(Camera3D *cam, const FBModel *m) {
    cam_yaw = 0.8f; cam_pitch = 0.5f;
    float r; compute_model_bounds(m, &cam_target, &r);
    cam_dist = r * 2.5f; if (cam_dist < 20) cam_dist = 20;
    cam->target = cam_target;
}

static void update_orbital_camera(Camera3D *cam, int viewport_x) {
    if (GetMouseX() <= viewport_x) return;
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 d = GetMouseDelta();
        cam_yaw -= d.x*0.005f; cam_pitch -= d.y*0.005f;
        if (cam_pitch > 1.5f) cam_pitch = 1.5f;
        if (cam_pitch < -1.5f) cam_pitch = -1.5f;
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        Vector2 d = GetMouseDelta();
        float cy=cosf(cam_yaw), sy=sinf(cam_yaw), sp=cam_dist*0.003f;
        cam_target.x -= cy*d.x*sp; cam_target.z -= sy*d.x*sp;
        cam_target.y -= d.y*sp;
    }
    float w = GetMouseWheelMove();
    if (w != 0) { cam_dist -= w*cam_dist*0.1f; if(cam_dist<5)cam_dist=5; if(cam_dist>200)cam_dist=200; }

    cam->target = cam_target;
    cam->position.x = cam_target.x + cam_dist*cosf(cam_pitch)*sinf(cam_yaw);
    cam->position.y = cam_target.y + cam_dist*sinf(cam_pitch);
    cam->position.z = cam_target.z + cam_dist*cosf(cam_pitch)*cosf(cam_yaw);
    cam->up = (Vector3){0,1,0};
}

// ── Log drawing ─────────────────────────────────────────────────────────────
static void draw_log(FBLog *log, int x, int y, int w, int h) {
    DrawRectangle(x, y, w, h, (Color){20, 20, 22, 255});
    int line_h = FONT_LABEL + 3;
    int max_lines = h / line_h;
    int start = log->count > max_lines ? log->count - max_lines : 0;
    for (int i = start; i < log->count; i++) {
        FBLogEntry *e = &log->entries[i];
        Color c = C_TEXT;
        switch (e->level) {
            case FB_LOG_SUCCESS: c = C_GREEN; break;
            case FB_LOG_WARN: c = C_YELLOW; break;
            case FB_LOG_ERROR: c = C_RED; break;
            case FB_LOG_HEADER: c = RAYWHITE; break;
            default: c = C_BLUE; break;
        }
        DrawText(e->text, x + 4, y + 2 + (i - start) * line_h, FONT_LABEL, c);
    }
}

// ── Label + text field helper ───────────────────────────────────────────────
static bool gui_labeled_field(int x, int y, int w, const char *label, char *buf, int buf_size) {
    DrawText(label, x, y, FONT_LABEL, C_DIM);
    Rectangle r = {(float)x, (float)(y + 14), (float)w, FIELD_H};
    // Use raygui textbox
    int result = GuiTextBox(r, buf, buf_size, CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
    return result != 0;
}

// ═════════════════════════════════════════════════════════════════════════════
// TAB: Import
// ═════════════════════════════════════════════════════════════════════════════

static bool import_editing[4] = {false}; // track which fields are being edited

static void draw_import_tab(FBAppState *s, int x, int y, int w, int h) {
    int cy = y + PAD;

    int browse_w = 60;
    DrawText("Resource Pack Folder", x + PAD, cy, FONT_LABEL, C_DIM); cy += FONT_LABEL + 3;
    Rectangle pack_r = {(float)(x+PAD), (float)cy, (float)(w-2*PAD-browse_w-PAD), FIELD_H};
    if (GuiTextBox(pack_r, s->pack_path, FB_MAX_PATH, import_editing[0])) import_editing[0] = !import_editing[0];
    Rectangle pack_btn = {(float)(x+w-PAD-browse_w), (float)cy, (float)browse_w, FIELD_H};
    if (GuiButton(pack_btn, "Browse")) {
        char tmp[FB_MAX_PATH];
        if (fb_pick_folder(tmp, sizeof(tmp))) {
            strncpy(s->pack_path, tmp, FB_MAX_PATH - 1);
        }
    }
    cy += FIELD_H + PAD;

    DrawText("BBModel File (drag & drop or browse)", x + PAD, cy, FONT_LABEL, C_DIM); cy += FONT_LABEL + 3;
    Rectangle bb_r = {(float)(x+PAD), (float)cy, (float)(w-2*PAD-browse_w-PAD), FIELD_H};
    if (GuiTextBox(bb_r, s->bbmodel_path, FB_MAX_PATH, import_editing[1])) import_editing[1] = !import_editing[1];
    Rectangle bb_btn = {(float)(x+w-PAD-browse_w), (float)cy, (float)browse_w, FIELD_H};
    if (GuiButton(bb_btn, "Browse")) {
        char tmp[FB_MAX_PATH];
        if (fb_pick_bbmodel(tmp, sizeof(tmp))) {
            strncpy(s->bbmodel_path, tmp, FB_MAX_PATH - 1);
            fb_sanitize_name(tmp, s->model_name, FB_MAX_NAME);
            // Also load preview
            if (s->preview_loaded) fb_unload_model_textures(&s->preview_model);
            if (fb_parse_bbmodel(tmp, &s->preview_model)) {
                fb_load_bbmodel_textures(tmp, &s->preview_model);
                s->preview_loaded = true;
                reset_camera_to_model(&s->camera, &s->preview_model);
            }
        }
    }
    cy += FIELD_H + PAD;

    DrawText("Model Name (blank = from filename)", x + PAD, cy, FONT_LABEL, C_DIM); cy += FONT_LABEL + 3;
    Rectangle name_r = {(float)(x+PAD), (float)cy, (float)(w-2*PAD), FIELD_H};
    if (GuiTextBox(name_r, s->model_name, FB_MAX_NAME, import_editing[2])) import_editing[2] = !import_editing[2];
    cy += FIELD_H + PAD;

    DrawText("Author (optional)", x + PAD, cy, FONT_LABEL, C_DIM); cy += FONT_LABEL + 3;
    Rectangle auth_r = {(float)(x+PAD), (float)cy, (float)(w-2*PAD), FIELD_H};
    if (GuiTextBox(auth_r, s->author, FB_MAX_NAME, import_editing[3])) import_editing[3] = !import_editing[3];
    cy += FIELD_H + PAD;

    DrawText("Minecraft Item", x + PAD, cy, FONT_LABEL, C_DIM); cy += FONT_LABEL + 3;
    Rectangle item_r = {(float)(x+PAD), (float)cy, (float)(w-2*PAD), FIELD_H};
    static bool item_edit = false;
    static char prev_item_text[FB_MAX_NAME] = {0};

    // Check for dropdown clicks FIRST, before GuiTextBox can steal the click
    bool dropdown_clicked = false;
    if (s->item_dropdown_open && s->item_suggestion_count > 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        // Build filtered list to check hit
        char needle_pre[FB_MAX_NAME];
        strncpy(needle_pre, s->item_type, sizeof(needle_pre) - 1); needle_pre[sizeof(needle_pre)-1] = '\0';
        for (char *p = needle_pre; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;

        int dd_y = cy + (int)FIELD_H;  // dropdown starts right below item field
        int dd_max = 8, dd_count = 0;
        int dd_indices_pre[FB_MAX_SUGGESTIONS];
        for (int i = 0; i < s->item_suggestion_count && dd_count < dd_max; i++) {
            if (!needle_pre[0] || strstr(s->item_suggestions[i], needle_pre))
                dd_indices_pre[dd_count++] = i;
        }
        for (int di = 0; di < dd_count; di++) {
            Rectangle dr = {(float)(x+PAD), (float)(dd_y + di * ROW_H), (float)(w-2*PAD), (float)ROW_H};
            if (CheckCollisionPointRec(GetMousePosition(), dr)) {
                strncpy(s->item_type, s->item_suggestions[dd_indices_pre[di]], FB_MAX_NAME - 1);
                strncpy(prev_item_text, s->item_type, FB_MAX_NAME - 1);
                s->item_dropdown_open = false;
                item_edit = false;
                dropdown_clicked = true;
                break;
            }
        }
    }

    if (!dropdown_clicked) {
        if (GuiTextBox(item_r, s->item_type, FB_MAX_NAME, item_edit)) {
            item_edit = !item_edit;
            if (item_edit && s->item_suggestion_count == 0 && s->pack_path[0]) {
                s->item_suggestion_count = fb_scan_pack_items(
                    s->pack_path, s->item_suggestions, FB_MAX_SUGGESTIONS);
            }
        }
    }
    // Toggle dropdown when editing and text changes
    if (item_edit && strcmp(s->item_type, prev_item_text) != 0) {
        s->item_dropdown_open = true;
        s->item_dropdown_scroll = 0;
    }
    strncpy(prev_item_text, s->item_type, FB_MAX_NAME - 1);
    if (!item_edit) s->item_dropdown_open = false;
    cy += FIELD_H;

    // Dropdown saved cy for later — button goes below zip section
    int dropdown_cy = cy; // remember where dropdown draws
    cy += PAD + 8;        // skip past dropdown area

    // ── Output log (fills remaining space above bottom controls) ────────
    // Bottom controls: Import button + pad + zip separator + pad + zip label + zip row + pad
    int bottom_h = BTN_H + PAD + 1 + PAD + FONT_LABEL + 3 + FIELD_H + PAD;
    int log_top = cy + FONT_LABEL + 3;
    int log_h = (y + h) - log_top - bottom_h - PAD;
    if (log_h < 40) log_h = 40; // minimum height
    DrawText("Output", x + PAD, cy, FONT_LABEL, C_DIM); cy += FONT_LABEL + 3;
    draw_log(&s->log, x + PAD, cy, w - 2*PAD, log_h);
    cy += log_h + PAD;

    // ── Import button ───────────────────────────────────────────────────
    static double import_cooldown_until = 0.0;
    Rectangle btn_r = {(float)(x+PAD), (float)cy, (float)(w-2*PAD), BTN_H};
    bool do_import = false;
    if (GetTime() <= import_cooldown_until) {
        int prev_state = GuiGetState();
        GuiSetState(STATE_DISABLED);
        GuiButton(btn_r, "Add to Pack");
        GuiSetState(prev_state);
    } else {
        do_import = GuiButton(btn_r, "Add to Pack");
    }
    // Also handle pending import (after heading dialog completes)
    if (s->pending_import && !s->heading_dialog_open) {
        do_import = true;
        s->pending_import = false;
    }

    if (do_import) {
        fb_log_clear(&s->log);
        if (!s->pack_path[0]) {
            fb_log(&s->log, FB_LOG_ERROR, "Set a pack path first");
        } else if (!s->bbmodel_path[0]) {
            fb_log(&s->log, FB_LOG_ERROR, "Select a .bbmodel file first");
        } else if (!s->item_type[0]) {
            fb_log(&s->log, FB_LOG_ERROR, "Enter a Minecraft item type");
        } else if (fb_needs_heading_name(s->item_type, s->custom_headings, s->custom_heading_count)
                   && !s->pending_heading_override[0]) {
            // Need to ask for heading name first
            s->heading_dialog_open = true;
            strncpy(s->heading_dialog_item, s->item_type, FB_MAX_NAME - 1);
            s->heading_dialog_value[0] = '\0';
            s->pending_import = true;
        } else {
            const char *name = s->model_name[0] ? s->model_name : NULL;
            const char *heading = s->pending_heading_override[0] ? s->pending_heading_override : NULL;
            if (!heading) heading = fb_custom_heading_for(s->item_type, s->custom_headings, s->custom_heading_count);
            fb_add_to_pack(s->bbmodel_path, s->pack_path, s->item_type,
                           name, s->author, heading, &s->log);
            s->pending_heading_override[0] = '\0';
            import_cooldown_until = GetTime() + 1.0; // 1s lockout
            // Auto-rescan
            s->pack_entry_count = fb_scan_pack(s->pack_path, s->pack_entries, FB_MAX_MODELS);
            s->pack_scanned = true;
            // Refresh item suggestions
            s->item_suggestion_count = fb_scan_pack_items(
                s->pack_path, s->item_suggestions, FB_MAX_SUGGESTIONS);
        }
    }
    cy += BTN_H + PAD;

    // ── Zip Pack section ────────────────────────────────────────────────
    DrawLine(x + PAD, cy, x + w - PAD, cy, C_BORDER); cy += PAD;
    static char zip_name[FB_MAX_NAME] = "VXX - Fruitbowl 4";
    DrawText("Zip Name", x + PAD, cy, FONT_LABEL, C_DIM); cy += FONT_LABEL + 3;
    int zip_btn_w = 80;
    Rectangle zip_name_r = {(float)(x+PAD), (float)cy, (float)(w-2*PAD-zip_btn_w-PAD), FIELD_H};
    static bool zip_name_edit = false;
    if (GuiTextBox(zip_name_r, zip_name, FB_MAX_NAME, zip_name_edit)) zip_name_edit = !zip_name_edit;
    Rectangle zip_btn_r = {(float)(x+w-PAD-zip_btn_w), (float)cy, (float)zip_btn_w, FIELD_H};
    if (GuiButton(zip_btn_r, "Zip Pack")) {
        fb_log_clear(&s->log);
        if (!s->pack_path[0]) {
            fb_log(&s->log, FB_LOG_ERROR, "Set a pack path first");
        } else {
            char out_path[FB_MAX_PATH];
            // Put the zip next to the pack folder
            char parent[FB_MAX_PATH];
            strncpy(parent, s->pack_path, sizeof(parent) - 1);
            parent[sizeof(parent)-1] = '\0';
            char *sl = strrchr(parent, '/');
            if (sl) *sl = '\0';
            snprintf(out_path, sizeof(out_path), "%s/%s.zip",
                     parent, zip_name[0] ? zip_name : "pack");
            fb_zip_pack(s->pack_path, out_path, &s->log);
        }
    }
    cy += FIELD_H + PAD;

    // ── Dropdown overlay (drawn last so it renders on top) ──────────────
    if (s->item_dropdown_open && s->item_suggestion_count > 0) {
        char needle[FB_MAX_NAME];
        strncpy(needle, s->item_type, sizeof(needle) - 1); needle[sizeof(needle)-1] = '\0';
        for (char *p = needle; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;

        int dd_max = 8;
        int dd_count = 0;
        int dd_indices[FB_MAX_SUGGESTIONS];
        for (int i = 0; i < s->item_suggestion_count && dd_count < dd_max; i++) {
            if (!needle[0] || strstr(s->item_suggestions[i], needle))
                dd_indices[dd_count++] = i;
        }

        if (dd_count > 0) {
            int dd_h = dd_count * ROW_H;
            DrawRectangle(x+PAD, dropdown_cy, w-2*PAD, dd_h, (Color){25,25,28,245});
            DrawRectangleLines(x+PAD, dropdown_cy, w-2*PAD, dd_h, C_BORDER);
            for (int di = 0; di < dd_count; di++) {
                int dy = dropdown_cy + di * ROW_H;
                Rectangle dr = {(float)(x+PAD), (float)dy, (float)(w-2*PAD), (float)ROW_H};
                bool dhov = CheckCollisionPointRec(GetMousePosition(), dr);
                if (dhov) DrawRectangleRec(dr, C_ROW_HOVER);
                DrawText(s->item_suggestions[dd_indices[di]], x+PAD+6,
                         dy + ROW_H/2 - FONT_SMALL/2, FONT_SMALL, dhov ? RAYWHITE : C_TEXT);
            }
        } else {
            s->item_dropdown_open = false;
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// TAB: Manage
// ═════════════════════════════════════════════════════════════════════════════

static bool filter_editing = false;
static char prev_filter[FB_MAX_NAME] = {0};

static bool entry_matches_filter(const FBPackEntry *e, const char *filter) {
    if (!filter[0]) return true;
    if (strcmp(filter, "noauthor") == 0) return e->author[0] == '\0';
    // Case-insensitive substring search (includes real_item_type)
    char haystack[512];
    snprintf(haystack, sizeof(haystack), "%s %s %s %s %s",
             e->item_type, e->real_item_type, e->model_name, e->author, e->display_name);
    for (char *p = haystack; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    char needle[FB_MAX_NAME];
    strncpy(needle, filter, sizeof(needle)-1); needle[sizeof(needle)-1] = '\0';
    for (char *p = needle; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return strstr(haystack, needle) != NULL;
}

// ── Sorting comparators ─────────────────────────────────────────────────────
static int sort_col_g = 0;
static bool sort_asc_g = true;
static FBPackEntry *sort_entries_g = NULL;

static int compare_entries(const void *a, const void *b) {
    int ia = *(const int *)a, ib = *(const int *)b;
    const FBPackEntry *ea = &sort_entries_g[ia];
    const FBPackEntry *eb = &sort_entries_g[ib];
    int cmp = 0;
    switch (sort_col_g) {
        case 0: cmp = strcmp(ea->item_type, eb->item_type); break;
        case 1: cmp = strcmp(ea->model_name, eb->model_name); break;
        case 2: cmp = ea->threshold - eb->threshold; break;
        case 3: cmp = strcmp(ea->author, eb->author); break;
        case 4: {
            bool a_ok = ea->has_texture && ea->has_model && ea->has_item_def;
            bool b_ok = eb->has_texture && eb->has_model && eb->has_item_def;
            cmp = (int)a_ok - (int)b_ok;
        } break;
    }
    return sort_asc_g ? cmp : -cmp;
}

static void draw_manage_tab(FBAppState *s, int x, int y, int w, int h) {
    int cy = y + PAD;

    // Pack path
    DrawText("Resource Pack Folder", x+PAD, cy, FONT_LABEL, C_DIM); cy += FONT_LABEL + 3;
    int mgr_browse_w = 60, mgr_scan_w = 80;
    Rectangle pack_r = {(float)(x+PAD), (float)cy, (float)(w - 2*PAD - mgr_browse_w - mgr_scan_w - 2*PAD), FIELD_H};
    static bool pack_edit_m = false;
    if (GuiTextBox(pack_r, s->pack_path, FB_MAX_PATH, pack_edit_m)) pack_edit_m = !pack_edit_m;

    Rectangle mgr_browse_r = {(float)(x + w - PAD - mgr_scan_w - PAD - mgr_browse_w), (float)cy, (float)mgr_browse_w, FIELD_H};
    if (GuiButton(mgr_browse_r, "Browse")) {
        char tmp[FB_MAX_PATH];
        if (fb_pick_folder(tmp, sizeof(tmp))) {
            strncpy(s->pack_path, tmp, FB_MAX_PATH - 1);
        }
    }

    // Scan button
    Rectangle scan_r = {(float)(x + w - PAD - mgr_scan_w), (float)cy, (float)mgr_scan_w, FIELD_H};
    if (GuiButton(scan_r, "Scan Pack")) {
        fb_log_clear(&s->log);
        s->pack_entry_count = fb_scan_pack(s->pack_path, s->pack_entries, FB_MAX_MODELS);
        s->pack_scanned = true;
        s->manage_selected = -1;
        fb_log(&s->log, FB_LOG_SUCCESS, "Found %d models", s->pack_entry_count);
    }
    cy += FIELD_H + PAD;

    // Filter
    DrawText("Search (type 'noauthor' to find missing)", x+PAD, cy, FONT_LABEL, C_DIM); cy += FONT_LABEL + 3;
    Rectangle filt_r = {(float)(x+PAD), (float)cy, (float)(w-2*PAD), FIELD_H};
    if (GuiTextBox(filt_r, s->manage_filter, FB_MAX_NAME, filter_editing)) filter_editing = !filter_editing;
    // Reset scroll when filter changes
    if (strcmp(s->manage_filter, prev_filter) != 0) {
        s->manage_scroll = 0;
        s->manage_selected = -1;
        strncpy(prev_filter, s->manage_filter, FB_MAX_NAME - 1);
    }
    cy += FIELD_H + PAD;

    // Model list header — columns proportional to width
    int list_x = x + PAD;
    int list_w = w - 2*PAD;
    int col_num = (int)(35 * gui_scale);
    int col_status = (int)(35 * gui_scale);
    int remaining = list_w - col_num - col_status;
    int col_type = remaining * 22 / 100;
    int col_name = remaining * 40 / 100;
    int col_author = remaining - col_type - col_name;

    // Sortable column headers
    DrawRectangle(list_x, cy, list_w, ROW_H, C_BORDER);
    int cx = list_x + 4;
    struct { const char *label; int width; int col_id; } hdrs[] = {
        {"Type", col_type, 0}, {"Model", col_name, 1}, {"#", col_num, 2},
        {"Author", col_author, 3}, {"OK", col_status, 4},
    };
    for (int hi = 0; hi < 5; hi++) {
        Rectangle hr = {(float)cx, (float)cy, (float)hdrs[hi].width, (float)ROW_H};
        bool hov = CheckCollisionPointRec(GetMousePosition(), hr);
        const char *sort_ind = "";
        if (s->manage_sort_col == hdrs[hi].col_id)
            sort_ind = s->manage_sort_asc ? " ^" : " v";
        Color hc = hov ? RAYWHITE : (Color){180,180,180,255};
        DrawText(TextFormat("%s%s", hdrs[hi].label, sort_ind), cx, cy + ROW_H/2 - FONT_SMALL/2, FONT_SMALL, hc);
        if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (s->manage_sort_col == hdrs[hi].col_id)
                s->manage_sort_asc = !s->manage_sort_asc;
            else { s->manage_sort_col = hdrs[hi].col_id; s->manage_sort_asc = true; }
        }
        cx += hdrs[hi].width;
    }
    cy += ROW_H;

    // Scrollable model list
    // Reserve: buttons (BTN_H) + count label (16) + log label (14) + log area (~80) + padding
    int reserve = BTN_H + 16 + 14 + 80 + 4*PAD;
    int list_h = h - (cy - y) - reserve;
    if (list_h < 60) list_h = 60;
    Rectangle list_area = {(float)list_x, (float)cy, (float)list_w, (float)list_h};

    BeginScissorMode(list_x, cy, list_w, list_h);

    int row_count = 0;
    int visible_rows = list_h / ROW_H;
    int scroll_offset = (int)(s->manage_scroll);

    // Count filtered entries
    int filtered_indices[FB_MAX_MODELS];
    int filtered_count = 0;
    for (int i = 0; i < s->pack_entry_count; i++) {
        if (entry_matches_filter(&s->pack_entries[i], s->manage_filter))
            filtered_indices[filtered_count++] = i;
    }

    // Sort filtered entries by selected column
    sort_col_g = s->manage_sort_col;
    sort_asc_g = s->manage_sort_asc;
    sort_entries_g = s->pack_entries;
    if (filtered_count > 1)
        qsort(filtered_indices, filtered_count, sizeof(int), compare_entries);

    // Scroll with mouse wheel when hovering list
    if (CheckCollisionPointRec(GetMousePosition(), list_area)) {
        s->manage_scroll -= GetMouseWheelMove() * 3;
        if (s->manage_scroll < 0) s->manage_scroll = 0;
        float max_scroll = (float)(filtered_count - visible_rows);
        if (max_scroll < 0) max_scroll = 0;
        if (s->manage_scroll > max_scroll) s->manage_scroll = max_scroll;
    }

    for (int fi = scroll_offset; fi < filtered_count && row_count < visible_rows + 1; fi++) {
        int i = filtered_indices[fi];
        FBPackEntry *e = &s->pack_entries[i];
        int ry = cy + row_count * ROW_H;

        // Row background
        bool selected = (s->manage_selected == i);
        bool hovered = CheckCollisionPointRec(GetMousePosition(),
                        (Rectangle){(float)list_x, (float)ry, (float)list_w, ROW_H});

        if (selected) DrawRectangle(list_x, ry, list_w, ROW_H, C_HIGHLIGHT);
        else if (hovered) DrawRectangle(list_x, ry, list_w, ROW_H, C_ROW_HOVER);
        else if (fi % 2) DrawRectangle(list_x, ry, list_w, ROW_H, C_ROW_ALT);

        // Click to select; double-click on author column for inline editing
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            // Check if double-click on author column
            float author_x = (float)(list_x + 4 + col_type + col_name + col_num);
            float mx = GetMousePosition().x;
            static double last_click_time = 0;
            static int last_click_idx = -1;
            double now = GetTime();
            if (i == last_click_idx && (now - last_click_time) < 0.35 &&
                mx >= author_x && mx < author_x + col_author) {
                // Start inline editing
                s->manage_editing_author = i;
                strncpy(s->manage_edit_buf, e->author, FB_MAX_NAME - 1);
            }
            last_click_time = now;
            last_click_idx = i;
            s->manage_selected = i;
        }

        // Columns
        bool all_ok = e->has_texture && e->has_model && e->has_item_def;
        Color text_c = all_ok ? C_TEXT : C_YELLOW;
        int ty = ry + ROW_H/2 - FONT_SMALL/2;

        cx = list_x + 4;
        DrawText(e->item_type, cx, ty, FONT_SMALL, text_c); cx += col_type;
        DrawText(e->model_name, cx, ty, FONT_SMALL, text_c); cx += col_name;
        DrawText(TextFormat("%d", e->threshold), cx, ty, FONT_SMALL, text_c); cx += col_num;

        // Author column — inline editing or display
        if (s->manage_editing_author == i) {
            Rectangle edit_r = {(float)cx, (float)ry, (float)(col_author - 4), (float)ROW_H};
            if (GuiTextBox(edit_r, s->manage_edit_buf, FB_MAX_NAME, true)) {
                // Editing finished (Enter or click away)
                strncpy(e->author, s->manage_edit_buf, FB_MAX_NAME - 1);
                fb_update_author(s->pack_path, e->real_item_type, e->threshold,
                                 e->display_name, s->manage_edit_buf, &s->log);
                s->manage_editing_author = -1;
            }
            // Cancel on Escape
            if (IsKeyPressed(KEY_ESCAPE)) s->manage_editing_author = -1;
        } else {
            DrawText(e->author[0] ? e->author : "-", cx, ty, FONT_SMALL, e->author[0] ? text_c : C_DIM);
        }
        cx += col_author;
        DrawText(all_ok ? "OK" : "!!", cx, ty, FONT_SMALL, all_ok ? C_GREEN : C_RED);

        row_count++;
    }

    EndScissorMode();
    cy += list_h;

    // Count display
    DrawText(TextFormat("%d / %d models", filtered_count, s->pack_entry_count),
             list_x, cy + 2, 10, C_DIM);
    cy += 16;

    // Action buttons
    int btn_w = (list_w - 3 * PAD) / 4;
    float by = (float)cy;

    int btn_count = 5;
    btn_w = (list_w - (btn_count - 1) * PAD) / btn_count;

    if (GuiButton((Rectangle){(float)list_x, by, (float)btn_w, BTN_H}, "Delete")) {
        if (s->manage_selected >= 0 && s->manage_selected < s->pack_entry_count) {
            FBPackEntry *e = &s->pack_entries[s->manage_selected];
            fb_log_clear(&s->log);
            fb_log(&s->log, FB_LOG_HEADER, "Deleting %s...", e->model_name);
            fb_delete_model(s->pack_path, e->real_item_type, e->model_name, e->threshold, &s->log);
            s->pack_entry_count = fb_scan_pack(s->pack_path, s->pack_entries, FB_MAX_MODELS);
            s->manage_selected = -1;
        }
    }

    if (GuiButton((Rectangle){(float)(list_x + (btn_w + PAD)), by, (float)btn_w, BTN_H}, "Preview")) {
        if (s->manage_selected >= 0 && s->pack_path[0]) {
            FBPackEntry *e = &s->pack_entries[s->manage_selected];
            if (s->preview_loaded) fb_unload_model_textures(&s->preview_model);
            s->preview_loaded = false;
            fb_log_clear(&s->log);
            if (fb_load_pack_model(s->pack_path, e->model_name, &s->preview_model)) {
                s->preview_loaded = true;
                reset_camera_to_model(&s->camera, &s->preview_model);
                fb_log(&s->log, FB_LOG_SUCCESS, "Loaded %s (%d elements, %d textures)",
                       e->model_name, s->preview_model.element_count, s->preview_model.texture_count);
                s->active_tab = TAB_PREVIEW;
            } else {
                fb_log(&s->log, FB_LOG_ERROR, "Failed to load model: %s", e->model_name);
            }
        }
    }

    if (GuiButton((Rectangle){(float)(list_x + 2*(btn_w+PAD)), by, (float)btn_w, BTN_H}, "Duplicate")) {
        if (s->manage_selected >= 0) {
            s->dup_dialog_open = true;
            s->dup_dialog_entry = s->manage_selected;
            s->dup_dialog_value[0] = '\0';
        }
    }

    if (GuiButton((Rectangle){(float)(list_x + 3*(btn_w+PAD)), by, (float)btn_w, BTN_H}, "Set Author")) {
        if (s->manage_selected >= 0) {
            FBPackEntry *e = &s->pack_entries[s->manage_selected];
            fb_log_clear(&s->log);
            fb_update_author(s->pack_path, e->real_item_type, e->threshold,
                             e->display_name, s->author, &s->log);
            strncpy(e->author, s->author, FB_MAX_NAME - 1);
        }
    }

    if (GuiButton((Rectangle){(float)(list_x + 4*(btn_w+PAD)), by, (float)btn_w, BTN_H}, "Update")) {
        if (s->manage_selected >= 0) {
            FBPackEntry *e = &s->pack_entries[s->manage_selected];
            char tmp[FB_MAX_PATH];
            if (fb_pick_bbmodel(tmp, sizeof(tmp))) {
                fb_log_clear(&s->log);
                fb_log(&s->log, FB_LOG_HEADER, "Updating %s...", e->model_name);
                fb_add_to_pack(tmp, s->pack_path, e->real_item_type,
                               e->model_name, e->author, NULL, &s->log);
                s->pack_entry_count = fb_scan_pack(s->pack_path, s->pack_entries, FB_MAX_MODELS);
            }
        }
    }
    cy += BTN_H + PAD;

    // Log
    DrawText("Output", x + PAD, cy, FONT_LABEL, C_DIM); cy += FONT_LABEL + 3;
    draw_log(&s->log, x + PAD, cy, w - 2*PAD, h - (cy - y) - PAD);
}

// ═════════════════════════════════════════════════════════════════════════════
// TAB: Preview (3D viewport)
// ═════════════════════════════════════════════════════════════════════════════

static void draw_preview_sidebar(FBAppState *s, int x, int y, int w, int h) {
    int cy = y + PAD;

    DrawText("Drag & drop a .bbmodel file", x+PAD, cy, FONT_NORMAL, C_DIM); cy += 16;
    DrawText("onto this window to preview", x+PAD, cy, FONT_NORMAL, C_DIM); cy += 24;

    if (s->preview_loaded) {
        DrawLine(x, cy, x+w, cy, C_BORDER); cy += PAD;
        DrawText("Loaded Model:", x+PAD, cy, FONT_TITLE, C_GREEN); cy += 18;
        DrawText(TextFormat("Name: %s", s->preview_model.name), x+PAD+4, cy, FONT_LABEL, C_TEXT); cy += FONT_LABEL + 3;
        DrawText(TextFormat("Elements: %d", s->preview_model.element_count), x+PAD+4, cy, FONT_LABEL, C_TEXT); cy += FONT_LABEL + 3;
        DrawText(TextFormat("Texture: %dx%d", s->preview_model.texture_size[0], s->preview_model.texture_size[1]),
                 x+PAD+4, cy, FONT_LABEL, C_TEXT); cy += FONT_LABEL + 3;
        DrawText(TextFormat("Textures: %d", s->preview_model.texture_count), x+PAD+4, cy, FONT_LABEL, C_TEXT); cy += 20;

        for (int i = 0; i < s->preview_model.texture_count && i < 4; i++) {
            Texture2D tex = s->preview_model.textures[i];
            if (tex.id > 0) {
                float scale = 56.0f / (float)(tex.width > tex.height ? tex.width : tex.height);
                DrawTextureEx(tex, (Vector2){(float)(x+PAD+4 + i*64), (float)cy}, 0, scale, WHITE);
            }
        }
        cy += 64;
    }

    // Controls
    cy = y + h - 90;
    DrawLine(x, cy, x+w, cy, C_BORDER); cy += PAD;
    DrawText("Controls:", x+PAD, cy, FONT_NORMAL, C_TEXT); cy += 16;
    DrawText("  Left drag  - Rotate", x+PAD, cy, FONT_SMALL, C_DIM); cy += 12;
    DrawText("  Scroll     - Zoom", x+PAD, cy, FONT_SMALL, C_DIM); cy += 12;
    DrawText("  Mid drag   - Pan", x+PAD, cy, FONT_SMALL, C_DIM); cy += 12;
    DrawText("  R - Reset   G - Grid", x+PAD, cy, FONT_SMALL, C_DIM); cy += 12;
    DrawText("  H - Hide sidebar", x+PAD, cy, FONT_SMALL, C_DIM);
}

static void draw_preview_viewport(FBAppState *s, int x, int y, int w, int h) {
    BeginScissorMode(x, y, w, h);
    DrawRectangle(x, y, w, h, C_BG);

    BeginMode3D(s->camera);
    if (show_grid) {
        DrawGrid(32, 1.0f);
        DrawCubeWires((Vector3){8,8,8}, 16,16,16, (Color){80,80,80,128});
    }
    if (s->preview_loaded) fb_draw_model(&s->preview_model);
    EndMode3D();

    if (!s->preview_loaded) {
        const char *msg = "Drop a .bbmodel file here";
        DrawText(msg, x + (w - MeasureText(msg, 18))/2, y + h/2 - 9, 18, C_DIM);
    }
    DrawText(TextFormat("%d FPS", GetFPS()), x+w-65, y+6, 12, C_DIM);
    EndScissorMode();
}

// ═════════════════════════════════════════════════════════════════════════════
// TAB: Batch Import
// ═════════════════════════════════════════════════════════════════════════════

static float batch_scroll = 0;
static int batch_editing_author = -1;
static char batch_edit_buf[FB_MAX_NAME] = {0};

static void draw_batch_tab(FBAppState *s, int x, int y, int w, int h) {
    int cy = y + PAD;

    // Pack path (shared with import tab)
    int browse_w = 60;
    DrawText("Resource Pack Folder", x + PAD, cy, FONT_LABEL, C_DIM); cy += FONT_LABEL + 3;
    Rectangle pack_r = {(float)(x+PAD), (float)cy, (float)(w-2*PAD-browse_w-PAD), FIELD_H};
    static bool bp_edit = false;
    if (GuiTextBox(pack_r, s->pack_path, FB_MAX_PATH, bp_edit)) bp_edit = !bp_edit;
    Rectangle bpb = {(float)(x+w-PAD-browse_w), (float)cy, (float)browse_w, FIELD_H};
    if (GuiButton(bpb, "Browse")) {
        char tmp[FB_MAX_PATH];
        if (fb_pick_folder(tmp, sizeof(tmp))) {
            strncpy(s->pack_path, tmp, FB_MAX_PATH - 1);
        }
    }
    cy += FIELD_H + PAD;

    // Item type for batch
    DrawText("Minecraft Item (for all files)", x + PAD, cy, FONT_LABEL, C_DIM); cy += FONT_LABEL + 3;
    Rectangle bit_r = {(float)(x+PAD), (float)cy, (float)(w-2*PAD), FIELD_H};
    static bool bit_edit = false;
    if (GuiTextBox(bit_r, s->item_type, FB_MAX_NAME, bit_edit)) bit_edit = !bit_edit;
    cy += FIELD_H + PAD;

    // Button row: Add Files, Add Folder, Remove, Clear, Set Author
    int bbtn_count = 5;
    int bbtn_w = (w - 2*PAD - (bbtn_count-1)*PAD) / bbtn_count;
    float bby = (float)cy;

    if (GuiButton((Rectangle){(float)(x+PAD), bby, (float)bbtn_w, BTN_H}, "Add Files")) {
        char tmp[FB_MAX_PATH];
        if (fb_pick_bbmodel(tmp, sizeof(tmp))) {
            if (s->batch_count < FB_MAX_BATCH) {
                FBBatchEntry *be = &s->batch_entries[s->batch_count];
                strncpy(be->path, tmp, FB_MAX_PATH - 1);
                strncpy(be->author, s->author, FB_MAX_NAME - 1);
                fb_sanitize_name(tmp, be->display_name, FB_MAX_NAME);
                s->batch_count++;
            }
        }
    }

    if (GuiButton((Rectangle){(float)(x+PAD+bbtn_w+PAD), bby, (float)bbtn_w, BTN_H}, "Add Folder")) {
        char folder[FB_MAX_PATH];
        if (fb_pick_folder(folder, sizeof(folder))) {
            // Scan folder for .bbmodel files
            DIR *d = opendir(folder);
            if (d) {
                struct dirent *ent;
                while ((ent = readdir(d)) && s->batch_count < FB_MAX_BATCH) {
                    int len = (int)strlen(ent->d_name);
                    if (len > 8 && strcmp(ent->d_name + len - 8, ".bbmodel") == 0) {
                        FBBatchEntry *be = &s->batch_entries[s->batch_count];
                        snprintf(be->path, FB_MAX_PATH, "%s/%s", folder, ent->d_name);
                        strncpy(be->author, s->author, FB_MAX_NAME - 1);
                        fb_sanitize_name(ent->d_name, be->display_name, FB_MAX_NAME);
                        s->batch_count++;
                    }
                }
                closedir(d);
            }
        }
    }

    if (GuiButton((Rectangle){(float)(x+PAD+2*(bbtn_w+PAD)), bby, (float)bbtn_w, BTN_H}, "Remove")) {
        if (s->batch_selected >= 0 && s->batch_selected < s->batch_count) {
            memmove(&s->batch_entries[s->batch_selected],
                    &s->batch_entries[s->batch_selected + 1],
                    sizeof(FBBatchEntry) * (s->batch_count - s->batch_selected - 1));
            s->batch_count--;
            if (s->batch_selected >= s->batch_count) s->batch_selected = s->batch_count - 1;
        }
    }

    if (GuiButton((Rectangle){(float)(x+PAD+3*(bbtn_w+PAD)), bby, (float)bbtn_w, BTN_H}, "Clear")) {
        s->batch_count = 0;
        s->batch_selected = -1;
    }

    if (GuiButton((Rectangle){(float)(x+PAD+4*(bbtn_w+PAD)), bby, (float)bbtn_w, BTN_H}, "Set Author")) {
        // Set author on selected entry (or all if none selected)
        if (s->batch_selected >= 0 && s->batch_selected < s->batch_count) {
            strncpy(s->batch_entries[s->batch_selected].author, s->author, FB_MAX_NAME - 1);
        } else {
            for (int i = 0; i < s->batch_count; i++)
                strncpy(s->batch_entries[i].author, s->author, FB_MAX_NAME - 1);
        }
    }
    cy += BTN_H + PAD;

    // Batch list header
    int list_x = x + PAD;
    int list_w = w - 2*PAD;
    int col_file = list_w * 65 / 100;
    int col_auth = list_w - col_file;

    DrawRectangle(list_x, cy, list_w, ROW_H, C_BORDER);
    DrawText("File", list_x + 4, cy + ROW_H/2 - FONT_SMALL/2, FONT_SMALL, RAYWHITE);
    DrawText("Author", list_x + 4 + col_file, cy + ROW_H/2 - FONT_SMALL/2, FONT_SMALL, RAYWHITE);
    cy += ROW_H;

    // Scrollable batch list
    int reserve = BTN_H + 14 + 80 + 4*PAD;
    int list_h = h - (cy - y) - reserve;
    if (list_h < 60) list_h = 60;
    Rectangle blist_area = {(float)list_x, (float)cy, (float)list_w, (float)list_h};

    if (CheckCollisionPointRec(GetMousePosition(), blist_area)) {
        batch_scroll -= GetMouseWheelMove() * 3;
        if (batch_scroll < 0) batch_scroll = 0;
        int visible = list_h / ROW_H;
        float max_s = (float)(s->batch_count - visible);
        if (max_s < 0) max_s = 0;
        if (batch_scroll > max_s) batch_scroll = max_s;
    }

    BeginScissorMode(list_x, cy, list_w, list_h);
    int visible_rows = list_h / ROW_H;
    int scroll_off = (int)batch_scroll;
    int drawn = 0;

    for (int i = scroll_off; i < s->batch_count && drawn < visible_rows + 1; i++) {
        FBBatchEntry *be = &s->batch_entries[i];
        int ry = cy + drawn * ROW_H;
        bool sel = (s->batch_selected == i);
        bool hov = CheckCollisionPointRec(GetMousePosition(),
                    (Rectangle){(float)list_x, (float)ry, (float)list_w, ROW_H});

        if (sel) DrawRectangle(list_x, ry, list_w, ROW_H, C_HIGHLIGHT);
        else if (hov) DrawRectangle(list_x, ry, list_w, ROW_H, C_ROW_HOVER);
        else if (i % 2) DrawRectangle(list_x, ry, list_w, ROW_H, C_ROW_ALT);

        if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            // Double-click on author column to edit
            float auth_x = (float)(list_x + 4 + col_file);
            static double b_last_click = 0;
            static int b_last_idx = -1;
            double now = GetTime();
            if (i == b_last_idx && (now - b_last_click) < 0.35 &&
                GetMousePosition().x >= auth_x) {
                batch_editing_author = i;
                strncpy(batch_edit_buf, be->author, FB_MAX_NAME - 1);
            }
            b_last_click = now;
            b_last_idx = i;
            s->batch_selected = i;
        }

        int ty = ry + ROW_H/2 - FONT_SMALL/2;
        DrawText(be->display_name, list_x + 4, ty, FONT_SMALL, C_TEXT);

        if (batch_editing_author == i) {
            Rectangle er = {(float)(list_x + 4 + col_file), (float)ry, (float)(col_auth - 4), (float)ROW_H};
            if (GuiTextBox(er, batch_edit_buf, FB_MAX_NAME, true)) {
                strncpy(be->author, batch_edit_buf, FB_MAX_NAME - 1);
                batch_editing_author = -1;
            }
            if (IsKeyPressed(KEY_ESCAPE)) batch_editing_author = -1;
        } else {
            DrawText(be->author[0] ? be->author : "-", list_x + 4 + col_file, ty, FONT_SMALL,
                     be->author[0] ? C_TEXT : C_DIM);
        }
        drawn++;
    }
    EndScissorMode();
    cy += list_h;

    // Count
    DrawText(TextFormat("%d files", s->batch_count), list_x, cy + 2, 10, C_DIM);
    cy += 16;

    // Import All button
    Rectangle import_all = {(float)(x+PAD), (float)cy, (float)(w-2*PAD), BTN_H};
    static bool batch_btn_was_pressed = false;
    bool batch_btn_now = GuiButton(import_all, "Import All to Pack");
    bool do_batch = batch_btn_now && !batch_btn_was_pressed;
    batch_btn_was_pressed = batch_btn_now;
    if (do_batch) {
        fb_log_clear(&s->log);
        if (!s->pack_path[0]) {
            fb_log(&s->log, FB_LOG_ERROR, "Set a pack path first");
        } else if (!s->item_type[0]) {
            fb_log(&s->log, FB_LOG_ERROR, "Enter a Minecraft item type");
        } else if (s->batch_count == 0) {
            fb_log(&s->log, FB_LOG_WARN, "No files to import");
        } else {
            for (int i = 0; i < s->batch_count; i++) {
                FBBatchEntry *be = &s->batch_entries[i];
                fb_log(&s->log, FB_LOG_HEADER, "Importing %s...", be->display_name);
                const char *heading = fb_custom_heading_for(s->item_type, s->custom_headings, s->custom_heading_count);
                fb_add_to_pack(be->path, s->pack_path, s->item_type,
                               be->display_name, be->author, heading, &s->log);
            }
            fb_log(&s->log, FB_LOG_SUCCESS, "Batch import complete (%d files)", s->batch_count);
            s->pack_entry_count = fb_scan_pack(s->pack_path, s->pack_entries, FB_MAX_MODELS);
            s->pack_scanned = true;
        }
    }
    cy += BTN_H + PAD;

    // Log
    DrawText("Output", x + PAD, cy, FONT_LABEL, C_DIM); cy += FONT_LABEL + 3;
    draw_log(&s->log, x + PAD, cy, w - 2*PAD, h - (cy - y) - PAD);
}

// ═════════════════════════════════════════════════════════════════════════════
// Heading Name Dialog (modal overlay)
// ═════════════════════════════════════════════════════════════════════════════

static bool heading_dlg_edit = false;

static void draw_heading_dialog(FBAppState *s, int panel_w) {
    // Dim background
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, (Color){0,0,0,150});

    // Dialog box
    int dw = (int)(350 * gui_scale), dh = (int)(160 * gui_scale);
    int dx = (panel_w - dw) / 2, dy = (sh - dh) / 2;
    DrawRectangle(dx, dy, dw, dh, C_PANEL);
    DrawRectangleLines(dx, dy, dw, dh, C_BORDER);

    int cy = dy + PAD;
    DrawText("Heading Name", dx + PAD, cy, FONT_TITLE, RAYWHITE); cy += FONT_TITLE + 8;
    DrawText(TextFormat("Enter a display heading for '%s'", s->heading_dialog_item),
             dx + PAD, cy, FONT_SMALL, C_DIM); cy += FONT_SMALL + 4;
    DrawText("(used in model list.txt)", dx + PAD, cy, FONT_SMALL, C_DIM); cy += FONT_SMALL + PAD;

    Rectangle edit_r = {(float)(dx+PAD), (float)cy, (float)(dw-2*PAD), FIELD_H};
    if (GuiTextBox(edit_r, s->heading_dialog_value, FB_MAX_NAME, heading_dlg_edit))
        heading_dlg_edit = !heading_dlg_edit;
    cy += FIELD_H + PAD;

    int btn_w2 = (dw - 3*PAD) / 2;
    if (GuiButton((Rectangle){(float)(dx+PAD), (float)cy, (float)btn_w2, BTN_H}, "OK")) {
        if (s->heading_dialog_value[0]) {
            // Save the custom heading
            strncpy(s->pending_heading_override, s->heading_dialog_value, FB_MAX_NAME - 1);
            if (s->custom_heading_count < FB_MAX_HEADINGS) {
                FBCustomHeading *ch = &s->custom_headings[s->custom_heading_count++];
                strncpy(ch->item_id, s->heading_dialog_item, FB_MAX_NAME - 1);
                strncpy(ch->heading, s->heading_dialog_value, FB_MAX_NAME - 1);
                fb_save_settings(s);
            }
            s->heading_dialog_open = false;
            heading_dlg_edit = false;
        }
    }
    if (GuiButton((Rectangle){(float)(dx+2*PAD+btn_w2), (float)cy, (float)btn_w2, BTN_H}, "Cancel")) {
        s->heading_dialog_open = false;
        s->pending_import = false;
        heading_dlg_edit = false;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        s->heading_dialog_open = false;
        s->pending_import = false;
        heading_dlg_edit = false;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Duplicate Dialog (modal overlay)
// ═════════════════════════════════════════════════════════════════════════════

static bool dup_dlg_edit = false;
static bool dup_dropdown_open = false;

static void draw_dup_dialog(FBAppState *s, int panel_w) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, (Color){0,0,0,150});

    int dw = (int)(350 * gui_scale), dh = (int)(220 * gui_scale);
    int dx = (panel_w - dw) / 2, dy = (sh - dh) / 2;
    DrawRectangle(dx, dy, dw, dh, C_PANEL);
    DrawRectangleLines(dx, dy, dw, dh, C_BORDER);

    FBPackEntry *e = &s->pack_entries[s->dup_dialog_entry];

    int cy = dy + PAD;
    DrawText("Duplicate Model", dx + PAD, cy, FONT_TITLE, RAYWHITE); cy += FONT_TITLE + 8;
    DrawText(TextFormat("Copy '%s' to another item:", e->model_name),
             dx + PAD, cy, FONT_SMALL, C_DIM); cy += FONT_SMALL + PAD;

    DrawText("Target Item", dx + PAD, cy, FONT_LABEL, C_DIM); cy += FONT_LABEL + 3;
    Rectangle edit_r = {(float)(dx+PAD), (float)cy, (float)(dw-2*PAD), FIELD_H};
    int dd_y = cy + (int)FIELD_H; // dropdown starts right below the field

    // Build filtered suggestion list once
    char needle[FB_MAX_NAME];
    strncpy(needle, s->dup_dialog_value, sizeof(needle) - 1);
    needle[sizeof(needle)-1] = '\0';
    for (char *p = needle; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;

    // Buttons are at bottom — limit dropdown to not overlap them
    int btn_y = dy + dh - BTN_H - PAD;
    int max_dd_h = btn_y - dd_y - PAD;
    int dd_max_items = max_dd_h / ROW_H;
    if (dd_max_items < 1) dd_max_items = 1;
    if (dd_max_items > 8) dd_max_items = 8;

    int dd_count = 0, dd_indices[FB_MAX_SUGGESTIONS];
    if (dup_dropdown_open && s->item_suggestion_count > 0) {
        for (int i = 0; i < s->item_suggestion_count && dd_count < dd_max_items; i++) {
            if (!needle[0] || strstr(s->item_suggestions[i], needle))
                dd_indices[dd_count++] = i;
        }
        if (dd_count == 0) dup_dropdown_open = false;
    }

    // Check dropdown hit BEFORE textbox — mouse press selects (not release,
    // because release arrives too late after textbox deactivation steals focus)
    bool dd_clicked = false;
    if (dup_dropdown_open && dd_count > 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        for (int di = 0; di < dd_count; di++) {
            Rectangle dr = {(float)(dx+PAD), (float)(dd_y + di * ROW_H),
                            (float)(dw-2*PAD), (float)ROW_H};
            if (CheckCollisionPointRec(GetMousePosition(), dr)) {
                strncpy(s->dup_dialog_value, s->item_suggestions[dd_indices[di]], FB_MAX_NAME - 1);
                dup_dropdown_open = false;
                dd_clicked = true;
                break;
            }
        }
    }

    if (!dd_clicked) {
        bool was_edit = dup_dlg_edit;
        if (GuiTextBox(edit_r, s->dup_dialog_value, FB_MAX_NAME, dup_dlg_edit))
            dup_dlg_edit = !dup_dlg_edit;
        if (dup_dlg_edit && s->dup_dialog_value[0])
            dup_dropdown_open = true;
        if (dup_dlg_edit && !s->dup_dialog_value[0])
            dup_dropdown_open = false;
        // Only close dropdown if clicked outside both field and dropdown area
        if (!dup_dlg_edit && was_edit && !dup_dropdown_open) {
            // textbox lost focus normally
        }
    }
    cy = dd_y; // move cy past the field

    // Draw dropdown overlay (clipped to not overlap buttons)
    if (dup_dropdown_open && dd_count > 0) {
        int dd_h = dd_count * ROW_H;
        DrawRectangle(dx+PAD, cy, dw-2*PAD, dd_h, (Color){25,25,28,245});
        DrawRectangleLines(dx+PAD, cy, dw-2*PAD, dd_h, C_BORDER);
        for (int di = 0; di < dd_count; di++) {
            int ddy = cy + di * ROW_H;
            Rectangle dr = {(float)(dx+PAD), (float)ddy, (float)(dw-2*PAD), (float)ROW_H};
            bool dhov = CheckCollisionPointRec(GetMousePosition(), dr);
            if (dhov) DrawRectangleRec(dr, C_ROW_HOVER);
            DrawText(s->item_suggestions[dd_indices[di]], dx+PAD+6,
                     ddy + ROW_H/2 - FONT_SMALL/2, FONT_SMALL, dhov ? RAYWHITE : C_TEXT);
        }
    }

    // Buttons at bottom of dialog
    int btn_w2 = (dw - 3*PAD) / 2;
    if (GuiButton((Rectangle){(float)(dx+PAD), (float)btn_y, (float)btn_w2, BTN_H}, "Duplicate")) {
        if (s->dup_dialog_value[0]) {
            fb_log_clear(&s->log);
            fb_duplicate_to_item(s->pack_path, e->model_name, s->dup_dialog_value, e->author, &s->log);
            s->pack_entry_count = fb_scan_pack(s->pack_path, s->pack_entries, FB_MAX_MODELS);
            s->dup_dialog_open = false;
            dup_dlg_edit = false;
            dup_dropdown_open = false;
        }
    }
    if (GuiButton((Rectangle){(float)(dx+2*PAD+btn_w2), (float)btn_y, (float)btn_w2, BTN_H}, "Cancel")) {
        s->dup_dialog_open = false;
        dup_dlg_edit = false;
        dup_dropdown_open = false;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        s->dup_dialog_open = false;
        dup_dlg_edit = false;
        dup_dropdown_open = false;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Main GUI
// ═════════════════════════════════════════════════════════════════════════════

void fb_draw_gui(FBAppState *s) {
    update_scale();
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    // Handle drag & drop (works in all tabs)
    if (IsFileDropped()) {
        FilePathList files = LoadDroppedFiles();
        if (files.count > 0) {
            const char *path = files.paths[0];
            int len = (int)strlen(path);
            if (len > 8 && strcmp(path + len - 8, ".bbmodel") == 0) {
                // Load for preview
                if (s->preview_loaded) fb_unload_model_textures(&s->preview_model);
                if (fb_parse_bbmodel(path, &s->preview_model)) {
                    fb_load_bbmodel_textures(path, &s->preview_model);
                    s->preview_loaded = true;
                    strncpy(s->bbmodel_path, path, FB_MAX_PATH - 1);
                    // Auto-fill model name
                    fb_sanitize_name(path, s->model_name, FB_MAX_NAME);
                    reset_camera_to_model(&s->camera, &s->preview_model);
                    // Switch to preview if not already there
                    if (s->active_tab != TAB_IMPORT) s->active_tab = TAB_PREVIEW;
                }
            }
        }
        UnloadDroppedFiles(files);
    }

    // Keyboard shortcuts (only in preview tab)
    if (s->active_tab == TAB_PREVIEW) {
        if (IsKeyPressed(KEY_R)) reset_camera_to_model(&s->camera, &s->preview_model);
        if (IsKeyPressed(KEY_G)) show_grid = !show_grid;
        if (IsKeyPressed(KEY_H)) sidebar_collapsed = !sidebar_collapsed;
    }

    // ── Compute viewport and panel sizes ───────────────────────────────
    int collapse_btn_w = (int)(COLLAPSE_BTN_W_BASE * gui_scale);
    int vp_x, vp_w, panel_w;
    bool preview_collapsed = (s->active_tab == TAB_PREVIEW && sidebar_collapsed);

    if (preview_collapsed) {
        // Collapsed: just a thin strip for the expand button
        panel_w = collapse_btn_w;
        vp_x = collapse_btn_w;
        vp_w = sw - collapse_btn_w;
    } else if (s->active_tab == TAB_PREVIEW) {
        panel_w = PANEL_W;
        vp_x = PANEL_W;
        vp_w = sw - PANEL_W;
    } else {
        vp_w = (sw - PANEL_W) / 2;
        if (vp_w < 100) vp_w = 0;
        panel_w = sw - vp_w;
        vp_x = panel_w;
    }

    // ── Tab bar ─────────────────────────────────────────────────────────
    if (!preview_collapsed) {
        const char *tab_names[] = {"Import", "Batch", "Manage", "Preview"};
        int tab_count = 4;
        int tab_w = panel_w / tab_count;

        for (int i = 0; i < tab_count; i++) {
            Rectangle r = {(float)(i * tab_w), 0, (float)tab_w, TAB_H};
            bool active = (s->active_tab == (FBTab)i);
            Color bg = active ? C_PANEL : C_PANEL2;
            Color fg = active ? RAYWHITE : C_DIM;
            DrawRectangleRec(r, bg);
            int tw = MeasureText(tab_names[i], 13);
            DrawText(tab_names[i], r.x + (r.width - tw)/2, r.y + 9, 13, fg);
            if (!active && CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                s->active_tab = (FBTab)i;
                if ((FBTab)i != TAB_PREVIEW) sidebar_collapsed = false;
            }
        }
        DrawLine(0, TAB_H, panel_w, TAB_H, C_BORDER);
    }

    // ── Left panel content ──────────────────────────────────────────────
    int panel_y = preview_collapsed ? 0 : TAB_H;
    int panel_h = sh - panel_y;

    bool just_expanded = false;
    if (preview_collapsed) {
        // Draw thin collapsed strip — entire strip is clickable to expand
        Rectangle strip_r = {0, 0, (float)collapse_btn_w, (float)sh};
        bool hover = CheckCollisionPointRec(GetMousePosition(), strip_r);
        DrawRectangleRec(strip_r, hover ? C_ROW_HOVER : C_PANEL);
        // Draw ">" arrow centered
        const char *arrow = ">";
        int aw = MeasureText(arrow, FONT_NORMAL);
        DrawText(arrow, (collapse_btn_w - aw)/2, sh/2 - FONT_NORMAL/2, FONT_NORMAL, hover ? RAYWHITE : C_DIM);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            sidebar_collapsed = false;
            just_expanded = true;
        }
    } else {
        DrawRectangle(0, panel_y, panel_w, panel_h, C_PANEL);

        switch (s->active_tab) {
            case TAB_IMPORT:
                draw_import_tab(s, 0, panel_y, panel_w, panel_h);
                break;
            case TAB_BATCH:
                draw_batch_tab(s, 0, panel_y, panel_w, panel_h);
                break;
            case TAB_MANAGE:
                draw_manage_tab(s, 0, panel_y, panel_w, panel_h);
                break;
            case TAB_PREVIEW:
                draw_preview_sidebar(s, 0, panel_y, panel_w, panel_h);
                break;
        }

        // Modal dialogs (drawn on top)
        if (s->heading_dialog_open) {
            draw_heading_dialog(s, panel_w);
        }
        if (s->dup_dialog_open) {
            draw_dup_dialog(s, panel_w);
        }
    }

    if (vp_w > 0) {
        update_orbital_camera(&s->camera, vp_x);
        draw_preview_viewport(s, vp_x, 0, vp_w, sh);
    }

    // Panel/viewport border
    if (vp_w > 0) {
        DrawLine(vp_x, 0, vp_x, sh, C_BORDER);
    }

    // Collapse button (drawn on top of the border, visible when sidebar is open in preview tab)
    if (s->active_tab == TAB_PREVIEW && !sidebar_collapsed && !just_expanded) {
        Rectangle col_r = {(float)(vp_x - collapse_btn_w), (float)(sh/2 - BTN_H/2), (float)collapse_btn_w, (float)BTN_H};
        bool col_hover = CheckCollisionPointRec(GetMousePosition(), col_r);
        DrawRectangleRec(col_r, col_hover ? C_ROW_HOVER : C_PANEL2);
        DrawRectangleLines((int)col_r.x, (int)col_r.y, (int)col_r.width, (int)col_r.height, C_BORDER);
        const char *arr = "<";
        int arrw = MeasureText(arr, FONT_NORMAL);
        DrawText(arr, (int)(col_r.x + (col_r.width - arrw)/2), sh/2 - FONT_NORMAL/2, FONT_NORMAL, col_hover ? RAYWHITE : C_DIM);
        if (col_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            sidebar_collapsed = true;
        }
    }
}
