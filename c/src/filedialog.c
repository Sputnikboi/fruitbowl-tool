#include "filedialog.h"
#include <stddef.h>
#include "tinyfiledialogs.h"
#include "raylib.h"
#include <string.h>

// Native file dialogs block the main loop. When they return, raylib still
// sees the mouse button as pressed from the click that opened the dialog,
// causing buttons underneath to fire a second time.
//
// We used to call PollInputEvents() here, but that can crash when called
// mid-frame (between BeginDrawing/EndDrawing) because glfwPollEvents may
// trigger window callbacks (resize, close, focus) that corrupt raylib state.
//
// Instead we set a cooldown flag that the GUI checks to suppress stale clicks.

static double dialog_close_time = 0.0;

bool fb_dialog_just_closed(void) {
    return (GetTime() - dialog_close_time) < 0.15;
}

bool fb_pick_folder(char *out_path, int out_size) {
    const char *result = tinyfd_selectFolderDialog("Select Resource Pack Folder", "");
    dialog_close_time = GetTime();
    if (!result || !result[0]) return false;
    strncpy(out_path, result, out_size - 1);
    out_path[out_size - 1] = '\0';
    return true;
}

bool fb_pick_bbmodel(char *out_path, int out_size) {
    const char *filters[] = {"*.bbmodel"};
    const char *result = tinyfd_openFileDialog(
        "Select BBModel File", "", 1, filters, "Blockbench Models (*.bbmodel)", 0);
    dialog_close_time = GetTime();
    if (!result || !result[0]) return false;
    strncpy(out_path, result, out_size - 1);
    out_path[out_size - 1] = '\0';
    return true;
}
