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

// ── Layout ──────────────────────────────────────────────────────────────────
#define PANEL_W     380
#define TAB_H       32
#define PAD         8
#define ROW_H       22
#define FIELD_H     24
#define BTN_H       28
#define LOG_LINES   8

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
    int line_h = 14;
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
        DrawText(e->text, x + 4, y + 2 + (i - start) * line_h, 11, c);
    }
}

// ── Label + text field helper ───────────────────────────────────────────────
static bool gui_labeled_field(int x, int y, int w, const char *label, char *buf, int buf_size) {
    DrawText(label, x, y, 11, C_DIM);
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
    DrawText("Resource Pack Folder", x + PAD, cy, 11, C_DIM); cy += 14;
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

    DrawText("BBModel File (drag & drop or browse)", x + PAD, cy, 11, C_DIM); cy += 14;
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

    DrawText("Model Name (blank = from filename)", x + PAD, cy, 11, C_DIM); cy += 14;
    Rectangle name_r = {(float)(x+PAD), (float)cy, (float)(w-2*PAD), FIELD_H};
    if (GuiTextBox(name_r, s->model_name, FB_MAX_NAME, import_editing[2])) import_editing[2] = !import_editing[2];
    cy += FIELD_H + PAD;

    DrawText("Author (optional)", x + PAD, cy, 11, C_DIM); cy += 14;
    Rectangle auth_r = {(float)(x+PAD), (float)cy, (float)(w-2*PAD), FIELD_H};
    if (GuiTextBox(auth_r, s->author, FB_MAX_NAME, import_editing[3])) import_editing[3] = !import_editing[3];
    cy += FIELD_H + PAD;

    DrawText("Minecraft Item", x + PAD, cy, 11, C_DIM); cy += 14;
    Rectangle item_r = {(float)(x+PAD), (float)cy, (float)(w-2*PAD), FIELD_H};
    static bool item_edit = false;
    if (GuiTextBox(item_r, s->item_type, FB_MAX_NAME, item_edit)) item_edit = !item_edit;
    cy += FIELD_H + PAD + 8;

    // Import button
    Rectangle btn_r = {(float)(x+PAD), (float)cy, (float)(w-2*PAD), BTN_H};
    if (GuiButton(btn_r, "Add to Pack")) {
        fb_log_clear(&s->log);
        if (!s->pack_path[0]) {
            fb_log(&s->log, FB_LOG_ERROR, "Set a pack path first");
        } else if (!s->bbmodel_path[0]) {
            fb_log(&s->log, FB_LOG_ERROR, "Select a .bbmodel file first");
        } else if (!s->item_type[0]) {
            fb_log(&s->log, FB_LOG_ERROR, "Enter a Minecraft item type");
        } else {
            const char *name = s->model_name[0] ? s->model_name : NULL;
            fb_add_to_pack(s->bbmodel_path, s->pack_path, s->item_type,
                           name, s->author, &s->log);
            // Auto-rescan
            s->pack_entry_count = fb_scan_pack(s->pack_path, s->pack_entries, FB_MAX_MODELS);
            s->pack_scanned = true;
        }
    }
    cy += BTN_H + PAD;

    // Log
    DrawText("Output", x + PAD, cy, 11, C_DIM); cy += 14;
    draw_log(&s->log, x + PAD, cy, w - 2*PAD, h - (cy - y) - PAD);
}

// ═════════════════════════════════════════════════════════════════════════════
// TAB: Manage
// ═════════════════════════════════════════════════════════════════════════════

static bool filter_editing = false;

