#include "filedialog.h"
#include "tinyfiledialogs.h"
#include <string.h>

bool fb_pick_folder(char *out_path, int out_size) {
    const char *result = tinyfd_selectFolderDialog("Select Resource Pack Folder", "");
    if (!result || !result[0]) return false;
    strncpy(out_path, result, out_size - 1);
    out_path[out_size - 1] = '\0';
    return true;
}

bool fb_pick_bbmodel(char *out_path, int out_size) {
    const char *filters[] = {"*.bbmodel"};
    const char *result = tinyfd_openFileDialog(
        "Select BBModel File", "", 1, filters, "Blockbench Models (*.bbmodel)", 0);
    if (!result || !result[0]) return false;
    strncpy(out_path, result, out_size - 1);
    out_path[out_size - 1] = '\0';
    return true;
}
