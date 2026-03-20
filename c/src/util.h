#ifndef FB_UTIL_H
#define FB_UTIL_H

#include "types.h"

// String helpers
void fb_sanitize_name(const char *input, char *output, int max_len);
void fb_display_name(const char *model_name, char *output, int max_len);
char *fb_read_file(const char *path, int *out_len);
bool fb_write_file(const char *path, const char *data, int len);
bool fb_file_exists(const char *path);
void fb_path_join(char *out, int max_len, const char *a, const char *b);
void fb_ensure_dir(const char *path);

// Logging
void fb_log(FBLog *log, FBLogLevel level, const char *fmt, ...);
void fb_log_clear(FBLog *log);

#endif // FB_UTIL_H
