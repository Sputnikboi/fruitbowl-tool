#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir_p(path) _mkdir(path)
#else
#define mkdir_p(path) mkdir(path, 0755)
#endif

void fb_sanitize_name(const char *input, char *output, int max_len) {
    // Strip path and extension
    const char *base = strrchr(input, '/');
    if (!base) base = strrchr(input, '\\');
    base = base ? base + 1 : input;

    const char *dot = strrchr(base, '.');
    int base_len = dot ? (int)(dot - base) : (int)strlen(base);

    int j = 0;
    for (int i = 0; i < base_len && j < max_len - 1; i++) {
        char c = base[i];
        if (c >= 'A' && c <= 'Z') {
            output[j++] = c - 'A' + 'a';
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            output[j++] = c;
        } else {
            // Replace special chars with underscore, avoid doubles
            if (j > 0 && output[j - 1] != '_') {
                output[j++] = '_';
            }
        }
    }
    // Trim trailing underscores
    while (j > 0 && output[j - 1] == '_') j--;
    // Trim leading underscores
    int start = 0;
    while (start < j && output[start] == '_') start++;
    if (start > 0) {
        memmove(output, output + start, j - start);
        j -= start;
    }
    output[j] = '\0';
}

void fb_display_name(const char *model_name, char *output, int max_len) {
    int j = 0;
    bool cap_next = true;
    for (int i = 0; model_name[i] && j < max_len - 1; i++) {
        if (model_name[i] == '_') {
            output[j++] = ' ';
            cap_next = true;
        } else if (cap_next) {
            output[j++] = toupper(model_name[i]);
            cap_next = false;
        } else {
            output[j++] = model_name[i];
        }
    }
    output[j] = '\0';
}

char *fb_read_file(const char *path, int *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    int len = (int)ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    if (out_len) *out_len = len;
    return buf;
}

bool fb_write_file(const char *path, const char *data, int len) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    fwrite(data, 1, len, f);
    fclose(f);
    return true;
}

bool fb_file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

void fb_path_join(char *out, int max_len, const char *a, const char *b) {
    snprintf(out, max_len, "%s/%s", a, b);
    // Normalize separators
    for (char *p = out; *p; p++) {
        if (*p == '\\') *p = '/';
    }
}

void fb_ensure_dir(const char *path) {
    char tmp[FB_MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            mkdir_p(tmp);
            *p = '/';
        }
    }
    mkdir_p(tmp);
}

void fb_log(FBLog *log, FBLogLevel level, const char *fmt, ...) {
    if (log->count >= FB_MAX_LOG) {
        // Shift everything up to make room
        memmove(&log->entries[0], &log->entries[1],
                sizeof(FBLogEntry) * (FB_MAX_LOG - 1));
        log->count = FB_MAX_LOG - 1;
    }
    FBLogEntry *e = &log->entries[log->count++];
    e->level = level;
    va_list args;
    va_start(args, fmt);
    vsnprintf(e->text, sizeof(e->text), fmt, args);
    va_end(args);
}

void fb_log_clear(FBLog *log) {
    log->count = 0;
}
