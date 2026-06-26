#include "filebrowser.h"
#include "raylib.h"
#include "raygui.h"

#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define FB_BR_MAX_ENTRIES 512
#define FB_BR_MAX_PATH    1024
#define FB_BR_ENTRY_H     24
#define FB_BR_PAD         8

typedef struct {
    char name[256];
    bool is_dir;
} BrEntry;

static struct {
    bool        open;
    FBBrowseMode mode;
    char        current_dir[FB_BR_MAX_PATH];
    char        selected_path[FB_BR_MAX_PATH];
    BrEntry     entries[FB_BR_MAX_ENTRIES];
    int         entry_count;
    int         scroll_offset;
    bool        done;           // selection was made
    const char **extensions;
    int         ext_count;
    char        path_input[FB_BR_MAX_PATH];
    bool        path_editing;
} state = {0};

static int entry_cmp(const void *a, const void *b) {
    const BrEntry *ea = (const BrEntry *)a;
    const BrEntry *eb = (const BrEntry *)b;
    // Dirs first, then alphabetical
    if (ea->is_dir != eb->is_dir) return ea->is_dir ? -1 : 1;
    return strcasecmp(ea->name, eb->name);
}

static bool matches_extension(const char *name) {
    if (state.mode == FB_BROWSE_FOLDER) return false; // only show dirs in folder mode
    if (state.ext_count == 0) return true; // no filter
    int len = (int)strlen(name);
    for (int i = 0; i < state.ext_count; i++) {
        const char *ext = state.extensions[i];
        int elen = (int)strlen(ext);
        // ext is like "*.bbmodel" — skip the "*"
        const char *suffix = ext;
        if (suffix[0] == '*') suffix++;
        int slen = (int)strlen(suffix);
        if (len >= slen && strcasecmp(name + len - slen, suffix) == 0) return true;
    }
    return false;
}

static void scan_dir(void) {
    state.entry_count = 0;
    state.scroll_offset = 0;

    DIR *dir = opendir(state.current_dir);
    if (!dir) return;

    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (state.entry_count >= FB_BR_MAX_ENTRIES) break;
        if (strcmp(de->d_name, ".") == 0) continue;

        // Build full path for stat
        char full[FB_BR_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", state.current_dir, de->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;

        bool is_dir = S_ISDIR(st.st_mode);

        // Skip hidden files (except "..")
        if (de->d_name[0] == '.' && strcmp(de->d_name, "..") != 0) continue;

        // For file mode: show dirs + matching files
        // For folder mode: show dirs only
        if (!is_dir && !matches_extension(de->d_name)) continue;

        BrEntry *e = &state.entries[state.entry_count++];
        strncpy(e->name, de->d_name, sizeof(e->name) - 1);
        e->name[sizeof(e->name) - 1] = '\0';
        e->is_dir = is_dir;
    }
    closedir(dir);

    qsort(state.entries, state.entry_count, sizeof(BrEntry), entry_cmp);
    strncpy(state.path_input, state.current_dir, FB_BR_MAX_PATH - 1);
}

void fb_browser_open(FBBrowseMode mode, const char *start_dir,
                     const char **extensions, int ext_count) {
    memset(&state, 0, sizeof(state));
    state.open = true;
    state.mode = mode;
    state.extensions = extensions;
    state.ext_count = ext_count;

    if (start_dir && start_dir[0]) {
        strncpy(state.current_dir, start_dir, FB_BR_MAX_PATH - 1);
    } else {
        getcwd(state.current_dir, FB_BR_MAX_PATH);
    }

    scan_dir();
}

bool fb_browser_is_open(void) { return state.open; }

