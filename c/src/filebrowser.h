#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#include <stdbool.h>

// Lightweight in-app file/folder browser using raygui.
// Call fb_browser_open() to start, fb_browser_draw() each frame,
// and check fb_browser_result() when done.

typedef enum {
    FB_BROWSE_FILE,
    FB_BROWSE_FOLDER,
} FBBrowseMode;

void fb_browser_open(FBBrowseMode mode, const char *start_dir,
                     const char **extensions, int ext_count);
bool fb_browser_is_open(void);
// Draw the browser overlay. Returns true if a selection was made.
bool fb_browser_draw(void);
// Get the selected path (valid after fb_browser_draw returns true).
const char *fb_browser_result(void);
void fb_browser_close(void);

#endif
