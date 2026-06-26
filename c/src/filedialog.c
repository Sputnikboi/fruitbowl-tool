#include "filedialog.h"
#include "filebrowser.h"
#include <string.h>

static int  pending_id = 0;     // which browse button opened the dialog
static char pending_result[1024];
static bool pending_ready = false;

static const char *file_exts[] = {"*.bbmodel", "*.png"};

bool fb_dialog_just_closed(void) {
    return false; // no longer needed with in-app browser
}

void fb_open_folder_dialog(int id, const char *start_dir) {
    fb_browser_open(FB_BROWSE_FOLDER, start_dir, NULL, 0);
    pending_id = id;
    pending_ready = false;
}

void fb_open_file_dialog(int id, const char *start_dir) {
    fb_browser_open(FB_BROWSE_FILE, start_dir, file_exts, 2);
    pending_id = id;
    pending_ready = false;
}

// Legacy wrappers — now just open the browser and return false
bool fb_pick_folder(char *out_path, int out_size) {
    (void)out_path; (void)out_size;
    return false;
}

bool fb_pick_bbmodel(char *out_path, int out_size) {
    (void)out_path; (void)out_size;
    return false;
}

// Call each frame. Returns true when a result is ready.
// id_out receives which button triggered it, result goes to out_path.
bool fb_dialog_poll(char *out_path, int out_size, int *id_out) {
    if (!fb_browser_is_open()) {
        if (pending_ready) {
            pending_ready = false;
            if (id_out) *id_out = pending_id;
            strncpy(out_path, pending_result, out_size - 1);
            out_path[out_size - 1] = '\0';
            pending_id = 0;
            return true;
        }
        return false;
    }

    if (fb_browser_draw()) {
        const char *result = fb_browser_result();
        if (result && result[0]) {
            strncpy(pending_result, result, sizeof(pending_result) - 1);
            pending_ready = true;
        }
        fb_browser_close();
    }
    return false;
}

bool fb_dialog_is_open(void) {
    return fb_browser_is_open();
}