bool fb_browser_draw(void) {
    if (!state.open) return false;

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int w = sw * 3 / 4;
    if (w < 500) w = 500;
    if (w > sw - 20) w = sw - 20;
    int h = sh * 3 / 4;
    if (h < 400) h = 400;
    if (h > sh - 20) h = sh - 20;
    int x = (sw - w) / 2, y = (sh - h) / 2;

    // Dim background
    DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 120});

    // Panel
    DrawRectangleRounded((Rectangle){(float)x, (float)y, (float)w, (float)h}, 0.02f, 4, (Color){35, 35, 40, 245});
    DrawRectangleRoundedLines((Rectangle){(float)x, (float)y, (float)w, (float)h}, 0.02f, 4, (Color){80, 80, 90, 255});

    int cx = x + FB_BR_PAD, cy = y + FB_BR_PAD;
    int inner_w = w - 2 * FB_BR_PAD;

    // Title
    const char *title = state.mode == FB_BROWSE_FOLDER ? "Select Folder" : "Select File";
    DrawText(title, cx, cy, 18, WHITE);
    cy += 24;

    // Path input bar
    Rectangle path_r = {(float)cx, (float)cy, (float)(inner_w - 60 - FB_BR_PAD), 26};
    if (GuiTextBox(path_r, state.path_input, FB_BR_MAX_PATH, state.path_editing))
        state.path_editing = !state.path_editing;

    Rectangle go_r = {(float)(cx + inner_w - 60), (float)cy, 60, 26};
    if (GuiButton(go_r, "Go")) {
        struct stat st;
        if (stat(state.path_input, &st) == 0 && S_ISDIR(st.st_mode)) {
            strncpy(state.current_dir, state.path_input, FB_BR_MAX_PATH - 1);
            scan_dir();
        }
    }
    cy += 32;

    // File list area
    int list_h = h - (cy - y) - 40 - FB_BR_PAD; // leave room for bottom buttons
    Rectangle list_area = {(float)cx, (float)cy, (float)inner_w, (float)list_h};
    DrawRectangleRec(list_area, (Color){25, 25, 30, 255});

    // Scroll with mouse wheel
    if (CheckCollisionPointRec(GetMousePosition(), list_area)) {
        state.scroll_offset -= (int)(GetMouseWheelMove() * 3);
        if (state.scroll_offset < 0) state.scroll_offset = 0;
        int max_scroll = state.entry_count - (list_h / FB_BR_ENTRY_H);
        if (max_scroll < 0) max_scroll = 0;
        if (state.scroll_offset > max_scroll) state.scroll_offset = max_scroll;
    }

    // Draw entries
    int visible = list_h / FB_BR_ENTRY_H;
    BeginScissorMode((int)list_area.x, (int)list_area.y, (int)list_area.width, (int)list_area.height);
    for (int i = 0; i < visible && (i + state.scroll_offset) < state.entry_count; i++) {
        int idx = i + state.scroll_offset;
        BrEntry *e = &state.entries[idx];
        int ey = cy + i * FB_BR_ENTRY_H;
        Rectangle row = {(float)cx, (float)ey, (float)inner_w, FB_BR_ENTRY_H};

        // Hover highlight
        bool hovered = CheckCollisionPointRec(GetMousePosition(), row);
        if (hovered) {
            DrawRectangleRec(row, (Color){60, 60, 70, 255});
        }

        // Icon + name
        Color col = e->is_dir ? (Color){100, 200, 255, 255} : (Color){220, 220, 220, 255};
        const char *prefix = e->is_dir ? "📁 " : "   ";
        DrawText(TextFormat("%s%s", prefix, e->name), cx + 4, ey + 4, 14, col);

        // Click handler
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (e->is_dir) {
                // Navigate into directory
                if (strcmp(e->name, "..") == 0) {
                    // Go up
                    char *last = strrchr(state.current_dir, '/');
                    if (last && last != state.current_dir) {
                        *last = '\0';
                    } else if (last) {
                        state.current_dir[1] = '\0'; // root
                    }
                } else {
                    int len = (int)strlen(state.current_dir);
                    if (len > 1) {
                        snprintf(state.current_dir + len, FB_BR_MAX_PATH - len, "/%s", e->name);
                    } else {
                        snprintf(state.current_dir + len, FB_BR_MAX_PATH - len, "%s", e->name);
                    }
                }
                scan_dir();
            } else {
                // Select file
                snprintf(state.selected_path, FB_BR_MAX_PATH, "%s/%s",
                         state.current_dir, e->name);
                state.done = true;
                state.open = false;
            }
        }
    }
    EndScissorMode();
    cy += list_h + FB_BR_PAD;

    // Bottom buttons
    float btn_w = 100;
    if (state.mode == FB_BROWSE_FOLDER) {
        // "Select This Folder" button
        Rectangle sel_r = {(float)cx, (float)cy, 150, 28};
        if (GuiButton(sel_r, "Select This Folder")) {
            strncpy(state.selected_path, state.current_dir, FB_BR_MAX_PATH - 1);
            state.done = true;
            state.open = false;
        }
    }

    Rectangle cancel_r = {(float)(cx + inner_w - btn_w), (float)cy, btn_w, 28};
    if (GuiButton(cancel_r, "Cancel") || IsKeyPressed(KEY_ESCAPE)) {
        state.open = false;
        state.done = false;
    }

    if (state.done) return true;
    return false;
}

const char *fb_browser_result(void) {
    return state.selected_path;
}

void fb_browser_close(void) {
    state.open = false;
    state.done = false;
}
