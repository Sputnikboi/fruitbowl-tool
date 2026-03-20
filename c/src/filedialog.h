#ifndef FB_FILEDIALOG_H
#define FB_FILEDIALOG_H

#include <stdbool.h>

// Open a native folder picker dialog. Returns true if a folder was selected.
// Result is written to out_path (max out_size bytes).
// Uses zenity on Linux, powershell on Windows.
bool fb_pick_folder(char *out_path, int out_size);

// Open a native file picker for .bbmodel files.
bool fb_pick_bbmodel(char *out_path, int out_size);

#endif
