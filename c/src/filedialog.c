#include "filedialog.h"
#include <stddef.h>
#include "tinyfiledialogs.h"
#include "raylib.h"
#include <string.h>

// Native file dialogs block the main loop. When they return, raylib still
// sees the mouse button as pressed from the click that opened the dialog,
// causing buttons underneath to fire a second time. Polling one frame of
// input after the dialog closes flushes that stale state.
static void flush_input(void) {
    PollInputEvents();
}

bool fb_pick_folder(char *out_path, int out_size) {
    const char *result = tinyfd_selectFolderDialog("Select Resource Pack Folder", "");
    flush_input();
    if (!result || !result[0]) return false;
    strncpy(out_path, result, out_size - 1);
    out_path[out_size - 1] = '\0';
    return true;
}

bool fb_pick_bbmodel(char *out_path, int out_size) {
    const char *filters[] = {"*.bbmodel"};
    const char *result = tinyfd_openFileDialog(
        "Select BBModel File", "", 1, filters, "Blockbench Models (*.bbmodel)", 0);
    flush_input();
    if (!result || !result[0]) return false;
    strncpy(out_path, result, out_size - 1);
    out_path[out_size - 1] = '\0';
    return true;
}