static bool entry_matches_filter(const FBPackEntry *e, const char *filter) {
    if (!filter[0]) return true;
    if (strcmp(filter, "noauthor") == 0) return e->author[0] == '\0';
    // Case-insensitive substring search
    char haystack[512];
    snprintf(haystack, sizeof(haystack), "%s %s %s %s",
             e->item_type, e->model_name, e->author, e->display_name);
    // Lowercase both
    for (char *p = haystack; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    char needle[FB_MAX_NAME];
    strncpy(needle, filter, sizeof(needle)-1); needle[sizeof(needle)-1] = '\0';
    for (char *p = needle; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return strstr(haystack, needle) != NULL;
}

static void draw_manage_tab(FBAppState *s, int x, int y, int w, int h) {
    int cy = y + PAD;

    // Pack path
    DrawText("Resource Pack Folder", x+PAD, cy, 11, C_DIM); cy += 14;
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
    DrawText("Search (type 'noauthor' to find missing)", x+PAD, cy, 11, C_DIM); cy += 14;
    Rectangle filt_r = {(float)(x+PAD), (float)cy, (float)(w-2*PAD), FIELD_H};
    if (GuiTextBox(filt_r, s->manage_filter, FB_MAX_NAME, filter_editing)) filter_editing = !filter_editing;
    cy += FIELD_H + PAD;

    // Model list header
    int list_x = x + PAD;
    int list_w = w - 2*PAD;
    int col_type = 80, col_name = 130, col_num = 30, col_author = 80, col_status = 40;

    DrawRectangle(list_x, cy, list_w, ROW_H, C_BORDER);
    int cx = list_x + 4;
    DrawText("Type", cx, cy+4, 11, RAYWHITE); cx += col_type;
    DrawText("Model", cx, cy+4, 11, RAYWHITE); cx += col_name;
    DrawText("#", cx, cy+4, 11, RAYWHITE); cx += col_num;
    DrawText("Author", cx, cy+4, 11, RAYWHITE); cx += col_author;
    DrawText("OK", cx, cy+4, 11, RAYWHITE);
    cy += ROW_H;

    // Scrollable model list
    int list_h = h - (cy - y) - 180; // Leave room for buttons + log
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

        // Click to select
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            s->manage_selected = i;
        }

        // Columns
        bool all_ok = e->has_texture && e->has_model && e->has_item_def;
        Color text_c = all_ok ? C_TEXT : C_YELLOW;

        cx = list_x + 4;
        DrawText(e->item_type, cx, ry+4, 10, text_c); cx += col_type;

        // Truncate model name if too long
        char name_buf[24];
        snprintf(name_buf, sizeof(name_buf), "%.22s", e->model_name);
        DrawText(name_buf, cx, ry+4, 10, text_c); cx += col_name;

        DrawText(TextFormat("%d", e->threshold), cx, ry+4, 10, text_c); cx += col_num;
        DrawText(e->author[0] ? e->author : "-", cx, ry+4, 10, e->author[0] ? text_c : C_DIM); cx += col_author;
        DrawText(all_ok ? "OK" : "!!", cx, ry+4, 10, all_ok ? C_GREEN : C_RED);

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

    if (GuiButton((Rectangle){(float)list_x, by, (float)btn_w, BTN_H}, "Delete")) {
        if (s->manage_selected >= 0 && s->manage_selected < s->pack_entry_count) {
            FBPackEntry *e = &s->pack_entries[s->manage_selected];
            fb_log_clear(&s->log);
            fb_log(&s->log, FB_LOG_HEADER, "Deleting %s...", e->model_name);
            fb_delete_model(s->pack_path, e->item_type, e->model_name, e->threshold, &s->log);
            s->pack_entry_count = fb_scan_pack(s->pack_path, s->pack_entries, FB_MAX_MODELS);
            s->manage_selected = -1;
        }
    }

    if (GuiButton((Rectangle){(float)(list_x + btn_w + PAD), by, (float)btn_w, BTN_H}, "Preview")) {
        if (s->manage_selected >= 0) {
            FBPackEntry *e = &s->pack_entries[s->manage_selected];
            // Load model from pack for preview
            char model_path[FB_MAX_PATH];
            snprintf(model_path, sizeof(model_path), "%s/%s/%s.json",
                     s->pack_path, FB_MODEL_DIR, e->model_name);
            if (fb_file_exists(model_path)) {
                if (s->preview_loaded) fb_unload_model_textures(&s->preview_model);
                // For preview from pack, we'd need to load the JSON model + texture files
                // For now, just log it
                fb_log_clear(&s->log);
                fb_log(&s->log, FB_LOG_INFO, "Preview: load the .bbmodel in the Preview tab");
                s->active_tab = TAB_PREVIEW;
            }
        }
    }

    if (GuiButton((Rectangle){(float)(list_x + 2*(btn_w+PAD)), by, (float)btn_w, BTN_H}, "Duplicate")) {
        if (s->manage_selected >= 0) {
            FBPackEntry *e = &s->pack_entries[s->manage_selected];
            fb_log_clear(&s->log);
            // For now, duplicate to item_type field
            if (s->item_type[0]) {
                fb_duplicate_to_item(s->pack_path, e->model_name, s->item_type, e->author, &s->log);
                s->pack_entry_count = fb_scan_pack(s->pack_path, s->pack_entries, FB_MAX_MODELS);
            } else {
                fb_log(&s->log, FB_LOG_WARN, "Enter target item in Import tab's 'Minecraft Item' field");
            }
        }
    }

    if (GuiButton((Rectangle){(float)(list_x + 3*(btn_w+PAD)), by, (float)btn_w, BTN_H}, "Set Author")) {
        if (s->manage_selected >= 0) {
            FBPackEntry *e = &s->pack_entries[s->manage_selected];
            fb_log_clear(&s->log);
            fb_update_author(s->pack_path, e->item_type, e->threshold,
                             e->display_name, s->author, &s->log);
            // Update local data
            strncpy(e->author, s->author, FB_MAX_NAME - 1);
        }
    }
    cy += BTN_H + PAD;

    // Log
    DrawText("Output", x + PAD, cy, 11, C_DIM); cy += 14;
    draw_log(&s->log, x + PAD, cy, w - 2*PAD, h - (cy - y) - PAD);
}

// ═════════════════════════════════════════════════════════════════════════════
// TAB: Preview (3D viewport)
// ═════════════════════════════════════════════════════════════════════════════

static void draw_preview_sidebar(FBAppState *s, int x, int y, int w, int h) {
    int cy = y + PAD;

    DrawText("Drag & drop a .bbmodel file", x+PAD, cy, 12, C_DIM); cy += 16;
    DrawText("onto this window to preview", x+PAD, cy, 12, C_DIM); cy += 24;

    if (s->preview_loaded) {
        DrawLine(x, cy, x+w, cy, C_BORDER); cy += PAD;
        DrawText("Loaded Model:", x+PAD, cy, 13, C_GREEN); cy += 18;
        DrawText(TextFormat("Name: %s", s->preview_model.name), x+PAD+4, cy, 11, C_TEXT); cy += 14;
        DrawText(TextFormat("Elements: %d", s->preview_model.element_count), x+PAD+4, cy, 11, C_TEXT); cy += 14;
        DrawText(TextFormat("Texture: %dx%d", s->preview_model.texture_size[0], s->preview_model.texture_size[1]),
                 x+PAD+4, cy, 11, C_TEXT); cy += 14;
        DrawText(TextFormat("Textures: %d", s->preview_model.texture_count), x+PAD+4, cy, 11, C_TEXT); cy += 20;

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
    DrawText("Controls:", x+PAD, cy, 12, C_TEXT); cy += 16;
    DrawText("  Left drag  - Rotate", x+PAD, cy, 10, C_DIM); cy += 12;
    DrawText("  Scroll     - Zoom", x+PAD, cy, 10, C_DIM); cy += 12;
    DrawText("  Mid drag   - Pan", x+PAD, cy, 10, C_DIM); cy += 12;
    DrawText("  R - Reset   G - Grid", x+PAD, cy, 10, C_DIM);
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
// Main GUI
// ═════════════════════════════════════════════════════════════════════════════

void fb_draw_gui(FBAppState *s) {
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
    }

    // ── Compute viewport and panel sizes ───────────────────────────────
    int vp_x, vp_w, panel_w;
    if (s->active_tab == TAB_PREVIEW) {
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
    const char *tab_names[] = {"Import", "Manage", "Preview"};
    int tab_count = 3;
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
        }
    }
    DrawLine(0, TAB_H, panel_w, TAB_H, C_BORDER);

    // ── Left panel content ──────────────────────────────────────────────
    int panel_y = TAB_H;
    int panel_h = sh - TAB_H;

    DrawRectangle(0, panel_y, panel_w, panel_h, C_PANEL);

    switch (s->active_tab) {
        case TAB_IMPORT:
            draw_import_tab(s, 0, panel_y, panel_w, panel_h);
            break;
        case TAB_MANAGE:
            draw_manage_tab(s, 0, panel_y, panel_w, panel_h);
            break;
        case TAB_PREVIEW:
            draw_preview_sidebar(s, 0, panel_y, panel_w, panel_h);
            break;
    }

    if (vp_w > 0) {
        update_orbital_camera(&s->camera, vp_x);
        draw_preview_viewport(s, vp_x, 0, vp_w, sh);
    }

    // Panel border
    DrawLine(PANEL_W, 0, PANEL_W, sh, C_BORDER);
    if (s->active_tab != TAB_PREVIEW && vp_w > 0) {
        DrawLine(vp_x, 0, vp_x, sh, C_BORDER);
    }
}
