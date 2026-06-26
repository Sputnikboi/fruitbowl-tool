#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <stdbool.h>

// In-app file browser (non-blocking, raygui-based).
// 1. Call fb_open_folder_dialog(id) or fb_open_file_dialog(id) to start.
// 2. Call fb_dialog_poll() each frame to draw the browser and check for results.
// 3. When poll returns true, out_path has the result and *id_out tells which button.

// Dialog IDs (arbitrary ints to identify which Browse button was pressed)
enum {
    DIALOG_IMPORT_PACK   = 1,  // Import tab: pack path browse
    DIALOG_IMPORT_FILE   = 2,  // Import tab: bbmodel file browse
    DIALOG_MANAGE_PACK   = 3,  // Manage tab: pack path browse
    DIALOG_UPDATE_FILE   = 4,  // Manage tab: update model file
    DIALOG_BATCH_PACK    = 5,  // Batch tab: pack path browse
    DIALOG_BATCH_FILE    = 6,  // Batch tab: add files
    DIALOG_BATCH_FOLDER  = 7,  // Batch tab: add folder
};

bool fb_dialog_just_closed(void);

// Open dialogs (non-blocking)
void fb_open_folder_dialog(int id, const char *start_dir);
void fb_open_file_dialog(int id, const char *start_dir);

// Legacy (unused but kept for compat)
bool fb_pick_folder(char *out_path, int out_size);
bool fb_pick_bbmodel(char *out_path, int out_size);

// Poll each frame. Draws browser overlay. Returns true when selection is made.
bool fb_dialog_poll(char *out_path, int out_size, int *id_out);
bool fb_dialog_is_open(void);

#endif
